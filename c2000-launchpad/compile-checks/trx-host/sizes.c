/* Ours: the size, in the compiler's own units, of the structs the TRX host driver lays over SPI bytes.
 * On Arm the unit is an 8-bit byte; on the C28x it is a 16-bit word. Compiled only, never run. */
#include "TRX.h"
#include "transport/spi_transport.h"
#define S(t) (unsigned long)sizeof(t)
const unsigned long frl_sizes[] = {
  S(TRX_TransactionHeader), S(TRX_TransactionFooter), S(TRX_Request_Header), S(TRX_Request_LastStatus),
  S(TRX_Phy), S(TRX_Request_CommandStore), S(TRX_Request_CommandSubmit), S(TRX_Request_CommandStop),
  S(TRX_Request_ConfigStore), S(TRX_PowerMode), S(TRX_DeviceConfigData), S(TRX_Request_CommandStore_Transmit_Params)
};
