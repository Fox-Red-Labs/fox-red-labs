/*
 *  Copyright (c) 2020, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @file
 *   The CC1354P10 host's radio: OpenThread's RadioSpinel over a TI UART to a TI RCP (CC2340R5, ot-rcp).
 *
 * Fox Red Labs, 2026. The otPlatRadio* wrappers below are OpenThread's src/posix/platform/radio.cpp verbatim
 * (BSD-3); the init, process-loop and timer parts replace the POSIX file-descriptor plumbing with ot-ti's
 * message-queue loop (platformRadioSignal / platformRadioProcess in system.c) and a ClockP one-shot.
 */

#include <string.h>

#include <openthread/platform/radio.h>
#include <openthread/platform/time.h>
#include <openthread/thread.h>
#include <ti/drivers/dpl/ClockP.h>

#include "spinel_uart.hpp"
#include "system.h"

extern "C" void frl_watchdog_kick(void);

#include "common/code_utils.hpp"
#include "lib/platform/exit_code.h"
#include "lib/spinel/radio_spinel.hpp"
#include "lib/spinel/spinel_driver.hpp"

namespace {

ot::Host::SpinelUart      sInterface;
ot::Spinel::SpinelDriver  sSpinelDriver;
ot::Spinel::RadioSpinel   sRadioSpinel;
ClockP_Struct             sPollClockStruct;
ClockP_Handle             sPollClock;
bool                      sStateCallbackSet;

constexpr otRadioCaps kRequiredRadioCaps =
#if OPENTHREAD_CONFIG_THREAD_VERSION >= OT_THREAD_VERSION_1_2
    OT_RADIO_CAPS_TRANSMIT_SEC | OT_RADIO_CAPS_TRANSMIT_TIMING |
#endif
    OT_RADIO_CAPS_ACK_TIMEOUT | OT_RADIO_CAPS_TRANSMIT_RETRIES | OT_RADIO_CAPS_CSMA_BACKOFF;

ot::Spinel::RadioSpinel &GetRadioSpinel(void) { return sRadioSpinel; }

void PollClockCallback(uintptr_t aArg)
{
    OT_UNUSED_VARIABLE(aArg);
    platformRadioSignal(0);
}

// What the POSIX host does with select() timeouts: wake again when RadioSpinel's next deadline comes.
void SchedulePoll(void)
{
    uint64_t now      = otPlatTimeGet();
    uint64_t deadline = GetRadioSpinel().GetNextRadioTimeRecalcStart();
    uint64_t remainUs;
    uint32_t tickUs = ClockP_getSystemTickPeriod();
    uint32_t ticks;

    if (GetRadioSpinel().IsTransmitting())
    {
        uint64_t txRadioEndUs = GetRadioSpinel().GetTxRadioEndUs();

        if (txRadioEndUs < deadline)
        {
            deadline = txRadioEndUs;
        }
    }

    if (GetRadioSpinel().IsTransmitDone())
    {
        platformRadioSignal(0);
        return;
    }

    if (deadline == UINT64_MAX)
    {
        ClockP_stop(sPollClock);
        return;
    }

    remainUs = (deadline > now) ? deadline - now : 0;

    if (remainUs > 60 * OT_US_PER_S)
    {
        remainUs = 60 * OT_US_PER_S;
    }

    ticks = static_cast<uint32_t>(remainUs / tickUs);
    ClockP_stop(sPollClock);
    ClockP_setTimeout(sPollClock, ticks == 0 ? 1 : ticks);
    ClockP_start(sPollClock);
}

void HandleStateChanged(otChangedFlags aFlags, void *aContext)
{
    otInstance *instance = static_cast<otInstance *>(aContext);

    if (OT_CHANGED_THREAD_NETIF_STATE & aFlags)
    {
        GetRadioSpinel().SetTimeSyncState(otIp6IsEnabled(instance));
    }
}

} // namespace

extern "C" {

// Called from otSysInit (ot-ti system.c) before the OpenThread instance exists; talks to the RCP synchronously.
void platformRadioInit(void)
{
    spinel_iid_t                            iidList[ot::Spinel::kSpinelHeaderMaxNumIid];
    struct ot::Spinel::RadioSpinelCallbacks callbacks;
    ClockP_Params                           clockParams;
    CoprocessorType                         type;

    ClockP_Params_init(&clockParams);
    clockParams.startFlag = false;
    clockParams.period    = 0;
    sPollClock            = ClockP_construct(&sPollClockStruct, PollClockCallback, 0, &clockParams);

    memset(iidList, SPINEL_HEADER_INVALID_IID, sizeof(iidList));
    iidList[0] = 0;

    type = sSpinelDriver.Init(sInterface, /* aSoftwareReset */ true, iidList, OT_ARRAY_LENGTH(iidList));
    VerifyOrDie(type == OT_COPROCESSOR_RCP, OT_EXIT_FAILURE);

    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.mEnergyScanDone    = otPlatRadioEnergyScanDone;
    callbacks.mBusLatencyChanged = otPlatRadioBusLatencyChanged;
    callbacks.mReceiveDone       = otPlatRadioReceiveDone;
    callbacks.mTransmitDone      = otPlatRadioTxDone;
    callbacks.mTxStarted         = otPlatRadioTxStarted;

    sRadioSpinel.SetCallbacks(callbacks);
    sRadioSpinel.Init(/* aSkipRcpVersionCheck */ false, /* aSoftwareReset */ true, &sSpinelDriver,
                      kRequiredRadioCaps, /* aEnableRcpTimeSync */ true);
}

// ot-ti's process loop hands us every radio signal: bytes from the UART callback, or the poll clock.
void platformRadioProcess(otInstance *aInstance, uintptr_t aArg)
{
    OT_UNUSED_VARIABLE(aArg);

    if (!sStateCallbackSet)
    {
        // Same hook as the POSIX host: RCP time sync only while the Thread interface is up.
        IgnoreError(otSetStateChangedCallback(aInstance, HandleStateChanged, aInstance));
        sStateCallbackSet = true;
    }

    frl_where = 0x40;
    sSpinelDriver.Process(nullptr);
    frl_where = 0x41;
    GetRadioSpinel().Process(nullptr);
    frl_where = 0x42;
    SchedulePoll();
    frl_where = 0x43;
}

// ot-ti's system.c still dispatches the NCP SPI peripheral; this host has none.
void platformSpiProcess(void) {}

} // extern "C"

// ---- OpenThread's POSIX otPlatRadio* wrappers, unchanged (src/posix/platform/radio.cpp) ----------------------

void otPlatRadioGetIeeeEui64(otInstance *aInstance, uint8_t *aIeeeEui64)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().GetIeeeEui64(aIeeeEui64));
}

void otPlatRadioSetPanId(otInstance *aInstance, uint16_t panid)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().SetPanId(panid));
}

void otPlatRadioSetExtendedAddress(otInstance *aInstance, const otExtAddress *aAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    otExtAddress addr;

    for (size_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aAddress->m8[sizeof(addr) - 1 - i];
    }

    SuccessOrDie(GetRadioSpinel().SetExtendedAddress(addr));
}

void otPlatRadioSetShortAddress(otInstance *aInstance, uint16_t aAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().SetShortAddress(aAddress));
}

void otPlatRadioSetAlternateShortAddress(otInstance *aInstance, uint16_t aAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().SetAlternateShortAddress(aAddress));
}

void otPlatRadioSetPromiscuous(otInstance *aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().SetPromiscuous(aEnable));
}

bool otPlatRadioIsEnabled(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().IsEnabled();
}

otError otPlatRadioEnable(otInstance *aInstance) { return GetRadioSpinel().Enable(aInstance); }

otError otPlatRadioDisable(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().Disable();
}

otError otPlatRadioSleep(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().Sleep();
}

otError otPlatRadioReceive(otInstance *aInstance, uint8_t aChannel)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error;

    SuccessOrExit(error = GetRadioSpinel().Receive(aChannel));

exit:
    return error;
}

otError otPlatRadioTransmit(otInstance *aInstance, otRadioFrame *aFrame)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().Transmit(*aFrame);
}

otRadioFrame *otPlatRadioGetTransmitBuffer(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return &GetRadioSpinel().GetTransmitFrame();
}

int8_t otPlatRadioGetRssi(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetRssi();
}

otRadioCaps otPlatRadioGetCaps(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetRadioCaps();
}

const char *otPlatRadioGetVersionString(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetVersion();
}

bool otPlatRadioGetPromiscuous(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().IsPromiscuous();
}
void otPlatRadioEnableSrcMatch(otInstance *aInstance, bool aEnable)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().EnableSrcMatch(aEnable));
}

otError otPlatRadioAddSrcMatchShortEntry(otInstance *aInstance, uint16_t aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().AddSrcMatchShortEntry(aShortAddress);
}

otError otPlatRadioAddSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    otExtAddress addr;

    for (size_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aExtAddress->m8[sizeof(addr) - 1 - i];
    }

    return GetRadioSpinel().AddSrcMatchExtEntry(addr);
}

otError otPlatRadioClearSrcMatchShortEntry(otInstance *aInstance, uint16_t aShortAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().ClearSrcMatchShortEntry(aShortAddress);
}

otError otPlatRadioClearSrcMatchExtEntry(otInstance *aInstance, const otExtAddress *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);
    otExtAddress addr;

    for (size_t i = 0; i < sizeof(addr); i++)
    {
        addr.m8[i] = aExtAddress->m8[sizeof(addr) - 1 - i];
    }

    return GetRadioSpinel().ClearSrcMatchExtEntry(addr);
}

void otPlatRadioClearSrcMatchShortEntries(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().ClearSrcMatchShortEntries());
}

void otPlatRadioClearSrcMatchExtEntries(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    SuccessOrDie(GetRadioSpinel().ClearSrcMatchExtEntries());
}

otError otPlatRadioEnergyScan(otInstance *aInstance, uint8_t aScanChannel, uint16_t aScanDuration)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().EnergyScan(aScanChannel, aScanDuration);
}

otError otPlatRadioGetTransmitPower(otInstance *aInstance, int8_t *aPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    assert(aPower != nullptr);
    return GetRadioSpinel().GetTransmitPower(*aPower);
}

otError otPlatRadioSetTransmitPower(otInstance *aInstance, int8_t aPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetTransmitPower(aPower);
}

otError otPlatRadioGetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t *aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    assert(aThreshold != nullptr);
    return GetRadioSpinel().GetCcaEnergyDetectThreshold(*aThreshold);
}

otError otPlatRadioSetCcaEnergyDetectThreshold(otInstance *aInstance, int8_t aThreshold)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetCcaEnergyDetectThreshold(aThreshold);
}

otError otPlatRadioGetFemLnaGain(otInstance *aInstance, int8_t *aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    assert(aGain != nullptr);
    return GetRadioSpinel().GetFemLnaGain(*aGain);
}

otError otPlatRadioSetFemLnaGain(otInstance *aInstance, int8_t aGain)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetFemLnaGain(aGain);
}

int8_t otPlatRadioGetReceiveSensitivity(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetReceiveSensitivity();
}

#if OPENTHREAD_CONFIG_PLATFORM_RADIO_COEX_ENABLE
otError otPlatRadioSetCoexEnabled(otInstance *aInstance, bool aEnabled)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetCoexEnabled(aEnabled);
}

bool otPlatRadioIsCoexEnabled(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().IsCoexEnabled();
}

otError otPlatRadioGetCoexMetrics(otInstance *aInstance, otRadioCoexMetrics *aCoexMetrics)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error = OT_ERROR_NONE;

    VerifyOrExit(aCoexMetrics != nullptr, error = OT_ERROR_INVALID_ARGS);

    error = GetRadioSpinel().GetCoexMetrics(*aCoexMetrics);

exit:
    return error;
}
#endif

uint32_t otPlatRadioGetSupportedChannelMask(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    uint32_t channelMask;

#if OPENTHREAD_POSIX_CONFIG_CONFIGURATION_FILE_ENABLE
    if (sConfig.IsValid())
    {
        channelMask = sConfig.GetSupportedChannelMask();
    }
    else
#endif
    {
        channelMask = GetRadioSpinel().GetRadioChannelMask(false);
    }

    return channelMask;
}

uint32_t otPlatRadioGetPreferredChannelMask(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    uint32_t channelMask;

#if OPENTHREAD_POSIX_CONFIG_CONFIGURATION_FILE_ENABLE
    if (sConfig.IsValid())
    {
        channelMask = sConfig.GetPreferredChannelMask();
    }
    else
#endif
    {
        channelMask = GetRadioSpinel().GetRadioChannelMask(true);
    }

    return channelMask;
}

otRadioState otPlatRadioGetState(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetState();
}

void otPlatRadioSetMacKey(otInstance             *aInstance,
                          uint8_t                 aKeyIdMode,
                          uint8_t                 aKeyId,
                          const otMacKeyMaterial *aPrevKey,
                          const otMacKeyMaterial *aCurrKey,
                          const otMacKeyMaterial *aNextKey,
                          otRadioKeyType          aKeyType)
{
    SuccessOrDie(GetRadioSpinel().SetMacKey(aKeyIdMode, aKeyId, aPrevKey, aCurrKey, aNextKey));
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aKeyType);
}

void otPlatRadioSetMacFrameCounter(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    SuccessOrDie(GetRadioSpinel().SetMacFrameCounter(aMacFrameCounter, /* aSetIfLarger */ false));
    OT_UNUSED_VARIABLE(aInstance);
}

void otPlatRadioSetMacFrameCounterIfLarger(otInstance *aInstance, uint32_t aMacFrameCounter)
{
    SuccessOrDie(GetRadioSpinel().SetMacFrameCounter(aMacFrameCounter, /* aSetIfLarger */ true));
    OT_UNUSED_VARIABLE(aInstance);
}

uint64_t otPlatRadioGetNow(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetNow();
}

uint32_t otPlatRadioGetBusSpeed(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetBusSpeed();
}

uint32_t otPlatRadioGetBusLatency(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().GetBusLatency();
}

#if OPENTHREAD_CONFIG_MAC_CSL_RECEIVER_ENABLE || OPENTHREAD_CONFIG_MAC_CSL_TRANSMITTER_ENABLE
uint8_t otPlatRadioGetCslAccuracy(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return GetRadioSpinel().GetCslAccuracy();
}
#endif

#if OPENTHREAD_CONFIG_MAC_CSL_TRANSMITTER_ENABLE
uint8_t otPlatRadioGetCslUncertainty(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return GetRadioSpinel().GetCslUncertainty();
}
#endif

otError otPlatRadioSetChannelMaxTransmitPower(otInstance *aInstance, uint8_t aChannel, int8_t aMaxPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetChannelMaxTransmitPower(aChannel, aMaxPower);
}

#if OPENTHREAD_CONFIG_PLATFORM_POWER_CALIBRATION_ENABLE
otError otPlatRadioAddCalibratedPower(otInstance    *aInstance,
                                      uint8_t        aChannel,
                                      int16_t        aActualPower,
                                      const uint8_t *aRawPowerSetting,
                                      uint16_t       aRawPowerSettingLength)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().AddCalibratedPower(aChannel, aActualPower, aRawPowerSetting, aRawPowerSettingLength);
}

otError otPlatRadioClearCalibratedPowers(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().ClearCalibratedPowers();
}

otError otPlatRadioSetChannelTargetPower(otInstance *aInstance, uint8_t aChannel, int16_t aTargetPower)
{
    OT_UNUSED_VARIABLE(aInstance);
    return GetRadioSpinel().SetChannelTargetPower(aChannel, aTargetPower);
}
#endif

otError otPlatRadioSetRegion(otInstance *aInstance, uint16_t aRegionCode)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error;

#if OPENTHREAD_POSIX_CONFIG_CONFIGURATION_FILE_ENABLE
    if (sConfig.IsValid())
    {
        error = sConfig.SetRegion(aRegionCode);
    }
    else
#endif
    {
        error = GetRadioSpinel().SetRadioRegion(aRegionCode);
    }

    return error;
}

otError otPlatRadioGetRegion(otInstance *aInstance, uint16_t *aRegionCode)
{
    OT_UNUSED_VARIABLE(aInstance);

    otError error;

#if OPENTHREAD_POSIX_CONFIG_CONFIGURATION_FILE_ENABLE
    if (sConfig.IsValid())
    {
        *aRegionCode = sConfig.GetRegion();
        error        = OT_ERROR_NONE;
    }
    else
#endif
    {
        error = GetRadioSpinel().GetRadioRegion(aRegionCode);
    }

    return error;
}

#if OPENTHREAD_CONFIG_MLE_LINK_METRICS_SUBJECT_ENABLE
otError otPlatRadioConfigureEnhAckProbing(otInstance          *aInstance,
                                          otLinkMetrics        aLinkMetrics,
                                          const otShortAddress aShortAddress,
                                          const otExtAddress  *aExtAddress)
{
    OT_UNUSED_VARIABLE(aInstance);

    return GetRadioSpinel().ConfigureEnhAckProbing(aLinkMetrics, aShortAddress, *aExtAddress);
}
#endif

otError otPlatRadioReceiveAt(otInstance *aInstance, uint8_t aChannel, uint32_t aStart, uint32_t aDuration)
{
    OT_UNUSED_VARIABLE(aInstance);
    OT_UNUSED_VARIABLE(aChannel);
    OT_UNUSED_VARIABLE(aStart);
    OT_UNUSED_VARIABLE(aDuration);
    return OT_ERROR_NOT_IMPLEMENTED;
}

#if OPENTHREAD_CONFIG_PLATFORM_BOOTLOADER_MODE_ENABLE
otError otPlatResetToBootloader(otInstance *aInstance)
{
    OT_UNUSED_VARIABLE(aInstance);

    return GetRadioSpinel().SendReset(SPINEL_RESET_BOOTLOADER);
}
#endif
