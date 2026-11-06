#include "../../common/frl_hal.h"
#include "../../common/frl_wasm.h"
extern volatile uint32_t frl_log_len;
int main(void) {
    hal_init();
    for (;;) {                       // repeat; the log restarts each pass, the LED blinks 3 times each pass
        frl_log_len = 0;
        frl_wasm_main();
        hal_delay_ms(3000);
    }
}
