# C2000 family porting FAQ: errors seen moving code to and from the LAUNCHXL-F2800137 and LAUNCHXL-F28P65X

Errors hit while moving code onto and off the two C2000 boards, with the fix and the file behind each. Board-only errors are in `c2000-launchpad/FAQ.md` and `c2000-launchpad/FAQ-f28p65x.md`.

Terms: [docs/vocab.md](../../../docs/vocab.md); the family guide is [README.md](README.md). 1 kB = 1,024 bytes.

## Building

### `error #20: identifier "uint8_t" is undefined`

Put `typedef unsigned char uint8_t;` before the library's headers. The C28x cannot address a byte, so TI's `stdint.h` omits `int8_t` and `uint8_t`. `c2000-launchpad/AGENTS.md`.

### `error #145: a value of type "char [772]" cannot be used to initialize an entity of type "const uint8_t [772]"` (u8g2)

Use the `unsigned char` typedef, not the `uint16_t` one. C2000Ware's `typedef uint16_t uint8_t;` clears error #20 but breaks the fonts. `c2000-launchpad/AGENTS.md`.

### `error: invalid redeclaration of type name "uint8_t"` when preincluding a typedef

Preinclude only the files that do not include driverlib. `driverlib/inc/hw_types.h` already defines `uint8_t`. `portability/three-ways/c2000/README.md`.

### `invalid attribute for "struct TRX_Phy_s"` with `uint8_t` undefined (TI CC1407P host driver)

`--preinclude` `portability/cc1407p-host/c28x/c28x_compat.h`, then replace the struct overlay with the field-by-field packer. `__attribute__((__packed__))` is written directly in `TRX.h` and there is no 8-bit type; the driver builds but the message bytes are still wrong on a 16-bit char (9 units sent against 10 bytes). `c2000-launchpad/AGENTS.md`.

### `warning #64-D: shift count is too large`

Write `1UL << n` (`1L << shift` in BitNetMCU). `int` is 16 bits. `c2000-launchpad/AGENTS.md`.

### `ERROR: ee_u16 is not a 16b datatype!`

Use this repo's three-change CoreMark patch; the score is then unofficial. Unmodified CoreMark assumes bytes. `c2000-launchpad/AGENTS.md`.

### `unresolved symbols remain` naming `code_start`, or `TransferRingBuffer_init`

For `code_start`, add `f280013x_codestartbranch.asm`, `--abi=eabi` and every `.c` SysConfig writes. For `TransferRingBuffer_init`, add `utilities/transfer/source/transfer_ringbuffer.c`. `c2000-launchpad/AGENTS.md`.

### `cannot set '$assign' to 'EPWM2'`, `'GPIO25'`, `'GPIO30'` or `cannot set 'ecapBase' to 'ECAP3_BASE'`

Pick pins and modules the F2800137 has. `c2000-launchpad/AGENTS.md`.

## Running

### A ported test builds and runs with no error but never measures anything

Reread this chip's own `hw_ints.h`. `INT_FREERTOS` is PIE group 12, line 6 on the F2800137 and line 12 (`0x00DB0C0CU`) on the F28P65X; a hard-coded flag bit never fired the handler. `c2000-launchpad/FAQ-f28p65x.md`.

### The CC1407P host driver sends different bytes than on Arm, or `LRF_*_byteCount` is half what you expect

Use `portability/cc1407p-host/wire/trx_wire.c`. Structs and bit-fields laid over the SPI buffer assume 8-bit bytes. The packer gave the same stream digest DC1DF5B1 as a Mac, 17 of 17 cases, 315 octets. `c2000-launchpad/AGENTS.md`, matrix id X47.

### `8888808888` for every digit (BitNetMCU), or a wrong tiny-AES-c ciphertext

Write the test digits as signed decimals and mask `xtime()` with `& 0xFF`. The code assumes a byte wraps at 256; FIPS-197 C.1 then passes. `c2000-launchpad/AGENTS.md`.

### `Operation was aborted on 'C28xx_CPU1'` on a low-power example

Press the wake-up button or reset. The chip is asleep and cannot be halted (`lpm_ex3_standbywake_gpio`, `lpm_ex5_haltwake_gpio`). `c2000-launchpad/AGENTS.md`.

### `There are not enough resources available`

Time with a CPU timer instead. The profile clock and a hardware breakpoint share debug hardware. `c2000-launchpad/AGENTS.md`.

### TI's DCC examples report fail as shipped

Build with `--define=USE_PLL_SRC_XTAL` (examples 1, 2 and 4 also `--define=CPU_FRQ_100MHZ`) or add `dcc1.fclk1 = 100;` to example 3's syscfg. `c2000-launchpad/AGENTS.md`.

## Mistakes to avoid

- Do not load `dcsm/dcsm_security_tool`: it puts data in the security area at 0x78000.
- Do not expect 4-pin JTAG on the F2800137 as shipped: TDI and TDO need R44 and R45 fitted.
