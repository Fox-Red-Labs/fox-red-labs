# SPDX-License-Identifier: Apache-2.0

if(CONFIG_BOARD_LP_MSPM0L1306_GPIO)
  zephyr_library_named(board_lp_mspm0l1306_gpio)
  zephyr_library_sources(gpio_mspm0_l1306.c)
endif()
