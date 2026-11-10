/* Link file for uLisp on the F2800137: code and constants in flash, data and the workspace in RAMLS0+RAMLS1 (contiguous, 16376 words), stack at the end of it. */
MEMORY
{
   BEGIN            : origin = 0x00080000, length = 0x00000002
   RAMM0            : origin = 0x00000128, length = 0x000002D8
   RAMM1            : origin = 0x00000400, length = 0x000003F8
   RAMLS01          : origin = 0x00008000, length = 0x00003FF8
   RESET            : origin = 0x003FFFC0, length = 0x00000002
   FLASH_LOW        : origin = 0x080002, length = 0x1FFE
   FLASH_MAIN       : origin = 0x082000, length = 0x1E000
}
SECTIONS
{
   codestart        : > BEGIN
   .text            : > FLASH_MAIN, ALIGN(8)
   .cinit           : > FLASH_LOW, ALIGN(8)
   .switch          : > FLASH_MAIN, ALIGN(8)
   .reset           : > RESET, TYPE = DSECT
   .stack           : > RAMLS01
   .init_array      : > FLASH_LOW, ALIGN(8)
   .const           : > FLASH_MAIN, ALIGN(8)
   .bss             : > RAMLS01
   .bss:output      : > RAMLS01
   .data            : > RAMLS01
   .sysmem          : > RAMM0
   .bss:cio         : > RAMLS01
   .TI.ramfunc      : LOAD = FLASH_MAIN, RUN = RAMLS01, LOAD_START(RamfuncsLoadStart), LOAD_SIZE(RamfuncsLoadSize), LOAD_END(RamfuncsLoadEnd), RUN_START(RamfuncsRunStart), RUN_SIZE(RamfuncsRunSize), RUN_END(RamfuncsRunEnd), ALIGN(8)
}
