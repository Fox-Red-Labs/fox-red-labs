# Arduino layer on the LP-MSPM0G3507: build errors and fixes

Checked 2026-10-06 on one LP-MSPM0G3507 with the ArduinoCore-API layer in this folder. Terms: [docs/vocab.md](../../docs/vocab.md). 1 kB = 1,024 bytes. None of these needed an edit to a library source file. Build steps: [README.md](README.md).

## The compiler cannot find `Print.h`

Add `api/deprecated/` and `api/deprecated-avr-comp/` of ArduinoCore-API to the include path in `build.sh`. `Arduino_GFX.h` does `#include <Print.h>`, a legacy top-level header only old cores provide. ArduinoCore-API ships wrappers for it in `api/deprecated/`, and for `<avr/pgmspace.h>` in `api/deprecated-avr-comp/`.

## "Exception specification does not match" on `atexit`

Build with `-DHOST`. `api/Common.h` declares `int atexit(void (*)()) __attribute__((weak))`, while TI's `stdlib.h` declares it with a C++14 exception specification. `-DHOST` is the macro that header offers to skip its own `atexit` declaration.

## Clang warns that `main` is declared `extern "C"`

Build with `-Wno-main`. The same header declares `main` as `extern "C"`. As a side effect `main` is weak, which changes nothing here.

## `SPIClass` is ambiguous

Name the layer's class `MspSPI`, derived from `HardwareSPI`. ArduinoCore-API already has `typedef HardwareSPI SPIClass` in namespace `arduino`, so a layer that defines its own `SPIClass` makes the name ambiguous. `Arduino_HWSPI` reaches `MspSPI` as `SPIClass *`. Deriving forces four methods the layer does not support; they stop at a breakpoint.

## `Adafruit_BusIO` fails on `digitalPinToPort` and `portOutputRegister`

Define `digitalPinToPort`, `digitalPinToBitMask`, `portOutputRegister` and `portInputRegister` in `Arduino.h`, backed by `DOUT31_0` and `DIN31_0`. `Adafruit_BusIO`'s software-SPI file expects these AVR-style macros on any ARM board. Neither sketch uses that code path, so the macros are untested at run time.

## A function prototype is missing when an `.ino` sketch is built

Run `tools/ino2cpp.py`. Arduino sketches rely on the IDE adding function prototypes, and reading the raw text misses a signature inside an `#ifdef` (`serialOut()` here). `tools/ino2cpp.py` runs the preprocessor and reads prototypes from the preprocessed sketch, as the IDE does.

## The linker cannot find the compiler builtins, or runs out of stack and heap

Link with `-llibclang_rt.builtins.a`, not `-lclang_rt.builtins`, and give the image a 2 kB stack and a 4 kB heap. The TI linker takes `-l` plus a file name. SysConfig's linker file asks for only a 512-byte stack and no heap, too small for C++ and `new`; `build.sh` copies it to `out/linker.cmd` with the larger sizes. The `display-test` recipe never needed the builtins.

## Which SPI mode does the display get?

Mode 0 with the 5-line subclass in the sketch's display header; the library's own mode 2 is built but not run. **Unknown (suspected: Third-party behavior).** For a board it does not know, `Arduino_HWSPI` picks SPI mode 2, and `Arduino_ST7796::begin` sets a mode only for ESP32, nRF52840 (mode 3), ESP8266 and AVR. The module's ST7796S is specified for modes 0 and 3. `build.sh` also builds `pdqgfx_libmode` with `-DGFX_SPI_MODE_LIB_DEFAULT`, which leaves the library's default in place, so one flash shows whether mode 2 matters. Mode 2 may work by luck on this module, or may show noise.

## The SPI clock is too fast for the panel

Define `F_CPU` as `32000000UL` in `variant.h`. `SPI_DEFAULT_FREQ` is 24 MHz on unknown boards unless `F_CPU` is defined, which would exceed the panel's 15 MHz limit. With `F_CPU` defined the library uses `F_CPU / 4`, which is 8 MHz.

## The image is bigger than expected: two copies of the default font

Nothing to do. `Arduino_GFX.cpp` and `Arduino_TFT.cpp` each carry their own copy of the 1,280-byte default font table, which wastes 1.25 kB.
