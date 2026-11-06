# Read-only diagnosis of an MSPM0C1104 that will not connect, plus TI's DSSM wait-for-debug (erases nothing).
# Usage (xPack OpenOCD 0.12.0+dev, the probe's CMSIS-DAP interface, which skips the XDS110 driver's core connect):
#   openocd -c "adapter driver cmsis-dap; adapter serial MC110001; transport select swd; adapter speed 1000; reset_config srst_only srst_nogate" \
#     -c "swd newdap c1104 cpu -expected-id 0; dap create c1104.dap -chain-position c1104.cpu" \
#     -f lockout_diag.tcl -c init -c c1104_bootdiag -c shutdown
# Register map: CFG-AP is AP1 (IDCODE 0x00, BOOTDIAG 0x10, LIFECYCLE 0x14; CCStudio targetdb Modules/msp/CFGAP.xml);
# SEC-AP is AP2 (TDR 0x0, TCR 0x4, RDR 0x8, RCR 0xC; OpenOCD target/ti_mspm0.cfg); PWR-AP is AP4.
# CCStudio's mspm0_cs_dap_init.gel reads BOOTDIAG 0x07 as a nested exception (double hard fault or NMI).

proc c1104_ap {ap r} { if {[catch {set v [c1104.dap apreg $ap $r]}]} {return -1}; return $v }

# 200 samples of the CFG-AP; a steady chip gives one line, a reset loop gives many.
proc c1104_bootdiag {} {
  set seen [dict create]
  for {set i 0} {$i < 200} {incr i} {
    dict incr seen [format "IDCODE 0x%08X BOOTDIAG 0x%08X LIFECYCLE 0x%08X AP0 IDR 0x%08X" \
      [c1104_ap 1 0x0] [c1104_ap 1 0x10] [c1104_ap 1 0x14] [c1104_ap 0 0xFC]]
  }
  dict for {k n} $seen { echo "$n x $k" }
}

# DSSM wait-for-debug (0x0206) through the mailbox, then a boot reset; prints the ROM's answer (0x00010003 = received).
proc c1104_wfd {} {
  c1104.dap apreg 2 0x4 0x0206
  c1104.dap apreg 2 0x0 0x0
  c1104_ap 2 0x8; c1104_ap 2 0xC
  sleep 200
  adapter assert srst; sleep 20; adapter deassert srst; sleep 50
  echo [format "WFD answer: RDR 0x%08X RCR 0x%08X" [c1104_ap 2 0x8] [c1104_ap 2 0xC]]
}

# ERASES main flash and resets NONMAIN to TI's defaults. DSSM factory reset (0x020A), as OpenOCD's mspm0_factory_reset
# sends it, with the ROM's answer printed. Run only with approval and a main-flash backup in hand (../data/oob-flash.bin).
proc c1104_factory_reset {} {
  c1104.dap apreg 2 0x4 0x020A
  c1104.dap apreg 2 0x0 0x0
  c1104_ap 2 0x8; c1104_ap 2 0xC
  sleep 1000
  adapter assert srst; sleep 20; adapter deassert srst
  set rcr -1
  for {set i 0} {$i < 1000} {incr i} { set rcr [c1104_ap 2 0xC]; if {$rcr >= 0 && ($rcr & 1)} break; sleep 2 }
  echo [format "factory reset answer: polls %d RCR 0x%08X RDR 0x%08X RCR 0x%08X" $i $rcr [c1104_ap 2 0x8] [c1104_ap 2 0xC]]
  sleep 1000
  adapter assert srst; sleep 20; adapter deassert srst; sleep 1000
}
