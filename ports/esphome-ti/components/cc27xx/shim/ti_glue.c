// TI F3 shim, C side. TI's driverlib headers (hw_fcfg.h / hw_ccfg.h) are not C++-clean: a struct declared inside a
// union inside a struct is incomplete when the header is included from C++. So the three register reads live here in C.
#include <stdint.h>
#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(inc/hw_fcfg.h)
#include DeviceFamily_constructPath(inc/hw_memmap.h)
#include DeviceFamily_constructPath(driverlib/pmctl.h)
#ifdef DeviceFamily_CC27XXX10
#include DeviceFamily_constructPath(driverlib/auxtempsense.h)
#else
#include <ti/drivers/Temperature.h>
#endif

void ti_fcfg_mac(uint8_t out[8]) {
  for (int i = 0; i < 8; i++) out[i] = fcfg->deviceInfo.macAddr[i];
}
void ti_reset_system(void) { PMCTLResetSystem(); }
#ifdef DeviceFamily_CC27XXX10
int32_t ti_die_temperature_c(void) { return AuxTempSenseGetTemperature(); }
#else
/* CC23x0: TI's Temperature driver (SDK ti/drivers/Temperature.h). It must be initialised by hand; it returns whole degrees C,
   and the caller expects degrees times 16 like the CC27xx call above. */
int32_t ti_die_temperature_c(void) {
  static int started;
  if (!started) { Temperature_init(); started = 1; }
  return (int32_t)Temperature_getTemperature() * 16;
}
#endif
