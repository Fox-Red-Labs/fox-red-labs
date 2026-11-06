# Does SACI PROG_MAIN_SECTOR erase first? Sector 249 (0x7C800) holds 0x0F everywhere; program 16 bytes of 0xF0 at its start.
# Erase-first: F0 x16 then FF. Program-only (AND): 00 x16, and the rest keeps 0F.
set R $::env(R)  ;# folder holding pattern.bin (2048 bytes of 0x0F); run with -f board/ti_lp_em_cc2340r5.cfg
proc ms {} { return [clock milliseconds] }
proc hexdump16 {file {off 0}} { set f [open $file rb]; seek $f $off; set d [read $f 16]; close $f; binary scan $d H* h; return $h }
proc allff {file} { set f [open $file rb]; set d [read $f]; close $f; return [expr {[string length [string map [list [binary format c -1] {}] $d]] == 0}] }
init
reset halt
flash write_bank 0 $R/pattern.bin 0x7C800
flash read_bank 0 $R/s249-p2.bin 0x7C800 0x800
echo "pattern: first16=[hexdump16 $R/s249-p2.bin] at16=[hexdump16 $R/s249-p2.bin 16]"
# enter SACI and leave the chip there (blank check of sector 249 is expected to fail: it holds the pattern)
catch { cc23xx saci_verify 0x7C800 0x800 } r; echo "saci_verify blank: $r"
proc saci_tx {w first} {
	while {[expr {[cc23x0.dap apreg 2 4] & 1}]} {}
	if {$first ne ""} { cc23x0.dap apreg 2 4 $first }
	cc23x0.dap apreg 2 0 $w
}
saci_tx [expr {0x0E | (0x90 << 8) | (16 << 16)}] 2
saci_tx 0xB7E3A08F 0
saci_tx 0x7C800 ""
foreach w {0xF0F0F0F0 0xF0F0F0F0 0xF0F0F0F0 0xF0F0F0F0} { saci_tx $w "" }
while {[expr {[cc23x0.dap apreg 2 4] & 1}]} {}
set t [ms]; while {!([cc23x0.dap apreg 2 0xc] & 1)} { if {[ms]-$t > 5000} { echo "no SACI answer"; break } }
set h [cc23x0.dap apreg 2 8]
echo "PROG_MAIN_SECTOR header [format 0x%08x $h] result [format 0x%02x [expr {($h >> 16) & 0xff}]]"
reset halt
flash read_bank 0 $R/s249-saciprog.bin 0x7C800 0x800
echo "after SACI program: first16=[hexdump16 $R/s249-saciprog.bin] at16=[hexdump16 $R/s249-saciprog.bin 16] last16=[hexdump16 $R/s249-saciprog.bin 2032]"
flash erase_sector 0 249 249
flash read_bank 0 $R/s249-end.bin 0x7C800 0x800
echo "end: blank=[allff $R/s249-end.bin]"
shutdown
