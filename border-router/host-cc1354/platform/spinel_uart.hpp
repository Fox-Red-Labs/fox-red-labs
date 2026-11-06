/*
 * Spinel over a TI UART2 instance: the host side of the link to the radio co-processor.
 *
 * Fills OpenThread's SpinelInterface the way src/posix/platform/hdlc_interface.cpp does over a Linux tty,
 * but on a TI SimpleLink MCU under FreeRTOS: the UART2 read callback drops bytes into a ring, and the
 * OpenThread task drains the ring through the HDLC decoder, either when it processes drivers or while it
 * blocks in WaitForFrame for a reply from the RCP.
 */

#ifndef FRL_SPINEL_UART_HPP_
#define FRL_SPINEL_UART_HPP_

#include <stddef.h>
#include <stdint.h>

#include <ti/drivers/UART2.h>
#include <ti/drivers/dpl/SemaphoreP.h>

#include "lib/hdlc/hdlc.hpp"
#include "lib/spinel/multi_frame_buffer.hpp"
#include "lib/spinel/spinel_interface.hpp"

extern "C" {
extern volatile uint32_t frl_where;      // breadcrumb read by the watchdog reporter (app/die.c)
extern volatile uint32_t frl_rx_callbacks;
}

namespace ot {
namespace Host {

class SpinelUart : public Spinel::SpinelInterface
{
public:
    static constexpr uint32_t kBaudRate = 921600; // TI's RCP speed (ot-ti src/uart.c)

    SpinelUart(void);

    otError Init(ReceiveFrameCallback aCallback, void *aCallbackContext, RxFrameBuffer &aFrameBuffer) override;
    void    Deinit(void) override;
    otError SendFrame(const uint8_t *aFrame, uint16_t aLength) override;
    otError WaitForFrame(uint64_t aTimeoutUs) override;
    void    UpdateFdSet(void *aMainloopContext) override { (void)aMainloopContext; }
    void    Process(const void *aMainloopContext) override;
    uint32_t GetBusSpeed(void) const override { return kBaudRate; }
    otError  HardwareReset(void) override { return OT_ERROR_NOT_IMPLEMENTED; } // no reset wire to the RCP yet
    const otRcpInterfaceMetrics *GetRcpInterfaceMetrics(void) const override { return &mMetrics; }

    uint32_t GetRingOverflowCount(void) const { return mRingOverflows; }

private:
    static constexpr uint16_t kReadChunk = 256;  // one UART2_read; returns early on an idle line
    static constexpr uint16_t kRingSize  = 4096; // bytes buffered between the callback and the task

    static void ReadCallback(UART2_Handle aHandle, void *aBuf, size_t aLen, void *aUserArg, int_fast16_t aStatus);
    static void HandleHdlcFrame(void *aContext, otError aError);
    void        HandleHdlcFrame(otError aError);
    void        Drain(void);
    void        Rearm(void);

    UART2_Handle mUart;
    uint8_t      mReadBuf[kReadChunk];

    // Single producer (the UART callback) and single consumer (the OpenThread task): plain indices are enough.
    volatile uint16_t mRingHead;
    volatile uint16_t mRingTail;
    uint8_t           mRing[kRingSize];
    uint32_t          mRingOverflows;
    volatile bool     mSignalPending; // one process-loop message per burst, not one per UART chunk

    SemaphoreP_Struct mRxSemStruct;
    SemaphoreP_Handle mRxSem;

    ReceiveFrameCallback mReceiveFrameCallback;
    void                *mReceiveFrameContext;
    RxFrameBuffer       *mReceiveFrameBuffer;
    Hdlc::Decoder        mHdlcDecoder;
    volatile bool        mFrameReceived;

    Spinel::FrameBuffer<kMaxFrameSize> mEncoderBuffer; // static, not on the task stack
    otRcpInterfaceMetrics              mMetrics;
};

} // namespace Host
} // namespace ot

#endif // FRL_SPINEL_UART_HPP_
