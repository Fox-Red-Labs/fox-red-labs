// Fox Red Labs: thermostat state for the LP-EM-CC35X1 Matter thermostat example.
#include "ThermostatManager.h"
#include "AppConfig.h"

#include <FreeRTOS.h>
#include <task.h>
#include <timers.h>

#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app/clusters/occupancy-sensor-server/occupancy-hal.h>
#include <platform/CHIPDeviceLayer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ti/drivers/UART2.h>
#include <ti/drivers/apps/LED.h>
#include <ti_drivers_config.h>

using namespace ::chip;
using namespace ::chip::DeviceLayer;
using namespace ::chip::app::Clusters;

extern LED_Handle gThermoLedRed; // set by AppTask
#ifdef FRL_CSI
#include "csi_glue.h"
#endif

namespace {
constexpr EndpointId kThermostatEp = 1;
constexpr EndpointId kOccupancyEp  = 2;
constexpr uint32_t kLineTimeoutMs  = 5000;
constexpr int16_t kFallbackTemp    = 2080; // 20.8 C in 0.01 C
UART2_Handle sUart;
ThermostatManager sMgr;
TimerHandle_t sWatchTimer;

uint32_t NowMs() { return (uint32_t) (xTaskGetTickCount() * portTICK_PERIOD_MS); }

int16_t Clamp(int32_t v, int32_t lo, int32_t hi) { return (int16_t) (v < lo ? lo : (v > hi ? hi : v)); }
} // namespace

ThermostatManager & ThermoMgr()
{
    return sMgr;
}

static void SetOccupied(bool occ)
{
    halOccupancyStateChangedCallback(kOccupancyEp, occ ? HAL_OCCUPANCY_STATE_OCCUPIED : HAL_OCCUPANCY_STATE_UNOCCUPIED);
    if (gThermoLedRed != nullptr)
    {
        if (occ)
            LED_setOn(gThermoLedRed, LED_BRIGHTNESS_MAX);
        else
            LED_setOff(gThermoLedRed);
    }
}

void ThermostatManager::ApplyFromLine(int presence, int setpointTenths, int tempTenths)
{
    PlatformMgr().LockChipStack();
    mSuppress = true;
    Thermostat::Attributes::LocalTemperature::Set(kThermostatEp, Clamp(tempTenths * 10, -5000, 12500));
    int16_t sp = Clamp(setpointTenths * 10, 700, 3000);
    Thermostat::Attributes::OccupiedHeatingSetpoint::Set(kThermostatEp, sp);
    SetOccupied(presence != 0);
    mSuppress = false;
    PlatformMgr().UnlockChipStack();
}

// Line: T,<seq>,<presence>,<zones>,<setpoint>,<temp>
void ThermostatManager::ParseLine(char * line)
{
    if (line[0] != 'T' || line[1] != ',')
        return;
    char * f[6] = { 0 };
    int n       = 0;
    for (char * p = line; n < 6; n++)
    {
        f[n]         = p;
        char * comma = strchr(p, ',');
        if (comma == nullptr)
        {
            n++;
            break;
        }
        *comma = 0;
        p      = comma + 1;
    }
    if (n != 6)
        return;
    int seq = atoi(f[1]), presence = atoi(f[2]), sp = atoi(f[4]), temp = atoi(f[5]);
    mLineSeen     = true;
    mLastLineTick = NowMs();
    PLAT_LOG("LINE_RX t=%lu seq=%d presence=%d sp=%d temp=%d", (unsigned long) mLastLineTick, seq, presence, sp, temp);
    ApplyFromLine(presence, sp, temp);
}

void ThermostatUartTask(void *)
{
    char line[48];
    size_t len = 0;
    uint8_t c;
    size_t got;
    for (;;)
    {
        got = 0;
        if (UART2_read(sUart, &c, 1, &got) != UART2_STATUS_SUCCESS || got != 1)
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

// Every second: if no line for 5 s, say so once and keep the fallback temperature.
void ThermostatWatchdog(void *)
{
    static bool warned = false;
    if (sMgr.mLineSeen && (NowMs() - sMgr.mLastLineTick) > kLineTimeoutMs)
    {
        sMgr.mLineSeen = false;
        warned         = true;
        PLAT_LOG("LINE_LOST: no T line for %lu ms, fallback 20.8 C, buttons active", (unsigned long) kLineTimeoutMs);
    }
    (void) warned;
}

void ThermostatManager::SendToPeer()
{
    // Proposed, not in PROTOCOL.md yet: "M,<heat tenths>,<cool tenths>,<mode>\n" back to the G3519 on UART TX.
    int16_t heat = 0, cool = 0;
    Thermostat::SystemModeEnum mode = Thermostat::SystemModeEnum::kHeat;
    PlatformMgr().LockChipStack();
    Thermostat::Attributes::OccupiedHeatingSetpoint::Get(kThermostatEp, &heat);
    Thermostat::Attributes::OccupiedCoolingSetpoint::Get(kThermostatEp, &cool);
    Thermostat::Attributes::SystemMode::Get(kThermostatEp, &mode);
    PlatformMgr().UnlockChipStack();
    char out[40];
    int n = snprintf(out, sizeof(out), "M,%d,%d,%u\n", heat / 10, cool / 10, (unsigned) static_cast<uint8_t>(mode));
    size_t wrote;
    if (sUart != nullptr && n > 0)
        UART2_write(sUart, out, (size_t) n, &wrote);
}

void ThermostatManager::OnControllerWrite(uint32_t attributeId, int16_t value)
{
    if (mSuppress)
        return;
    // Runs in the Matter thread. The stamp is the board's tick clock; the log is read on the Mac, which stamps its own clock.
    PLAT_LOG("CONTROLLER_WRITE t=%lu attr=0x%lx value=%d", (unsigned long) NowMs(), (unsigned long) attributeId, (int) value);
    SendToPeer();
}

void ThermostatManager::StepSetpoint(int16_t deltaCentiC)
{
    int16_t heat = 0;
    PlatformMgr().LockChipStack();
    mSuppress = true;
    Thermostat::Attributes::OccupiedHeatingSetpoint::Get(kThermostatEp, &heat);
    heat = Clamp(heat + deltaCentiC, 700, 3000);
    Thermostat::Attributes::OccupiedHeatingSetpoint::Set(kThermostatEp, heat);
    mSuppress = false;
    PlatformMgr().UnlockChipStack();
    PLAT_LOG("BUTTON_SETPOINT t=%lu heat=%d", (unsigned long) NowMs(), (int) heat);
    SendToPeer();
}

static void HeartbeatTask(void *)
{
    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
        PLAT_LOG("FXL_ALIVE t=%lu heap_free=%u min_free=%u line=%d", (unsigned long) NowMs(), (unsigned) xPortGetFreeHeapSize(),
                 (unsigned) xPortGetMinimumEverFreeHeapSize(), (int) sMgr.LineSeen());
    }
}

// Called right after the log UART opens, before the Matter stack starts: proves the image boots and the console works.
void ThermostatBootMarker(void)
{
    PLAT_LOG("FXL_BOOT thermostat image built " __DATE__ " " __TIME__);
    xTaskCreate(HeartbeatTask, "FXLHB", 2048 / sizeof(StackType_t), NULL, 1, NULL);
}

int ThermostatManager::Init()
{
    PLAT_LOG("THERMO init start");
    // Fallback state: 20.8 C, nobody present, default setpoints from the data model.
    PlatformMgr().LockChipStack();
    Thermostat::Attributes::LocalTemperature::Set(kThermostatEp, kFallbackTemp);
    PlatformMgr().UnlockChipStack();

    PLAT_LOG("THERMO opening UART2_1");
    UART2_Params p;
    UART2_Params_init(&p);
    p.baudRate        = 115200;
    p.readReturnMode  = UART2_ReadReturnMode_FULL;
    sUart             = UART2_open(CONFIG_UART2_1, &p);
    if (sUart == nullptr)
    {
        PLAT_LOG("THERMO UART open failed");
        return -1;
    }
    if (xTaskCreate(ThermostatUartTask, "TUART", 3072 / sizeof(StackType_t), NULL, 3, NULL) != pdPASS)
        return -2;
    sWatchTimer = xTimerCreate("TWATCH", pdMS_TO_TICKS(1000), pdTRUE, NULL, (TimerCallbackFunction_t) ThermostatWatchdog);
    if (sWatchTimer != nullptr)
        xTimerStart(sWatchTimer, 0);
    PLAT_LOG("THERMO ready: UART2_1 115200, endpoint 1 thermostat, endpoint 2 occupancy");
#ifdef FRL_CSI
    csi_glue_start();
    PLAT_LOG("THERMO csi: Wi-Fi presence pipeline armed, waits for the station to join");
#endif
    return 0;
}

#ifdef FRL_CSI
// One line per 2 s window from TI's CSI presence pipeline (csi_glue.c). The Wi-Fi result drives the Occupancy Sensing
// endpoint only while no radar line has arrived for 5 s; with a radar line present it is logged and the radar wins.
extern "C" void ThermoCsiResult(int decision, float p1, float pavg, float pps, uint32_t winIndex, uint32_t computeMs)
{
    bool radarLive = sMgr.LineSeen();
    PLAT_LOG("CSI_WIN t=%lu win=%lu cls=%d p1=%d pavg=%d pps=%d ms=%lu src=%s", (unsigned long) NowMs(), (unsigned long) winIndex,
             decision, (int) (p1 * 1000), (int) (pavg * 1000), (int) pps, (unsigned long) computeMs, radarLive ? "radar" : "wifi");
    if (!radarLive)
    {
        PlatformMgr().LockChipStack();
        SetOccupied(decision != 0);
        PlatformMgr().UnlockChipStack();
    }
}
#endif

