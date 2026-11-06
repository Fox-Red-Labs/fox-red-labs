/*
 * Spinel over TI UART2 (see spinel_uart.hpp). Modeled on OpenThread's src/posix/platform/hdlc_interface.cpp.
 */

#include "spinel_uart.hpp"

#include <string.h>

#include <openthread/platform/time.h>
#include <ti/drivers/dpl/ClockP.h>

#include "ti_drivers_config.h"
#include "system.h"

#include "common/code_utils.hpp"

volatile uint32_t frl_where        = 0;
volatile uint32_t frl_rx_callbacks = 0;

namespace ot {
namespace Host {

SpinelUart::SpinelUart(void)
    : mUart(nullptr)
    , mRingHead(0)
    , mRingTail(0)
    , mRingOverflows(0)
    , mSignalPending(false)
    , mRxSem(nullptr)
    , mReceiveFrameCallback(nullptr)
    , mReceiveFrameContext(nullptr)
    , mReceiveFrameBuffer(nullptr)
    , mFrameReceived(false)
{
    memset(&mMetrics, 0, sizeof(mMetrics));
}

otError SpinelUart::Init(ReceiveFrameCallback aCallback, void *aCallbackContext, RxFrameBuffer &aFrameBuffer)
{
    otError      error = OT_ERROR_NONE;
    UART2_Params params;

    VerifyOrExit(mUart == nullptr, error = OT_ERROR_ALREADY);

    mRxSem = SemaphoreP_constructBinary(&mRxSemStruct, 0);

    UART2_Params_init(&params);
    params.readMode       = UART2_Mode_CALLBACK;
    params.readCallback   = ReadCallback;
    params.readReturnMode = UART2_ReadReturnMode_PARTIAL;
    params.userArg        = this;
    params.writeMode      = UART2_Mode_BLOCKING;
    params.baudRate       = kBaudRate;
    params.dataLength     = UART2_DataLen_8;
    params.stopBits       = UART2_StopBits_1;
    params.parityType     = UART2_Parity_NONE;

    mUart = UART2_open(CONFIG_UART2_RCP, &params);
    VerifyOrExit(mUart != nullptr, error = OT_ERROR_FAILED);

    mHdlcDecoder.Init(aFrameBuffer, HandleHdlcFrame, this);
    mReceiveFrameCallback = aCallback;
    mReceiveFrameContext  = aCallbackContext;
    mReceiveFrameBuffer   = &aFrameBuffer;

    Rearm();

exit:
    return error;
}

void SpinelUart::Deinit(void)
{
    if (mUart != nullptr)
    {
        UART2_readCancel(mUart);
        UART2_close(mUart);
        mUart = nullptr;
    }
    mReceiveFrameCallback = nullptr;
    mReceiveFrameContext  = nullptr;
    mReceiveFrameBuffer   = nullptr;
}

void SpinelUart::Rearm(void) { UART2_read(mUart, mReadBuf, sizeof(mReadBuf), NULL); }

// Runs in the UART2 driver's callback context (interrupt or software interrupt): copy out, re-arm, wake the task.
void SpinelUart::ReadCallback(UART2_Handle aHandle, void *aBuf, size_t aLen, void *aUserArg, int_fast16_t aStatus)
{
    SpinelUart *self = static_cast<SpinelUart *>(aUserArg);

    (void)aHandle;
    frl_rx_callbacks++;

    if (aStatus == UART2_STATUS_SUCCESS && aLen > 0)
    {
        const uint8_t *bytes = static_cast<const uint8_t *>(aBuf);

        for (size_t i = 0; i < aLen; i++)
        {
            uint16_t next = static_cast<uint16_t>((self->mRingHead + 1) % kRingSize);

            if (next == self->mRingTail)
            {
                self->mRingOverflows++;
                break;
            }

            self->mRing[self->mRingHead] = bytes[i];
            self->mRingHead              = next;
        }
    }

    self->Rearm();
    SemaphoreP_post(self->mRxSem);

    // ot-ti's process queue holds 32 messages and asserts when full; a burst of chunks must not fill it.
    if (!self->mSignalPending)
    {
        self->mSignalPending = true;
        platformRadioSignal(1);
    }
}

// Ring to HDLC decoder. Only the OpenThread task calls this.
void SpinelUart::Drain(void)
{
    uint8_t chunk[128];

    mSignalPending = false;
    frl_where      = 0x30;

    while (mRingTail != mRingHead)
    {
        uint16_t n = 0;

        while (mRingTail != mRingHead && n < sizeof(chunk))
        {
            chunk[n++] = mRing[mRingTail];
            mRingTail  = static_cast<uint16_t>((mRingTail + 1) % kRingSize);
        }

        mHdlcDecoder.Decode(chunk, n);
    }
}

otError SpinelUart::SendFrame(const uint8_t *aFrame, uint16_t aLength)
{
    otError       error = OT_ERROR_NONE;
    Hdlc::Encoder hdlcEncoder(mEncoderBuffer);
    size_t        written = 0;

    frl_where = 0x10;
    mEncoderBuffer.Clear();
    SuccessOrExit(error = hdlcEncoder.BeginFrame());
    SuccessOrExit(error = hdlcEncoder.Encode(aFrame, aLength));
    SuccessOrExit(error = hdlcEncoder.EndFrame());

    VerifyOrExit(UART2_write(mUart, mEncoderBuffer.GetFrame(), mEncoderBuffer.GetLength(), &written) ==
                     UART2_STATUS_SUCCESS,
                 error = OT_ERROR_FAILED);
    VerifyOrExit(written == mEncoderBuffer.GetLength(), error = OT_ERROR_FAILED);

exit:
    frl_where = 0x11;
    mMetrics.mTransferredFrameCount++;

    if (error == OT_ERROR_NONE)
    {
        mMetrics.mTxFrameCount++;
        mMetrics.mTxFrameByteCount += aLength;
        mMetrics.mTransferredValidFrameCount++;
    }
    else
    {
        mMetrics.mTransferredGarbageFrameCount++;
    }

    return error;
}

// Block the OpenThread task until a whole spinel frame has been decoded, or the timeout passes.
otError SpinelUart::WaitForFrame(uint64_t aTimeoutUs)
{
    otError  error  = OT_ERROR_RESPONSE_TIMEOUT;
    uint64_t end    = otPlatTimeGet() + aTimeoutUs;
    uint32_t tickUs = ClockP_getSystemTickPeriod();

    mFrameReceived = false;
    frl_where      = 0x20;

    while (true)
    {
        uint64_t now;
        uint32_t ticks;

        Drain();

        if (mFrameReceived)
        {
            error = OT_ERROR_NONE;
            break;
        }

        now = otPlatTimeGet();

        if (now >= end)
        {
            break;
        }

        ticks     = static_cast<uint32_t>((end - now + tickUs - 1) / tickUs);
        frl_where = 0x21;
        SemaphoreP_pend(mRxSem, ticks == 0 ? 1 : ticks);
        frl_where = 0x22;
    }

    return error;
}

void SpinelUart::Process(const void *aMainloopContext)
{
    (void)aMainloopContext;
    Drain();
}

void SpinelUart::HandleHdlcFrame(void *aContext, otError aError)
{
    static_cast<SpinelUart *>(aContext)->HandleHdlcFrame(aError);
}

void SpinelUart::HandleHdlcFrame(otError aError)
{
    VerifyOrExit((mReceiveFrameCallback != nullptr) && (mReceiveFrameBuffer != nullptr));

    mMetrics.mTransferredFrameCount++;

    if (aError == OT_ERROR_NONE)
    {
        mMetrics.mRxFrameCount++;
        mMetrics.mRxFrameByteCount += mReceiveFrameBuffer->GetLength();
        mMetrics.mTransferredValidFrameCount++;
        mFrameReceived = true;
        mReceiveFrameCallback(mReceiveFrameContext);
    }
    else
    {
        mMetrics.mTransferredGarbageFrameCount++;
        mReceiveFrameBuffer->DiscardFrame();
    }

exit:
    return;
}

} // namespace Host
} // namespace ot
