// Fox Red Labs: attribute change hook for the thermostat example (replaces the lighting one).
#include "AppConfig.h"
#include "ThermostatManager.h"

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
    ClusterId clusterId     = attributePath.mClusterId;
    AttributeId attributeId = attributePath.mAttributeId;
    if (clusterId == Thermostat::Id)
    {
        if (attributeId == Thermostat::Attributes::OccupiedHeatingSetpoint::Id ||
            attributeId == Thermostat::Attributes::OccupiedCoolingSetpoint::Id)
        {
            int16_t v = (size >= 2) ? (int16_t) (value[0] | (value[1] << 8)) : 0;
            ThermoMgr().OnControllerWrite(attributeId, v);
        }
        else if (attributeId == Thermostat::Attributes::SystemMode::Id)
        {
            ThermoMgr().OnControllerWrite(attributeId, (int16_t) value[0]);
        }
    }
    else if (clusterId == Identify::Id)
    {
        AppEvent event;
        event.Type = AppEvent::kEventType_Identify;
        GetAppTask().PostEvent(&event);
    }
}
