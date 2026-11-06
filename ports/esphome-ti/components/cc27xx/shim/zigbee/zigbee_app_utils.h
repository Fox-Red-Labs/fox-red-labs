// TI F3 shim: the four Nordic "zigbee_app_utils" calls ESPHome's zigbee component makes. Implemented in ti_app.cpp on TI's ZBOSS.
#pragma once
#include <zboss_api.h>
#ifdef __cplusplus
extern "C" {
#endif
void zigbee_enable(void);
zb_ret_t zigbee_default_signal_handler(zb_bufid_t bufid);
void zigbee_configure_sleepy_behavior(zb_bool_t enable);
int ieee_addr_to_str(char *str_buf, int buf_len, const zb_ieee_addr_t addr);
#ifdef __cplusplus
}
#endif
