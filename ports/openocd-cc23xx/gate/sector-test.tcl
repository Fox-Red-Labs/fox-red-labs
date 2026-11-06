# One sector at the top of the CC2755P10's application area, 0xE7800 (sector 463): erase, blank check, write,
# then whether SACI PROG_MAIN_SECTOR erases first. Restore the whole board from the DSLite backup afterwards.
#   R=<folder with pattern.bin> openocd -c "adapter serial <serial>" -f board/ti_lp_em_cc2755p10.cfg -f sector-test.tcl
#   pattern.bin: 2048 bytes of 0x0F
set R $::env(R)
proc ms {} { return [clock milliseconds] }
proc hexdump16 {file} { set f [open $file rb]; set d [read $f 16]; close $f; binary scan $d H* h; return $h }
proc allff {file} { set f [open $file rb]; set d [read $f]; close $f; return [expr {[string length [string map [list [binary format c -1] {}] $d]] == 0}] }
init
set t [ms]; reset halt; echo "TIME reset_halt_ms [expr {[ms]-$t}]"
echo "pc after reset halt: [format 0x%08x [lindex [get_reg pc] 1]]"
echo "DEVICESTATUS [format 0x%08x [cc27xx.dap apreg 1 0xc]]"
flash probe 0
flash read_bank 0 $R/s463-before.bin 0xE7800 0x800
echo "before: blank=[allff $R/s463-before.bin] first16=[hexdump16 $R/s463-before.bin]"
set t [ms]; flash erase_sector 0 463 463; echo "TIME erase_one_sector_ms [expr {[ms]-$t}]"
flash read_bank 0 $R/s463-erased.bin 0xE7800 0x800
echo "after erase: blank=[allff $R/s463-erased.bin]"
set t [ms]; flash write_bank 0 $R/pattern.bin 0xE7800; echo "TIME write_one_sector_ms [expr {[ms]-$t}]"
flash read_bank 0 $R/s463-pattern.bin 0xE7800 0x800
echo "after write: first16=[hexdump16 $R/s463-pattern.bin]"

# Does SACI PROG_MAIN_SECTOR erase first? Program 16 bytes of 0xF0 over the 0x0F pattern.
# Erase-first: the sector reads F0 x16 then FF. Program-only (AND): 00 x16, and the rest keeps the pattern.
cc23xx saci_verify 0xE7800 0x800
proc saci_tx {w first} {
	while {[expr {[cc27xx.dap apreg 2 4] & 1}]} {}
	if {$first ne ""} { cc27xx.dap apreg 2 4 $first }
	cc27xx.dap apreg 2 0 $w
}
set seq 0x90
saci_tx [expr {0x0E | ($seq << 8) | (16 << 16)}] 2
saci_tx 0xB7E3A08F 0
saci_tx 0xE7800 ""
foreach w {0xF0F0F0F0 0xF0F0F0F0 0xF0F0F0F0 0xF0F0F0F0} { saci_tx $w "" }
while {[expr {[cc27xx.dap apreg 2 4] & 1}]} {}
set t [ms]; while {!([cc27xx.dap apreg 2 0xc] & 1)} { if {[ms]-$t > 5000} { echo "no SACI answer"; break } }
set h [cc27xx.dap apreg 2 8]
echo "PROG_MAIN_SECTOR header [format 0x%08x $h] result [format 0x%02x [expr {($h >> 16) & 0xff}]]"
set t [ms]; reset halt; echo "TIME reset_halt_ms [expr {[ms]-$t}]"
flash read_bank 0 $R/s463-saciprog.bin 0xE7800 0x800
echo "after SACI program: first16=[hexdump16 $R/s463-saciprog.bin]"
# leave the sector blank
flash erase_sector 0 463 463
flash read_bank 0 $R/s463-end.bin 0xE7800 0x800
echo "end: blank=[allff $R/s463-end.bin]"
shutdown
