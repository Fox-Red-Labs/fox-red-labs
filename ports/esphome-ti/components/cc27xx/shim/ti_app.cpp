// TI F3 shim: replaces TI's on_off_light.c. TI's osif/ti_f3_main.c owns main() and the pthread that calls
// zb_ti_f3_main(); this file is what runs there. ZBOSS runs on that thread (as in TI's example);
// ESPHome's setup() runs on it first, then loop() on a second FreeRTOS task.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <ti/log/Log.h>
#include "ti_zigbee_config.h"
#include "zboss_api.h"
#include "zb_hal_utils.h"
void ti_fcfg_mac(uint8_t out[8]);
#include "zigbee/zigbee_app_utils.h"
#include "zb_error_to_string.h"
#ifdef ZB_CONFIGURABLE_MEM
#include "zb_mem_config_lprf3.h"  /* the memory sizing TI's example includes; macros only */
#endif
}

void setup();
void loop();

// ---- heap: C malloc is already routed to FreeRTOS heap_4 by TI's configuration; C++ new/delete follow it ----
void *operator new(size_t n) { return pvPortMalloc(n); }
void *operator new[](size_t n) { return pvPortMalloc(n); }
void *operator new(size_t n, std::align_val_t) { return pvPortMalloc(n); }
void operator delete(void *p) noexcept { vPortFree(p); }
void operator delete[](void *p) noexcept { vPortFree(p); }
void operator delete(void *p, size_t) noexcept { vPortFree(p); }
void operator delete[](void *p, size_t) noexcept { vPortFree(p); }
void operator delete(void *p, std::align_val_t) noexcept { vPortFree(p); }

// ---- Nordic zigbee_app_utils calls, on TI's ZBOSS ----
extern "C" {

const char *zb_error_to_string_get(int err) { (void) err; return "zb_err"; }

int ieee_addr_to_str(char *str_buf, int buf_len, const zb_ieee_addr_t addr) {
  return snprintf(str_buf, buf_len, "%02x%02x%02x%02x%02x%02x%02x%02x", addr[7], addr[6], addr[5], addr[4], addr[3],
                  addr[2], addr[1], addr[0]);
}

void zigbee_configure_sleepy_behavior(zb_bool_t enable) { (void) enable; }

static zb_bool_t started = ZB_FALSE;

static void restart_commissioning(zb_uint8_t param) {
  (void) param;
  bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
}

// Called by ESPHome's ZigbeeComponent after it has run the app's signal handling; mirrors TI's zboss_signal_handler.
zb_ret_t zigbee_default_signal_handler(zb_bufid_t bufid) {
  zb_zdo_app_signal_hdr_t *hdr = nullptr;
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(bufid, &hdr);
  zb_ret_t status = ZB_GET_APP_SIGNAL_STATUS(bufid);
  if (status == RET_OK) {
    switch (sig) {
      case ZB_ZDO_SIGNAL_SKIP_STARTUP:
        zboss_start_continue();
        break;
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        break;
      default:
        break;
    }
  } else {
    switch (sig) {
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      case ZB_BDB_SIGNAL_STEERING:
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break;
      default:
        break;
    }
  }
  return RET_OK;
}

void zigbee_enable(void) {
  if (zboss_start_no_autostart() == RET_OK) started = ZB_TRUE;
}

// link-test stubs for the radio-sleep accounting ESPHome wraps (no zb_trans_* in TI's prebuilt libraries)
void __real_zb_trans_enter_sleep(void) {}
void __real_zb_trans_enter_receive(void) {}
zb_bool_t __real_zb_trans_transmit(zb_uint8_t wait_type, zb_time_t tx_at, zb_uint8_t *tx_buf, zb_uint8_t ch) {
  (void) wait_type; (void) tx_at; (void) tx_buf; (void) ch;
  return ZB_FALSE;
}

static void esphome_task(void *arg) {
  (void) arg;
  for (;;) loop();
}

MAIN() {
  ARGV_UNUSED;
  ZB_INIT("esphome_fit");
  zb_ieee_addr_t ieee_mac_addr;
  ti_fcfg_mac(ieee_mac_addr);
  zb_set_long_address(ieee_mac_addr);
#if defined ZB_ED_ROLE
  zb_set_network_ed_role(ZB_TRANSCEIVER_ALL_CHANNELS_MASK);  // all channels, not TI's channel 11 only
  zb_set_ed_timeout(ED_TIMEOUT_VALUE);
  zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);
#if (ED_RX_ALWAYS_ON == ZB_FALSE)
  zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ED_POLL_RATE));
  zb_zdo_pim_set_long_poll_interval(ED_POLL_RATE);
#endif
#else
  zb_set_network_router_role(ZB_TRANSCEIVER_ALL_CHANNELS_MASK);  // all channels, not TI's channel 11 only
#endif
  zb_set_nvram_erase_at_start(ZB_FALSE);
  setup();  // ESPHome: registers the device context, callbacks, calls zigbee_enable()
  if (!started) MAIN_RETURN(1);
  xTaskCreate(esphome_task, "esphome", 512, nullptr, 1, nullptr);
  zboss_main_loop();
  MAIN_RETURN(0);
}
}
