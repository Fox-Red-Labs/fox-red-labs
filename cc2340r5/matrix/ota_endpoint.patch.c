/* Fox Red Labs fix: ZBOSS's on/off switch endpoint macro advertises 4 output clusters and leaves
 * out OTA Upgrade (0x0019), although this example's cluster list includes it. Coordinators that read
 * the simple descriptor (Home Assistant ZHA) then never see an OTA client. Same endpoint, OTA added. */
ZB_DECLARE_SIMPLE_DESC(3, 5);
ZB_AF_SIMPLE_DESC_TYPE(3, 5) simple_desc_on_off_switch_ep =
{
  ZB_SWITCH_ENDPOINT, ZB_AF_HA_PROFILE_ID, ZB_HA_ON_OFF_SWITCH_DEVICE_ID, ZB_HA_DEVICE_VER_ON_OFF_SWITCH, 0, 3, 5,
  {
    ZB_ZCL_CLUSTER_ID_BASIC, ZB_ZCL_CLUSTER_ID_IDENTIFY, ZB_ZCL_CLUSTER_ID_ON_OFF_SWITCH_CONFIG,
    ZB_ZCL_CLUSTER_ID_ON_OFF, ZB_ZCL_CLUSTER_ID_SCENES, ZB_ZCL_CLUSTER_ID_GROUPS, ZB_ZCL_CLUSTER_ID_IDENTIFY,
    ZB_ZCL_CLUSTER_ID_OTA_UPGRADE,
  }
};
ZB_AF_DECLARE_ENDPOINT_DESC(on_off_switch_ep, ZB_SWITCH_ENDPOINT, ZB_AF_HA_PROFILE_ID, 0, NULL,
    ZB_ZCL_ARRAY_SIZE(on_off_switch_clusters, zb_zcl_cluster_desc_t), on_off_switch_clusters,
    (zb_af_simple_desc_1_1_t*)&simple_desc_on_off_switch_ep, 0, NULL, 0, NULL);
