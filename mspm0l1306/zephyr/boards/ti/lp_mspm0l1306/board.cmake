# SPDX-License-Identifier: Apache-2.0
# Same runners as the upstream lp_mspm0l2228 board.
# OpenOCD reads support/openocd.cfg (XDS110 probe, target/ti_mspm0.cfg).

board_runner_args(jlink "--device=MSPM0L1306" "--speed=4000")
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
