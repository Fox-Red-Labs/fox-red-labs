/* Serial on UART0. Polled transmit, interrupt-driven receive into a 128 byte ring buffer.
   The 4-byte hardware FIFOs stay off (SysConfig default), so the receive interrupt fires per byte. */
#include "Arduino.h"
#include "pins_internal.h"

UartSerial Serial;

void UartSerial::begin(unsigned long baud) { begin(baud, SERIAL_8N1); }

void UartSerial::begin(unsigned long baud, uint16_t config)
{
  DL_UART_PARITY par;
  switch (config & SERIAL_PARITY_MASK) {
  case SERIAL_PARITY_EVEN:  par = DL_UART_PARITY_EVEN; break;
  case SERIAL_PARITY_ODD:   par = DL_UART_PARITY_ODD; break;
  case SERIAL_PARITY_MARK:  par = DL_UART_PARITY_STICK_ONE; break;
  case SERIAL_PARITY_SPACE: par = DL_UART_PARITY_STICK_ZERO; break;
  default:                  par = DL_UART_PARITY_NONE; break;
  }
  DL_UART_WORD_LENGTH wl;
  switch (config & SERIAL_DATA_MASK) {
  case SERIAL_DATA_5: wl = DL_UART_WORD_LENGTH_5_BITS; break;
  case SERIAL_DATA_6: wl = DL_UART_WORD_LENGTH_6_BITS; break;
  case SERIAL_DATA_7: wl = DL_UART_WORD_LENGTH_7_BITS; break;
  default:            wl = DL_UART_WORD_LENGTH_8_BITS; break;
  }
  DL_UART_STOP_BITS sb = (config & SERIAL_STOP_BIT_MASK) == SERIAL_STOP_BIT_2 ? DL_UART_STOP_BITS_TWO
                                                                              : DL_UART_STOP_BITS_ONE;
  head = tail = 0;
  DL_UART_changeConfig(ARD_UART_INST);                       /* disables the UART for reconfiguration */
  DL_UART_setParityMode(ARD_UART_INST, par);
  DL_UART_setWordLength(ARD_UART_INST, wl);
  DL_UART_setStopBits(ARD_UART_INST, sb);
  DL_UART_configBaudRate(ARD_UART_INST, ARD_UART_INST_FREQUENCY, baud);
  DL_UART_enableInterrupt(ARD_UART_INST, DL_UART_INTERRUPT_RX);
  DL_UART_enable(ARD_UART_INST);
  NVIC_EnableIRQ(ARD_UART_INST_INT_IRQN);
}

void UartSerial::end()
{
  NVIC_DisableIRQ(ARD_UART_INST_INT_IRQN);
  flush();
  DL_UART_disable(ARD_UART_INST);
}

void UartSerial::_isr(void)
{
  while (!DL_UART_isRXFIFOEmpty(ARD_UART_INST)) {
    uint8_t c = DL_UART_receiveData(ARD_UART_INST);
    uint16_t next = (head + 1) % RXSIZE;
    if (next != tail) { buf[head] = c; head = next; }   /* buffer full: drop the new byte */
  }
}
extern "C" void UART0_IRQHandler(void)
{
  Serial._isr();
  DL_UART_clearInterruptStatus(ARD_UART_INST, DL_UART_INTERRUPT_RX | DL_UART_INTERRUPT_OVERRUN_ERROR |
                               DL_UART_INTERRUPT_BREAK_ERROR | DL_UART_INTERRUPT_FRAMING_ERROR |
                               DL_UART_INTERRUPT_PARITY_ERROR | DL_UART_INTERRUPT_NOISE_ERROR);
}

int UartSerial::available(void) { return (RXSIZE + head - tail) % RXSIZE; }
int UartSerial::peek(void) { return head == tail ? -1 : buf[tail]; }
int UartSerial::read(void)
{
  if (head == tail) return -1;
  uint8_t c = buf[tail];
  tail = (tail + 1) % RXSIZE;
  return c;
}
void UartSerial::flush(void) { while (DL_UART_isBusy(ARD_UART_INST)) ; }
size_t UartSerial::write(uint8_t c)
{
  DL_UART_transmitDataBlocking(ARD_UART_INST, c);
  return 1;
}
