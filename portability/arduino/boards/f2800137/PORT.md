# Arduino layer on the LAUNCHXL-F2800137: not supported

**Short answer.** The layer does not build for the C28x. Its API half is the unmodified ArduinoCore-API, which is written in C++11, and TI's C2000 compiler (cl2000 25.11.1.LTS) accepts C++03 only. The first error is in `String.h`, before any board code is reached. Measured 2026-11-09 with CCStudio 21.0.1.

| Step | Result |
|---|---|
| `api/Print.cpp` and `api/String.cpp` with `--c++14` | `String.h` line 75: `copy constructor for class "arduino::String" may not have a parameter of type "arduino::String"`, then `expected a ")"` at lines 75 and 100 (the rvalue-reference constructor `String(String &&rval)`) |
| `api/Common.cpp` | `Common.h` line 145: `invalid storage class for a template declaration` (the trailing return type `auto min(...) -> decltype(...)`) |
| `--c++11` and `--c++14` | `WARNING: invalid compiler option --c++14 (ignored)`; the compiler lists `--c++03` as its only C++ mode |
| Two-line test (`int f(int&& a)`, `auto g(int a)->int`) | the same errors in every mode |

All five API files (`Common`, `Print`, `Stream`, `String`, `IPAddress`) fail the same way. Log: `c2000-launchpad/bench/arduino/cl2000-arduino-core-api.log`.

**What this leaves.** The layer's MSPM0 half (`core/wiring.cpp`: SysTick and the IOMUX pin table; `core/HardwareSerial.cpp`: DriverLib UART) would also be rewritten for C2000Ware, which is more than a few files, but it was not attempted because the API half stops first. Two routes exist and neither is the layer as written: a small hand-written `Arduino.h` over the C2000Ware calls, which is what `ports/ulisp/boards/f2800137/` does for uLisp (`pinMode`, `digitalWrite`, `millis`, `delay` and a `Serial` on SCI-A all compile there), or a different compiler. Blink would run on the first; `String`, `Print` and the libraries that use them would not be the unmodified ArduinoCore-API.

The C28x `char` is 16 bits as well: `uint8_t` needs the `unsigned char` typedef from `ports/ulisp/boards/f2800137/compat.h` (`c2000-launchpad/AGENTS.md`).
