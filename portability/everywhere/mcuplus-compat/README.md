# Moving a driver example between the AM263Px and the IWRL6432 radar SDKs

To move one, force-include one generated header and most of them build with the app untouched. Both SDKs are TI's MCU+ SDK underneath (same `/drivers/*` SysConfig modules, same makefile layout), but the radar's MMWAVE-L-SDK 05.05.04.02 carries an older spelling of some driver APIs than the AM263Px MCU+ SDK 26.01.00.08. Measured 2026-10-11 on every driver example the two SDKs ship under the same name:

| Direction | App files only | With the header | Ran on the board |
|---|---|---|---|
| AM263Px example in a radar SDK project | 11 of 23 | 19 of 23 | uart_echo and gpio_led_blink on the IWRL6432BOOST (before the header existed; both print TI's pass line) |
| Radar SDK example in an AM263Px project | 11 of 19 | 15 of 19 | built only |

Terms: [docs/vocab.md](../../../docs/vocab.md). Matrix ids X35 and X36 in `portability/matrix.csv`.

## Use it

1. Copy the target SDK's twin example (its SysConfig file, `main.c` and linker stay).
2. Drop the source SDK's app `.c` and `.h` files from the example root over it.
3. Add one line to the target makefile's `CFLAGS_common`: `-include <this folder>/am263p_on_mmwave.h` (AM263Px code on the radar) or `-include <this folder>/mmwave_on_am263p.h` (radar code on the AM263Px).
4. Build with the target SDK's makefile as usual. `../crossbuild_am263p_mmwave.sh` does all four for every twin: `COMPAT=<header> MODES=swapped ./crossbuild_am263p_mmwave.sh <work dir> <out.jsonl> am263p_to_mmwave`.

On a Mac the radar SDK's makefiles also need `"MKDIR=mkdir -p" "RMDIR=rm -rf" "RM=rm -f" COPY=cp CAT=cat TOUCH=touch` on the make line, and its boot-image step (`.appimage`) runs only on Linux.

## What the header maps

| Difference | Fix in the header |
|---|---|
| 47 EDMA functions spelled two ways (`EDMACCPaRAMEntry_init` and `EDMA_ccPaRAMEntry_init`, `EDMAChainChannel` and `EDMA_chainChannel`) | one `#define` each, generated from both `edma.h` files by `gen_compat.py` |
| AM263Px code gets the cache API through `edma.h`; the radar's does not include it | `#include <kernel/dpl/CacheP.h>`, plus `CacheP_wbInv` built from `CacheP_wb` and `CacheP_inv` |
| `EDMA_PARAM_BIDX`, `EDMA_PARAM_BIDX_EXT` exist only on the AM263Px | the AM263Px definitions |
| LIN data registers named `CSL_LIN_LINRD0` and `CSL_APP_LIN_LINRD0` | name maps (same offsets, 0x64 and 0x74, checked) |
| The AM263Px LIN gates its two interrupt lines with global enable and clear registers; the radar's LIN has none | no-ops on the radar (builds; interrupt path not run) |
| **Watchdog: the AM263Px driver starts the counter in `Watchdog_open`; the radar driver waits for `Watchdog_enable`** | on the radar, `Watchdog_open` is wrapped to call `Watchdog_enable`; on the AM263Px, `Watchdog_enable` is a no-op. Without this an AM263Px watchdog example builds on the radar and never fires |
| **I2C status codes share names but not values** (`I2C_STS_ERR_BUS_BUSY` is -4 on the AM263Px and -3 on the radar); the radar has `I2C_STS_ERR_ACCESS_ERROR`, the AM263Px does not | compare against the names, never numbers; the missing code becomes a value the AM263Px never returns |

## What it does not map, and why

| Example | Why it stays |
|---|---|
| `gpio_input_interrupt`, both ways | Different GPIO models: AM263Px interrupts are per bank (`GPIO_getBankIntrStatus`), the radar's per pin with level registers (`GPIO_getHighLowLevelPendingInterrupt`). Needs an adapter with logic, not a rename |
| `i2c_read`, AM263Px to radar | The AM263Px `I2C_Transaction` carries a `status` field the radar's does not |
| `lin_external`, `mcan_external_read_write`, AM263Px to radar | Board functions of the AM263Px LaunchPad (an I2C IO expander `CONFIG_I2C2`, `mcanEnableTransceiver`) |
| `edma_*_transfer`, radar to AM263Px | The radar example places its buffers in the radar's own memory (`CSL_APP_RAM_U_BASE`, a shared-memory clock gate): a chip memory map, not an API |

Regenerate the headers after either SDK changes: `python3 gen_compat.py <AM263Px SDK> <MMWAVE-L-SDK>`.
