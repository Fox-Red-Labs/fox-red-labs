// Fox Red Labs: replaces TI's LightingManager for the smoke/CO build. The class keeps TI's names so TI's AppTask.cpp
// compiles unchanged; the "light" is now the alarm: left button = smoke critical, right button = clear both.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "AppEvent.h"
#include "FreeRTOS.h"
#include "timers.h"
#include <lib/core/CHIPError.h>

class LightingManager
{
public:
    enum Action_t { ON_ACTION = 0, OFF_ACTION, IDENTIFY_ACTION, INVALID_ACTION } Action;
    enum State_t { kState_OffInitiated = 0, kState_OffCompleted, kState_OnInitiated, kState_OnCompleted } State;

    CHIP_ERROR Init();
    bool IsLightOn();
    bool IsActionInProgress();
    bool InitiateAction(int32_t aActor, Action_t aAction);

    typedef void (*Callback_fn_initiated)(Action_t, int32_t aActor);
    typedef void (*Callback_fn_completed)(Action_t);
    void SetCallbacks(Callback_fn_initiated aActionInitiated_CB, Callback_fn_completed aActionCompleted_CB);
    void IdentifyEventHandler();

private:
    friend LightingManager & LightMgr(void);
    State_t mState = kState_OffCompleted;
    Callback_fn_initiated mActionInitiated_CB = nullptr;
    Callback_fn_completed mActionCompleted_CB = nullptr;
    static LightingManager sLight;
};

inline LightingManager & LightMgr(void)
{
    return LightingManager::sLight;
}
