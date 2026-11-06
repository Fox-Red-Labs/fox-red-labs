#!/bin/bash
OCD=$HOME/zephyr-toolchain/xpack-openocd-0.12.0-7
$OCD/bin/openocd -s $OCD/openocd/scripts -c "adapter driver xds110; adapter serial MG020001; transport select swd; adapter speed 100; reset_config srst_only srst_nogate" -c "swd newdap g5187 cpu -expected-id 0; dap create g5187.dap -chain-position g5187.cpu" -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" -c init "$@" -c shutdown 2>&1
