// TI F3 shim: stands in for Zephyr's settings API, which ESPHome's zigbee component calls. NV storage is ZBOSS's own on TI.
#pragma once
static inline int settings_subsys_init(void) { return 0; }
static inline int settings_load(void) { return 0; }
