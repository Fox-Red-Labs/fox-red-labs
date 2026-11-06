// Fox Red Labs: Smoke CO Alarm glue for the CC27xx build.
#include "SmokeAlarm.h"
#include "AppConfig.h"

#include <FreeRTOS.h>
#include <task.h>

#include <app/clusters/smoke-co-alarm-server/smoke-co-alarm-server.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/CommandHandler.h>
#include <platform/CHIPDeviceLayer.h>

#include <platform/ti/cc27xx/Logging.h>
#include <ti_drivers_config.h>

#include <string.h>
#include <sys/types.h>

using namespace chip;
using namespace chip::app::Clusters::SmokeCoAlarm;
using namespace chip::DeviceLayer;

static constexpr EndpointId kEp = 1;

static std::array<ExpressedStateEnum, SmokeCoAlarmServer::kPriorityOrderLength> sPriorityOrder = {
    ExpressedStateEnum::kSmokeAlarm,     ExpressedStateEnum::kInterconnectSmoke, ExpressedStateEnum::kCOAlarm,
    ExpressedStateEnum::kInterconnectCO, ExpressedStateEnum::kHardwareFault,     ExpressedStateEnum::kTesting,
    ExpressedStateEnum::kEndOfService,   ExpressedStateEnum::kBatteryAlert
};

void SmokeAlarmInit()
{
    PlatformMgr().LockChipStack();
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kEp, sPriorityOrder);
    PlatformMgr().UnlockChipStack();
}

void SmokeAlarmSet(SmokeAlarmKind kind, bool on)
{
    AlarmStateEnum st = on ? AlarmStateEnum::kCritical : AlarmStateEnum::kNormal;
    PlatformMgr().LockChipStack();
    if (kind == SmokeAlarmKind::kSmoke)
        SmokeCoAlarmServer::Instance().SetSmokeState(kEp, st);
    else
        SmokeCoAlarmServer::Instance().SetCOState(kEp, st);
    SmokeCoAlarmServer::Instance().SetExpressedStateByPriority(kEp, sPriorityOrder);
    PlatformMgr().UnlockChipStack();
    PLAT_LOG("alarm %s %d", kind == SmokeAlarmKind::kSmoke ? "SMOKE" : "CO", on);
}

// The data model's self-test command: report "testing" for 3 s is not worth a timer here, so acknowledge only.
void emberAfPluginSmokeCoAlarmSelfTestRequestCommand(EndpointId) {}
void emberAfSmokeCoAlarmClusterInitCallback(EndpointId) {}

// The shared smoke data model carries the ICD Management cluster. Without the ICD server (router build) its two commands
// have no handler, so answer them as unsupported.
#if !CHIP_CONFIG_ENABLE_ICD_CIP
bool emberAfIcdManagementClusterRegisterClientCallback(app::CommandHandler * c, const app::ConcreteCommandPath & p,
                                                       const app::Clusters::IcdManagement::Commands::RegisterClient::DecodableType &)
{
    c->AddStatus(p, Protocols::InteractionModel::Status::UnsupportedCommand);
    return true;
}
bool emberAfIcdManagementClusterUnregisterClientCallback(app::CommandHandler * c, const app::ConcreteCommandPath & p,
                                                         const app::Clusters::IcdManagement::Commands::UnregisterClient::DecodableType &)
{
    c->AddStatus(p, Protocols::InteractionModel::Status::UnsupportedCommand);
    return true;
}
#endif

// ---- UART line input -----------------------------------------------------------------------------------------
// TI's platform logger already owns the XDS110 backchannel UART (115200 8N1, opened in cc27xxLogInit), so reading goes
// through its console helpers instead of a second UART2_open (which returns NULL).

volatile uint32_t gFrlUartLines = 0; // lines seen, readable over the debug probe

static void Reply(const char * msg)
{
    for (int i = 0; i < 5; i++) // the logger may be mid-write; the driver then refuses a second writer
    {
        if (uartConsoleWrite(msg, strlen(msg)) > 0) return;
        vTaskDelay(2);
    }
}

static void UartTask(void *)
{
    char line[16];
    size_t n = 0;
    for (;;)
    {
        char c;
        if (uartConsoleRead(&c, 1) != 1) { vTaskDelay(1); continue; }
        if (c == '\r' || c == '\n')
        {
            line[n] = 0;
            if (n >= 3)
            {
                gFrlUartLines++;
                bool on = line[n - 1] == '1';
                bool ok = true;
                if (!strncmp(line, "SMOKE ", 6) && n == 7) SmokeAlarmSet(SmokeAlarmKind::kSmoke, on);
                else if (!strncmp(line, "CO ", 3) && n == 4) SmokeAlarmSet(SmokeAlarmKind::kCO, on);
                else ok = false;
                // reply once the cluster attribute is set (reports to subscribers follow on the Thread side)
                Reply(ok ? "FRL OK\r\n" : "FRL ?\r\n");
            }
            n = 0;
        }
        else if (n < sizeof(line) - 1)
        {
            line[n++] = c;
        }
        else
        {
            n = 0;
        }
    }
}

int SmokeUartStart()
{
    return xTaskCreate(UartTask, "UART", 768, NULL, 3, NULL) == pdPASS ? 0 : -1;
}
