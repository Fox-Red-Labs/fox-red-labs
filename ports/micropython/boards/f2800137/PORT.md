# MicroPython on the LAUNCHXL-F2800137: not built

**Short answer.** MicroPython's `py/` core nearly compiles for the C28x (132 of 133 files) once four toolchain gaps are filled, but no image was linked and the port has no board layer. It is not a working port, and the byte assumptions the core makes (a `char` is 8 bits) are not resolved by compiling. Measured 2026-11-09 with cl2000 25.11.1.LTS, MicroPython v1.29.0, the `FRL_MIN` profile of `ports/micropython`.

First error per pass (log: `c2000-launchpad/bench/micropython/cl2000-py-core-compile.log`):

| Pass | Change | First error |
|---|---|---|
| 1 | none | `mpconfigport.h` line 81: `cannot open source file "alloca.h"` |
| 2 | stub `alloca.h`, `unistd.h` | `mpconfig.h` line 2325: `cannot open source file "endian.h"` |
| 3 | stub `endian.h` | `vm.c` line 1181: `type name is not allowed` (a declaration inside `for (...)`; the compiler defaults to C89) |
| 4 | `--c11` | `emitbc.c` and `runtime.c`: `identifier "ssize_t" is undefined`; `scope.c`: `identifier "UINT8_MAX" is undefined` (this chip has no 8-bit integer type, so `stdint.h` omits it); `modsys.c`: the port's board name macro |
| 5 | `ssize_t` typedef, `UINT8_MAX` 255, board-name macros | 132 of 133 compile. `scheduler.c` fails in the assembler: the port's idle hook is the Arm instruction `wfi` |

**What compiling does not settle.** The core stores bytecode, qstrs and heap blocks as `byte` arrays and sizes them with `sizeof`; on this chip `sizeof` counts 16-bit words and a `uint8_t` typedef is a 16-bit unit, so the bytecode compiler and the VM may agree with each other and still disagree with every size the core prints or reads as a 32-bit value through a cast. `__builtin_clzl` (used by `misc.h`) does not exist for this compiler either. A port would need a board layer (SCI-A console, a timer for ticks, GPIO), a C28x idle hook, and a run of MicroPython's own tests to show the byte handling holds; none of that was started.

Rows: `port_flash_bytes` and `micropython_repl_boot` are `failed` in `data/results/f2800137.csv`.
