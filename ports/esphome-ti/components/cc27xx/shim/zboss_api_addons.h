// TI F3 shim: stands in for Nordic's zboss_api_addons.h. TI ships the macros (ZB_ZCL_DECLARE_BASIC_ATTRIB_LIST_EXT,
// ZBOSS_DECLARE_DEVICE_CTX_1_EP..) but not the struct or the variadic device-context macro ESPHome's generated code uses.
#pragma once
#include <zboss_api.h>
typedef struct {
  zb_uint8_t zcl_version;
  zb_uint8_t app_version;
  zb_uint8_t stack_version;
  zb_uint8_t hw_version;
  zb_char_t mf_name[ZB_ZCL_MAX_STRING_SIZE];
  zb_char_t model_id[ZB_ZCL_MAX_STRING_SIZE];
  zb_char_t date_code[ZB_ZCL_MAX_STRING_SIZE];
  zb_uint8_t power_source;
  zb_char_t location_id[ZB_ZCL_MAX_STRING_SIZE];
  zb_uint8_t ph_env;
  zb_char_t sw_ver[ZB_ZCL_MAX_STRING_SIZE];
} zb_zcl_basic_attrs_ext_t;
#define ESPHOME_ZB_COUNT_ARGS_(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define ZBOSS_DECLARE_DEVICE_CTX_EP_VA(device_ctx_name, ...) \
  zb_af_endpoint_desc_t *ep_list_##device_ctx_name[] = {__VA_ARGS__}; \
  ZBOSS_DECLARE_DEVICE_CTX(device_ctx_name, ep_list_##device_ctx_name, \
                           (ZB_ZCL_ARRAY_SIZE(ep_list_##device_ctx_name, zb_af_endpoint_desc_t *)))
