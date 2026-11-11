// SPDX-License-Identifier: GPL-3.0-only
// TI F3 shim: C entry points the ti_adc and ti_pwm components call (they cannot include TI's headers themselves).
// Each pin the YAML uses has a SysConfig instance (CONFIG_FRL_ADC_<n>, CONFIG_FRL_PWM_<n>); frl_periph.h, written by
// periph.py, lists which DIO each one serves.
#include <cstddef>
#include <cstdint>

extern "C" {
#include <FreeRTOS.h>
#include <ti/drivers/ADC.h>
#include <ti/drivers/PWM.h>
#include "ti_drivers_config.h"
#include "frl_periph.h"
}

namespace {
struct PinCfg {
  uint8_t dio;
  uint8_t cfg;
};
const PinCfg adc_pins[] = FRL_ADC_PINS;
const PinCfg pwm_pins[] = FRL_PWM_PINS;
ADC_Handle adc_handles[FRL_ADC_COUNT ? FRL_ADC_COUNT : 1];
PWM_Handle pwm_handles[FRL_PWM_COUNT ? FRL_PWM_COUNT : 1];

int find(const PinCfg *t, int n, int dio) {
  for (int i = 0; i < n; i++)
    if (t[i].dio == dio) return i;
  return -1;
}
}  // namespace

extern "C" {

size_t frl_heap_free(void) { return xPortGetFreeHeapSize(); }
size_t frl_heap_min_free(void) { return xPortGetMinimumEverFreeHeapSize(); }

// ADC: returns 0 and the pin's voltage in microvolts, or a negative number.
int frl_adc_read_uv(int dio, int32_t *uv, int32_t *raw) {
  int i = find(adc_pins, FRL_ADC_COUNT, dio);
  if (i < 0) return -1;
  if (adc_handles[i] == nullptr) {
    ADC_init();
    ADC_Params p;
    ADC_Params_init(&p);
    adc_handles[i] = ADC_open(adc_pins[i].cfg, &p);
    if (adc_handles[i] == nullptr) return -2;
  }
  uint16_t r = 0;
  if (ADC_convert(adc_handles[i], &r) != ADC_STATUS_SUCCESS) return -3;
  if (raw) *raw = r;
  if (uv) *uv = static_cast<int32_t>(ADC_convertToMicroVolts(adc_handles[i], r));
  return 0;
}

// PWM: frequency in Hz, duty as a fraction 0 to 65535 (of the period).
int frl_pwm_start(int dio, uint32_t hz) {
  int i = find(pwm_pins, FRL_PWM_COUNT, dio);
  if (i < 0) return -1;
  PWM_init();
  PWM_Params p;
  PWM_Params_init(&p);
  p.dutyUnits = PWM_DUTY_FRACTION;
  p.dutyValue = 0;
  p.periodUnits = PWM_PERIOD_HZ;
  p.periodValue = hz;
  pwm_handles[i] = PWM_open(pwm_pins[i].cfg, &p);
  if (pwm_handles[i] == nullptr) return -2;
  PWM_start(pwm_handles[i]);
  return 0;
}

int frl_pwm_set(int dio, uint16_t duty) {
  int i = find(pwm_pins, FRL_PWM_COUNT, dio);
  if (i < 0 || pwm_handles[i] == nullptr) return -1;
  uint32_t d = static_cast<uint32_t>((static_cast<uint64_t>(duty) * PWM_DUTY_FRACTION_MAX) / 65535u);
  return PWM_setDuty(pwm_handles[i], d) == PWM_STATUS_SUCCESS ? 0 : -2;
}

}  // extern "C"
