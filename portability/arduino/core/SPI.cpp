#include "SPI.h"
#include "pins_internal.h"

MspSPI SPI;


void MspSPI::begin() { begun_ = true; DL_SPI_enable(ARD_SPI_INST); }
void MspSPI::end() { begun_ = false; DL_SPI_disable(ARD_SPI_INST); haveCurrent_ = false; }

void MspSPI::apply(const SPISettings &s)
{
  if (haveCurrent_ && s == current_) return;
  /* SCK = 32 MHz / (2 * (SCR + 1)); pick the fastest clock that does not exceed the request */
  uint32_t want = s.getClockFreq() ? s.getClockFreq() : 1;
  uint32_t scr = 0;
  while (scr < 1023 && (CPUCLK_FREQ / (2 * (scr + 1))) > want) scr++;
  DL_SPI_FRAME_FORMAT ff;
  switch (s.getDataMode()) {
  case SPI_MODE1: ff = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA1; break;
  case SPI_MODE2: ff = DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA0; break;
  case SPI_MODE3: ff = DL_SPI_FRAME_FORMAT_MOTO3_POL1_PHA1; break;
  default:        ff = DL_SPI_FRAME_FORMAT_MOTO3_POL0_PHA0; break;
  }
  DL_SPI_disable(ARD_SPI_INST);
  DL_SPI_setFrameFormat(ARD_SPI_INST, ff);
  DL_SPI_setBitOrder(ARD_SPI_INST, s.getBitOrder() == LSBFIRST ? DL_SPI_BIT_ORDER_LSB_FIRST
                                                               : DL_SPI_BIT_ORDER_MSB_FIRST);
  DL_SPI_setBitRateSerialClockDivider(ARD_SPI_INST, scr);
  DL_SPI_enable(ARD_SPI_INST);
  current_ = s;
  haveCurrent_ = true;
}

void MspSPI::beginTransaction(SPISettings settings)
{
  if (!begun_) begin();
  apply(settings);
  inTransaction_ = true;
}

void MspSPI::endTransaction(void) { inTransaction_ = false; }

/* Send count bytes and receive count bytes. At most 4 bytes are in flight, so the receive
   FIFO cannot overflow. Received bytes go back into buf unless buf is in flash (a library
   may pass a const table through the in-place API; the write would fault or be lost). */
static void spi_xfer(uint8_t *buf, size_t count)
{
  const bool writeback = (uintptr_t)buf >= 0x20000000UL;
  size_t tx = 0, rx = 0;
  while (!DL_SPI_isRXFIFOEmpty(ARD_SPI_INST)) (void)DL_SPI_receiveData8(ARD_SPI_INST);
  while (rx < count) {
    while (tx < count && (tx - rx) < 4 && !DL_SPI_isTXFIFOFull(ARD_SPI_INST)) {
      DL_SPI_transmitData8(ARD_SPI_INST, buf[tx]);
      tx++;
    }
    while (rx < tx && !DL_SPI_isRXFIFOEmpty(ARD_SPI_INST)) {
      uint8_t v = DL_SPI_receiveData8(ARD_SPI_INST);
      if (writeback) buf[rx] = v;
      rx++;
    }
  }
  while (DL_SPI_isBusy(ARD_SPI_INST)) ;   /* last bit has left, so chip select can rise */
}

uint8_t MspSPI::transfer(uint8_t data)
{
  spi_xfer(&data, 1);
  return data;
}

uint16_t MspSPI::transfer16(uint16_t data)
{
  uint8_t b[2] = { (uint8_t)(data >> 8), (uint8_t)data };
  if (current_.getBitOrder() == LSBFIRST) { b[0] = (uint8_t)data; b[1] = (uint8_t)(data >> 8); }
  spi_xfer(b, 2);
  return current_.getBitOrder() == LSBFIRST ? (uint16_t)(b[0] | (b[1] << 8)) : (uint16_t)((b[0] << 8) | b[1]);
}

void MspSPI::transfer(void *buf, size_t count)
{
  if (count) spi_xfer((uint8_t *)buf, count);
}

void MspSPI::usingInterrupt(int) { compat_unsupported("SPI.usingInterrupt"); }
void MspSPI::notUsingInterrupt(int) { compat_unsupported("SPI.notUsingInterrupt"); }
void MspSPI::attachInterrupt() { compat_unsupported("SPI.attachInterrupt"); }
void MspSPI::detachInterrupt() { compat_unsupported("SPI.detachInterrupt"); }
