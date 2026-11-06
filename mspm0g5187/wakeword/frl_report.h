/* Fox Red Labs additions to TI's wakeword_detection_ai_npu: serial report, detection events,
 * a held green LED, and the replay input (samples from the Mac over the serial port). */
#ifndef FRL_REPORT_H
#define FRL_REPORT_H
#include <stdint.h>
#ifndef INPUT_ADC
#define INPUT_ADC 0
#endif
#ifndef INPUT_I2S
#define INPUT_I2S 0
#endif
#ifndef INPUT_REPLAY
#define INPUT_REPLAY 0
#endif
void frl_init(void);
void frl_ready(void);
void frl_poll(void);
void frl_t0(void);
void frl_t1(void);
void frl_lpc_only(void);
void frl_window_add(void);
void frl_l0(void);
void frl_l1(void);
void frl_after_inference(int8_t c0, int8_t c1);
void frl_tx_str(const char *s);
void frl_tx_int(int32_t v);
void frl_audio_start(void);
void frl_push_sample(int32_t s18);
#endif
