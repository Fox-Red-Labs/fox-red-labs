// Fox Red Labs: attribute change hook for the Smoke CO Alarm example (replaces the lighting one). Identify blinks the green LED;
// alarm attributes are written by SmokeCoManager and only logged here, with the board tick clock, for the latency table.
#include "AppConfig.h"
#include "SmokeCoManager.h"

#include <FreeRTOS.h>
#include <task.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/clusters/identify-server/identify-server.h>
#include <lib/support/logging/CHIPLogging.h>

#include "AppTask.h"

using namespace ::chip;
using namespace ::chip::app::Clusters;

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    if (attributePath.mClusterId == SmokeCoAlarm::Id)
    {
        PLAT_LOG("ATTR t=%lu smokeco attr=0x%lx value=%u", (unsigned long) (xTaskGetTickCount() * portTICK_PERIOD_MS),
                 (unsigned long) attributePath.mAttributeId, (unsigned) value[0]);
    }
    else if (attributePath.mClusterId == Identify::Id)
    {
        AppEvent event;
        event.Type = AppEvent::kEventType_Identify;
        GetAppTask().PostEvent(&event);
    }
}
