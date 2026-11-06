// Fox Red Labs: Smoke CO Alarm state for the LP-EM-CC35X1 Matter Smoke CO Alarm example.
#include "SmokeCoManager.h"
#include "AppConfig.h"

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include <app/clusters/smoke-co-alarm-server/smoke-co-alarm-server.h>
#include <platform/CHIPDeviceLayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ti/drivers/UART2.h>
#include <ti/drivers/apps/LED.h>
#include <ti_drivers_config.h>

using namespace ::chip;
using namespace ::chip::DeviceLayer;
using namespace ::chip::app::Clusters::SmokeCoAlarm;

extern LED_Handle gThermoLedRed; // set by AppTask
extern UART2_Handle sDebugUartHandle; // platform Logging.cpp; opened with an RX ring buffer by our patch

namespace {
constexpr EndpointId kEp = 1;
SmokeCoManager sMgr;
TimerHandle_t sTestTimer;

std::array<ExpressedStateEnum, SmokeCoAlarmServer::kPriorityOrderLength> sPriority = {
    ExpressedStateEnum::kSmokeAlarm,     ExpressedStateEnum::kInterconnectSmoke, ExpressedStateEnum::kCOAlarm,
    ExpressedStateEnum::kInterconnectCO, ExpressedStateEnum::kHardwareFault,     ExpressedStateEnum::kTesting,
    ExpressedStateEnum::kEndOfService,   ExpressedStateEnum::kBatteryAlert
};

uint32_t NowMs() { return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS); }
} // namespace

SmokeCoManager & SmokeCoMgr()
{
    return sMgr;
}

// Writes both alarm states and the expressed state in one locked step. Critical is what Apple Home turns into a critical
// notification; Normal clears it (the server emits AllClear when everything is Normal).
void SmokeCoManager::Apply(const char * src)
{
    uint32_t t0 = NowMs();
    PlatformMgr().LockChipStack();
    SmokeCoAlarmServer & s = SmokeCoAlarmServer::Instance();
    s.SetSmokeState(kEp, mSmoke ? AlarmStateEnum::kCritical : AlarmStateEnum::kNormal);
    s.SetCOState(kEp, mCO ? AlarmStateEnum::kCritical : AlarmStateEnum::kNormal);
    s.SetExpressedStateByPriority(kEp, sPriority);
    PlatformMgr().UnlockChipStack();
    if (gThermoLedRed != nullptr)
    {
        if (mSmoke || mCO)
            LED_setOn(gThermoLedRed, LED_BRIGHTNESS_MAX);
        else
            LED_setOff(gThermoLedRed);
    }
    PLAT_LOG("ALARM_SET t=%lu src=%s smoke=%d co=%d apply_ms=%lu", (unsigned long) t0, src, (int) mSmoke, (int) mCO,
             (unsigned long) (NowMs() - t0));
}

void SmokeCoManager::SetSmoke(bool on, const char * src)
{
    mSmoke = on;
    Apply(src);
}

void SmokeCoManager::SetCO(bool on, const char * src)
{
    mCO = on;
    Apply(src);
}

void SmokeCoManager::ParseLine(char * line)
{
    PLAT_LOG("LINE_RX t=%lu '%s'", (unsigned long) NowMs(), line);
    if (strncmp(line, "SMOKE ", 6) == 0)
        SetSmoke(line[6] == '1', "uart");
    else if (strncmp(line, "CO ", 3) == 0)
        SetCO(line[3] == '1', "uart");
}

void SmokeCoUartTask(void *)
{
    char line[24];
    size_t len = 0;
    uint8_t c;
    size_t got;
    for (;;)
    {
        got = 0;
        if (UART2_read(sDebugUartHandle, &c, 1, &got) != UART2_STATUS_SUCCESS || got != 1)
            continue;
        if (c == '\n' || c == '\r')
        {
            if (len > 0)
            {
                line[len] = 0;
                sMgr.ParseLine(line);
            }
            len = 0;
        }
        else if (len < sizeof(line) - 1)
            line[len++] = (char) c;
        else
            len = 0;
    }
}

// Self test (SelfTestRequest command, the "Test" button in some apps): the server has already set TestInProgress and the
// Testing expressed state. We hold it 3 s, then clear it.
static void TestDone(TimerHandle_t)
{
    PlatformMgr().LockChipStack();
    SmokeCoAlarmServer::Instance().SetTestInProgress(kEp, false);
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kEp, sPriority);
    PlatformMgr().UnlockChipStack();
    PLAT_LOG("SELFTEST done t=%lu", (unsigned long) NowMs());
}

void SmokeCoManager::OnSelfTest()
{
    PLAT_LOG("SELFTEST start t=%lu", (unsigned long) NowMs());
    if (sTestTimer == nullptr)
        sTestTimer = xTimerCreate("STEST", pdMS_TO_TICKS(3000), pdFALSE, NULL, TestDone);
    if (sTestTimer != nullptr)
        xTimerStart(sTestTimer, 0);
}

void emberAfPluginSmokeCoAlarmSelfTestRequestCommand(EndpointId)
{
    SmokeCoMgr().OnSelfTest();
}

static void HeartbeatTask(void *)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        PLAT_LOG("FXL_ALIVE t=%lu heap_free=%u min_free=%u smoke=%d co=%d", (unsigned long) NowMs(),
                 (unsigned) xPortGetFreeHeapSize(), (unsigned) xPortGetMinimumEverFreeHeapSize(), (int) sMgr.mSmoke,
                 (int) sMgr.mCO);
    }
}

void SmokeCoBootMarker(void)
{
    PLAT_LOG("FXL_BOOT smoke-co image built " __DATE__ " " __TIME__);
    xTaskCreate(HeartbeatTask, "FXLHB", 2048 / sizeof(StackType_t), NULL, 1, NULL);
}

int SmokeCoManager::Init()
{
    PLAT_LOG("SMOKECO init start");
    PlatformMgr().LockChipStack();
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kEp, sPriority);
    PlatformMgr().UnlockChipStack();
    if (xTaskCreate(SmokeCoUartTask, "SCUART", 3072 / sizeof(StackType_t), NULL, 3, NULL) != pdPASS)
        return -2;
    PLAT_LOG("SMOKECO ready: console UART 115200, lines SMOKE 0|1 and CO 0|1, SW1 smoke, SW2 CO, endpoint 1");
    return 0;
}
