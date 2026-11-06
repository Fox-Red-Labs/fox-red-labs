# CC2340R5 (LS470GTU): sector 249 at 0x7C800, the highest blank sector below the image's top sectors (250 to 255).
set R $::env(R)  ;# folder holding pattern.bin (2048 bytes of 0x0F); run with -f board/ti_lp_em_cc2340r5.cfg
proc ms {} { return [clock milliseconds] }
proc hexdump16 {file {off 0}} { set f [open $file rb]; seek $f $off; set d [read $f 16]; close $f; binary scan $d H* h; return $h }
proc allff {file} { set f [open $file rb]; set d [read $f]; close $f; return [expr {[string length [string map [list [binary format c -1] {}] $d]] == 0}] }
init
set t [ms]; reset halt; echo "TIME reset_halt_ms [expr {[ms]-$t}]"
echo "state [cc23x0.cpu curstate] pc [format 0x%08x [dict get [get_reg pc] pc]] r3 [format 0x%08x [dict get [get_reg r3] r3]]"
echo "DEVICESTATUS [format 0x%08x [cc23x0.dap apreg 1 0xc]]"
flash probe 0
flash read_bank 0 $R/s249-before.bin 0x7C800 0x800
echo "before: blank=[allff $R/s249-before.bin]"
set t [ms]; flash write_bank 0 $R/pattern.bin 0x7C800; echo "TIME write_one_sector_ms [expr {[ms]-$t}]"
flash read_bank 0 $R/s249-pattern.bin 0x7C800 0x800
echo "after write: first16=[hexdump16 $R/s249-pattern.bin] last16=[hexdump16 $R/s249-pattern.bin 2032]"
set t [ms]; flash erase_sector 0 249 249; echo "TIME erase_one_sector_ms [expr {[ms]-$t}]"
flash read_bank 0 $R/s249-erased.bin 0x7C800 0x800
echo "after erase: blank=[allff $R/s249-erased.bin]"
shutdown
