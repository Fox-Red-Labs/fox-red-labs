/* Linker script for uLisp on the MSPM0G5187, MSPM0G3519 and MSPM0G3507 (adapted from ports/wasm3/boards/g3519/mspm0.ld).
   Sizes come from the build: --defsym FRL_FLASH_LEN, FRL_RAM_LEN, FRL_STACK_LEN.
   Flash starts at 0 and SRAM at 0x20200000 on both devices. The symbol names match what
   TI's startup file expects (__data_load__, __data_start__, __bss_start__, __StackTop). */

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (RX)  : ORIGIN = 0x00000000, LENGTH = FRL_FLASH_LEN
    SRAM  (RWX) : ORIGIN = 0x20200000, LENGTH = FRL_RAM_LEN
}

SECTIONS
{
    .intvecs : {
        KEEP(*(.intvecs))
    } > FLASH

    .text : {
        *(.text .text.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    .rodata : {
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > FLASH

    .preinit_array : {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } > FLASH

    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } > FLASH

    .fini_array : {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } > FLASH

    .ARM.exidx : {
        *(.ARM.exidx*)
    } > FLASH

    __etext = .;

    .ramfunc : {
        __ramfunct_load__ = LOADADDR(.ramfunc);
        __ramfunct_start__ = .;
        . = ALIGN(8);
        __ramfunct_end__ = .;
    } > SRAM AT> FLASH

    .data : {
        __data_load__ = LOADADDR(.data);
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(8);
        __data_end__ = .;
    } > SRAM AT> FLASH

    .bss (NOLOAD) : {
        __bss_start__ = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(8);
        __bss_end__ = .;
    } > SRAM

    /* No heap. Everything from here to the top of SRAM is the stack; uLisp checks its depth against `end`. */
    . = ALIGN(8);
    __heap_start__ = .;
    __StackTop = ORIGIN(SRAM) + LENGTH(SRAM);
    __stack_floor__ = __heap_start__;
    PROVIDE(__stack = __StackTop);
    PROVIDE(end = __heap_start__);
    PROVIDE(_end = __heap_start__);

    /DISCARD/ : {
        *(.ARM.extab*)
    }

    ASSERT(__StackTop - __heap_start__ >= FRL_STACK_LEN, "less than FRL_STACK_LEN bytes left for the stack")
}
