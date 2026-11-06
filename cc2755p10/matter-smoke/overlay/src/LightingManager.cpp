// Fox Red Labs: stand-in for TI's LightingManager (see the header).
#include "LightingManager.h"
#include "AppConfig.h"
#include "SmokeAlarm.h"

LightingManager LightingManager::sLight;

CHIP_ERROR LightingManager::Init()
{
    mState = kState_OffCompleted;
    return CHIP_NO_ERROR;
}

void LightingManager::SetCallbacks(Callback_fn_initiated i, Callback_fn_completed c)
{
    mActionInitiated_CB = i;
    mActionCompleted_CB = c;
}

bool LightingManager::IsActionInProgress() { return false; }
bool LightingManager::IsLightOn() { return mState == kState_OnCompleted; }

bool LightingManager::InitiateAction(int32_t aActor, Action_t aAction)
{
    if (aAction == ON_ACTION)
    {
        mState = kState_OnCompleted;
        SmokeAlarmSet(SmokeAlarmKind::kSmoke, true);
    }
    else if (aAction == OFF_ACTION)
    {
        mState = kState_OffCompleted;
        SmokeAlarmSet(SmokeAlarmKind::kSmoke, false);
        SmokeAlarmSet(SmokeAlarmKind::kCO, false);
    }
    else
    {
        return false;
    }
    if (mActionInitiated_CB) mActionInitiated_CB(aAction, aActor);
    if (mActionCompleted_CB) mActionCompleted_CB(aAction);
    return true;
}

void LightingManager::IdentifyEventHandler() {}
