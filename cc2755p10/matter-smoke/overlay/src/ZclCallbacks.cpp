// Fox Red Labs: data-model callbacks for the smoke/CO build (TI's lighting version handled OnOff).
#include "AppConfig.h"
#include "LightingManager.h"

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace ::chip;
using namespace ::chip::app::Clusters;

void MatterPostAttributeChangeCallback(const chip::app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    ChipLogProgress(Zcl, "Cluster callback: " ChipLogFormatMEI, ChipLogValueMEI(attributePath.mClusterId));
    if (attributePath.mClusterId == Identify::Id)
    {
        LightMgr().IdentifyEventHandler();
    }
}
