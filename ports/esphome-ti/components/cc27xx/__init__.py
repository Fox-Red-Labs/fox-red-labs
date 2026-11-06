"""ESPHome platform `cc27xx`: TI CC27xx (Cortex-M33) boards. Zigbee only so far. See ../../README.md."""

from .platform_factory import make_platform

globals().update(make_platform("cc27xx", {"LP_EM_CC2755P10": "CC2755P10"}, "LP_EM_CC2755P10"))
