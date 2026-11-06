/* TI linker command file for size measurement: one flash region, one RAM region. */
--retain="*(.vectors)"
MEMORY { FLASH : origin = 0x00000000, length = 0x100000
         RAM   : origin = 0x20000000, length = 0x40000 }
SECTIONS {
  .vectors : > FLASH
  .text    : > FLASH
  .rodata  : > FLASH
  .data    : load = FLASH, run = RAM
  .bss     : > RAM
  .stack   : > RAM (HIGH)
}
_estack = 0x20040000;
