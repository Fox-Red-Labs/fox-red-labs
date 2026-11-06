"""ESPHome platform `cc23xx`: TI CC23xx (Cortex-M0+) boards. Zigbee end device only. Needs the cc27xx folder beside it."""

from esphome.components.cc27xx.platform_factory import make_platform

globals().update(make_platform("cc23xx", {"LP_EM_CC2340R5": "CC2340R5"}, "LP_EM_CC2340R5"))
