/*
 * Zigbee smoke and CO sensor for the LP-EM-CC2340R5 (Fox Red Labs, run Z).
 * Built on TI's onoff_light skeleton (F3 SDK 9.21.00.36, ZBOSS). Two endpoints, each an IAS Zone server:
 *   endpoint 1: zone type 0x0028 Fire Sensor   (UART lines "SMOKE 1" / "SMOKE 0")
 *   endpoint 2: zone type 0x002B CO sensor     (UART lines "CO 1" / "CO 0")
 * ZoneStatus bit alarm1 is set on 1 and cleared on 0; a Zone Status Change Notification goes to the enrolled CIE.
 * Console: UART2 on the XDS110 back channel, 115200 8N1, newline terminated.
 * Parts of this file follow TI's on_off_light.c (BSD-3, Texas Instruments Incorporated, 2024-2026).
 */
#define ZB_TRACE_FILE_ID 40124

#include "ti_zigbee_config.h"
#include "zboss_api.h"
#include "zb_led_button.h"
#include "zboss_api_error.h"

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)

#include <ti/drivers/GPIO.h>
#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/SemaphoreP.h>
#include "ti_drivers_config.h"

#ifdef ZB_CONFIGURABLE_MEM
#include "zb_mem_config_lprf3.h"
#endif

#define EP_SMOKE   1
#define EP_CO      2

extern SemaphoreP_Handle wakeSem;

static zb_bool_t perform_factory_reset = ZB_FALSE;

/* ---- ZCL attributes, one set per endpoint ---- */
#define DECL_BASIC_VARS(n) \
  static zb_uint8_t b_zcl_##n = ZB_ZCL_BASIC_ZCL_VERSION_DEFAULT_VALUE; \
  static zb_uint8_t b_app_##n = 1, b_stk_##n = 2, b_hw_##n = 1; \
  static zb_char_t  b_man_##n[33] = {12, 'F','o','x',' ','R','e','d',' ','L','a','b','s'}; \
  static zb_char_t  b_mod_##n[33] = {9, 'F','R','L','-','A','L','A','R','M'}; \
  static zb_char_t  b_date_##n[17] = {0}; \
  static zb_uint8_t b_pwr_##n = ZB_ZCL_BASIC_POWER_SOURCE_BATTERY; \
  static zb_char_t  b_loc_##n[17] = {0}; \
  static zb_uint8_t b_env_##n = 0; \
  static zb_char_t  b_sw_##n[17] = {0}; \
  static zb_uint16_t idt_##n = 0; \
  static zb_uint8_t  zstate_##n = ZB_ZCL_IAS_ZONE_ZONESTATE_NOT_ENROLLED; \
  static zb_uint16_t zstatus_##n = 0; \
  static zb_ieee_addr_t cie_##n; \
  static zb_uint16_t cie_short_##n; \
  static zb_uint8_t  cie_ep_##n; \
  static zb_uint8_t  zid_##n = 0xff, zlev_##n = 2, zcur_##n = 0;

DECL_BASIC_VARS(a)
DECL_BASIC_VARS(b)
static zb_uint16_t ztype_a = ZB_ZCL_IAS_ZONE_ZONETYPE_FIRE_SENSOR;
static zb_uint16_t ztype_b = 0x002B; /* ZCL "CO sensor"; ZBOSS names the same value GAS_SENSOR */

ZB_DECLARE_SIMPLE_DESC(3, 0);

#define DECL_EP(n, ep, ztype) \
  ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT(basic_al_##n, &b_zcl_##n, &b_app_##n, &b_stk_##n, &b_hw_##n, \
      b_man_##n, b_mod_##n, b_date_##n, &b_pwr_##n, b_loc_##n, &b_env_##n, b_sw_##n); \
  ZB_ZCL_DECLARE_IDENTIFY_ATTRIB_LIST(ident_al_##n, &idt_##n); \
  ZB_ZCL_DECLARE_IAS_ZONE_ATTRIB_LIST_EXT(zone_al_##n, &zstate_##n, &ztype, &zstatus_##n, &zlev_##n, &zcur_##n, \
      cie_##n, &zid_##n, &cie_short_##n, &cie_ep_##n); \
  zb_zcl_cluster_desc_t clusters_##n[] = { \
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_ARRAY_SIZE(basic_al_##n, zb_zcl_attr_t), basic_al_##n, \
                        ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_ARRAY_SIZE(ident_al_##n, zb_zcl_attr_t), ident_al_##n, \
                        ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID), \
    ZB_ZCL_CLUSTER_DESC(ZB_ZCL_CLUSTER_ID_IAS_ZONE, ZB_ZCL_ARRAY_SIZE(zone_al_##n, zb_zcl_attr_t), zone_al_##n, \
                        ZB_ZCL_CLUSTER_SERVER_ROLE, ZB_ZCL_MANUF_CODE_INVALID) }; \
  ZB_AF_SIMPLE_DESC_TYPE(3, 0) simple_desc_ep_##n = { ep, ZB_AF_HA_PROFILE_ID, ZB_HA_IAS_ZONE_ID, 0, 0, 3, 0, \
      { ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_IAS_ZONE } }; \
  ZBOSS_DEVICE_DECLARE_REPORTING_CTX(rep_##n, ZB_ZCL_IAS_ZONE_REPORT_ATTR_COUNT); \
  ZB_AF_DECLARE_ENDPOINT_DESC(ep_##n, ep, ZB_AF_HA_PROFILE_ID, 0, NULL, \
      ZB_ZCL_ARRAY_SIZE(clusters_##n, zb_zcl_cluster_desc_t), clusters_##n, \
      (zb_af_simple_desc_1_1_t *)&simple_desc_ep_##n, ZB_ZCL_IAS_ZONE_REPORT_ATTR_COUNT, rep_##n, 0, NULL)

DECL_EP(a, EP_SMOKE, ztype_a);
DECL_EP(b, EP_CO, ztype_b);
ZBOSS_DECLARE_DEVICE_CTX_2_EP(alarm_ctx, ep_a, ep_b);

/* ---- UART lines to alarm events ---- */
static volatile zb_uint8_t want[2];   /* 0 smoke, 1 CO: last requested state */
static volatile zb_uint8_t dirty;     /* bit0 smoke, bit1 CO */
static UART2_Handle uart;
static zb_uint8_t rxb;
static char line[12];
static zb_uint8_t llen;

static void apply_cb(zb_uint8_t param);

/* Status lines back to the listener ("ZB sig=<n> st=<n>"): blocking write, a few bytes. */
#ifdef FRL_DEBUG_UART
static void dbg(const char *tag, zb_uint16_t a, zb_uint16_t b)
{
  char m[40]; zb_uint8_t n = 0; const char *t = tag;
  if (!uart) { return; }
  while (*t && n < 8) { m[n++] = *t++; }
  m[n++] = ' ';
  { zb_uint16_t v = a; char d[5]; zb_uint8_t k = 0; do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) { m[n++] = d[--k]; } }
  m[n++] = ' ';
  { zb_uint16_t v = b; char d[5]; zb_uint8_t k = 0; do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) { m[n++] = d[--k]; } }
  m[n++] = '\n';
  UART2_write(uart, m, n, NULL);
}
#else
#define dbg(t, a, b) ((void)0)
#endif

static void line_done(void)
{
  zb_uint8_t z;
  zb_uint8_t v;
  line[llen] = 0;
  if (llen == 7 && line[0] == 'S' && line[1] == 'M' && line[2] == 'O' && line[3] == 'K' && line[4] == 'E') { z = 0; }
  else if (llen == 4 && line[0] == 'C' && line[1] == 'O') { z = 1; }
  else { llen = 0; return; }
  v = (zb_uint8_t)(line[llen - 1] == '1');
  if (line[llen - 2] != ' ' || (line[llen - 1] != '0' && line[llen - 1] != '1')) { llen = 0; return; }
  want[z] = v;
  dirty |= (zb_uint8_t)(1U << z);
  llen = 0;
  ZB_SCHEDULE_APP_CALLBACK(apply_cb, 0);
  SemaphoreP_post(wakeSem);
}

static void rx_cb(UART2_Handle h, void *buf, size_t count, void *arg, int_fast16_t status)
{
  ZVUNUSED(buf); ZVUNUSED(count); ZVUNUSED(arg);
  if (status == UART2_STATUS_SUCCESS)
  {
    if (rxb == '\n' || rxb == '\r') { if (llen) { line_done(); } }
    else if (llen < sizeof(line) - 1) { line[llen++] = (char)rxb; }
    else { llen = 0; }
  }
  UART2_read(h, &rxb, 1, NULL);
}

static void uart_start(void)
{
  UART2_Params p;
  UART2_Params_init(&p);
  p.baudRate = 115200;
  p.readMode = UART2_Mode_CALLBACK;
  p.readCallback = rx_cb;
  uart = UART2_open(CONFIG_UART2_0, &p);
  if (uart) { UART2_read(uart, &rxb, 1, NULL); }
}

/* Runs in the ZBOSS scheduler: push the wanted state into ZoneStatus and send the notification. */
static void apply_cb(zb_uint8_t param)
{
  zb_uint8_t d, z;
  ZVUNUSED(param);
  for (z = 0; z < 2; z++)
  {
    zb_uint8_t ep = (z == 0) ? EP_SMOKE : EP_CO;
    zb_uint16_t cur = (z == 0) ? zstatus_a : zstatus_b;
    zb_uint16_t nv;
    zb_bufid_t buf;
    d = (zb_uint8_t)((dirty >> z) & 1U);
    if (!d) { continue; }
    dirty &= (zb_uint8_t)~(1U << z);
    nv = want[z] ? (zb_uint16_t)(cur | ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1)
                 : (zb_uint16_t)(cur & ~ZB_ZCL_IAS_ZONE_ZONE_STATUS_ALARM1);
    buf = zb_buf_get_out();
    if (buf == ZB_BUF_INVALID) { dirty |= (zb_uint8_t)(1U << z); ZB_SCHEDULE_APP_ALARM(apply_cb, 0, ZB_MILLISECONDS_TO_BEACON_INTERVAL(50)); return; }
    zb_osif_led_on(1); if (!want[0] && !want[1]) { zb_osif_led_off(1); }
    {
      /* ZBOSS's zb_zcl_ias_zone_set_status() notifies only when an alarm bit is set (clears return false), so set the
         attribute and send the Zone Status Change Notification ourselves, to the CIE ZHA wrote (default 0x0000 ep 1). */
      zb_uint16_t dst = (z == 0) ? cie_short_a : cie_short_b;
      zb_uint8_t dep = (z == 0) ? cie_ep_a : cie_ep_b;
      zb_uint8_t st = (z == 0) ? zstate_a : zstate_b;
      zb_uint8_t zid = (z == 0) ? zid_a : zid_b;
      ZB_ZCL_SET_ATTRIBUTE(ep, ZB_ZCL_CLUSTER_ID_IAS_ZONE, ZB_ZCL_CLUSTER_SERVER_ROLE,
                           ZB_ZCL_ATTR_IAS_ZONE_ZONESTATUS_ID, (zb_uint8_t *)&nv, ZB_FALSE);
      if (st == ZB_ZCL_IAS_ZONE_ZONESTATE_ENROLLED)
      {
        ZB_ZCL_IAS_ZONE_SEND_STATUS_CHANGE_NOTIFICATION_REQ(buf, dst, ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
            dep ? dep : 1, ep, ZB_AF_HA_PROFILE_ID, NULL, nv, 0, zid, 0);
        dbg("NTF", ep, nv);
      }
      else { zb_buf_free(buf); dbg("NOENR", ep, nv); }
    }  }
}

/* ---- ZBOSS plumbing (as in on_off_light.c) ---- */
static void restart_commissioning(zb_uint8_t param)
{
  ZVUNUSED(param);
  bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
}

static void off_network_attention(zb_uint8_t param)
{
  ZVUNUSED(param);
  zb_osif_led_toggle(0);
  ZB_SCHEDULE_APP_ALARM(off_network_attention, 0, 1 * ZB_TIME_ONE_SECOND);
}

static void set_tx_power(zb_int8_t power)
{
  zb_uint32_t chanlist = DEFAULT_CHANLIST;
  for (zb_uint8_t i = 0; i < 32; i++)
  {
    if (chanlist & (1U << i))
    {
      zb_bufid_t buf = zb_buf_get_out();
      if (!buf) { return; }
      zb_tx_power_params_t *pp = (zb_tx_power_params_t *)zb_buf_begin(buf);
      pp->status = RET_OK; pp->page = 0; pp->channel = i; pp->tx_power = power; pp->cb = NULL;
      zb_set_tx_power_async(buf);
    }
  }
}

static zb_bool_t error_ind_handler(zb_uint8_t severity, zb_ret_t error_code, void *additional_info)
{
  ZVUNUSED(severity); ZVUNUSED(error_code); ZVUNUSED(additional_info);
  return ZB_FALSE;
}

static void device_cb(zb_uint8_t param)
{
  zb_zcl_device_callback_param_t *p = ZB_BUF_GET_PARAM(param, zb_zcl_device_callback_param_t);
  p->status = RET_OK;
}

MAIN()
{
  ARGV_UNUSED;
  ZB_INIT("alarm");

  zb_ieee_addr_t ieee_mac_addr;
  ZB_MEMCPY(ieee_mac_addr, fcfg->deviceInfo.macAddr, 8);
  /* Run Z: the same board joined ZHA earlier under its factory address with other endpoints. Flipping one bit of the
     low byte makes ZHA see a new device instead of a changed one; a product keeps the factory address. */
  ieee_mac_addr[0] ^= 0x10;
  zb_set_long_address(ieee_mac_addr);

#if defined ZB_ED_ROLE
  zb_set_network_ed_role(DEFAULT_CHANLIST);
  zb_set_ed_timeout(ED_TIMEOUT_VALUE);
  zb_set_rx_on_when_idle(ED_RX_ALWAYS_ON);
#if (ED_RX_ALWAYS_ON == ZB_FALSE)
  zb_set_keepalive_timeout(ZB_MILLISECONDS_TO_BEACON_INTERVAL(ED_POLL_RATE));
  zb_zdo_pim_set_long_poll_interval(ED_POLL_RATE);
#endif
#else
  zb_set_network_router_role(DEFAULT_CHANLIST);
#endif
#ifdef FRL_ERASE_NVRAM
  zb_set_nvram_erase_at_start(ZB_TRUE);   /* first boot after an image with other endpoints: forget the old network */
#else
  zb_set_nvram_erase_at_start(ZB_FALSE);
#endif

  ZB_AF_REGISTER_DEVICE_CTX(&alarm_ctx);
  ZB_ZCL_REGISTER_DEVICE_CB(device_cb);
  zb_error_register_app_handler(error_ind_handler);

  if (zboss_start_no_autostart() != RET_OK)
  {
    /* nothing to log: logging is off to save RAM */
  }
  else
  {
    GPIO_setConfig(CONFIG_GPIO_BTN1, GPIO_CFG_IN_PU);
    GPIO_setConfig(CONFIG_GPIO_BTN2, GPIO_CFG_IN_PU);
    if ((GPIO_read((zb_uint8_t)CONFIG_GPIO_BTN1) == 0U) || (GPIO_read((zb_uint8_t)CONFIG_GPIO_BTN2) == 0U))
    {
      perform_factory_reset = ZB_TRUE;
    }
    zb_osif_led_button_init();
    ZB_SCHEDULE_APP_ALARM(off_network_attention, 0, 1 * ZB_TIME_ONE_SECOND);
    uart_start();
    zboss_main_loop();
  }
  MAIN_RETURN(0);
}

void zboss_signal_handler(zb_uint8_t param)
{
  zb_zdo_app_signal_type_t sig = zb_get_app_signal(param, NULL);
  if (sig != 22) { dbg("ZB", (zb_uint16_t)sig, (zb_uint16_t)ZB_GET_APP_SIGNAL_STATUS(param)); }

  if (ZB_GET_APP_SIGNAL_STATUS(param) == 0)
  {
    switch (sig)
    {
      case ZB_ZDO_SIGNAL_SKIP_STARTUP:
        set_tx_power(DEFAULT_TX_PWR);
        zboss_start_continue();
        break;

      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      case ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (perform_factory_reset)
        {
          zb_bdb_reset_via_local_action(0);
          perform_factory_reset = ZB_FALSE;
        }
        else if (ZB_BDB_SIGNAL_DEVICE_REBOOT == sig)
        {
          ZB_SCHEDULE_APP_ALARM_CANCEL(off_network_attention, ZB_ALARM_ANY_PARAM);
          zb_osif_led_off(0);
        }
        set_tx_power(DEFAULT_TX_PWR);
        bdb_start_top_level_commissioning(ZB_BDB_NETWORK_STEERING);
        break;

      case ZB_COMMON_SIGNAL_CAN_SLEEP:
#ifdef ZB_USE_SLEEP
        /* zb_sleep_now() here stops the receiver and the UART listener (README, "Sleepy"); left out. */
#endif
        break;

      case ZB_BDB_SIGNAL_STEERING:
        ZB_SCHEDULE_APP_ALARM_CANCEL(off_network_attention, ZB_ALARM_ANY_PARAM);
        zb_osif_led_off(0);
        break;

      default:
        break;
    }
  }
  else
  {
    switch (sig)
    {
      case ZB_BDB_SIGNAL_DEVICE_FIRST_START:
      case ZB_BDB_SIGNAL_STEERING:
        ZB_SCHEDULE_APP_ALARM(restart_commissioning, 0, 10 * ZB_TIME_ONE_SECOND);
        break;
      default:
        break;
    }
  }
  if (param) { zb_buf_free(param); }
}
