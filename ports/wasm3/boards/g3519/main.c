#include "../../common/frl_hal.h"
#include "../../common/frl_wasm.h"
int main(void) {
    hal_init();
    for (;;) {                       // repeat so a late-opened terminal sees a report; the LED blinks 3 times each pass
        frl_wasm_main();
        hal_delay_ms(3000);
    }
}
