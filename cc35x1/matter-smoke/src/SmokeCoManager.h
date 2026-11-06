// Fox Red Labs: Smoke CO Alarm state for the LP-EM-CC35X1 Matter Smoke CO Alarm example (device type 0x0076, cluster 0x005C).
// Reads "SMOKE 1", "SMOKE 0", "CO 1", "CO 0" lines from the console UART (CONFIG_UART2_0, 115200 8N1, the XDS110 back channel,
// /dev/cu.usbmodemE10000FQ1 on the Mac) and writes SmokeState, COState and ExpressedState on endpoint 1.
// SW1 toggles smoke and SW2 toggles CO, so the board works with no listener attached.
#pragma once

#include <stdint.h>

class SmokeCoManager
{
public:
    int Init();
    void SetSmoke(bool on, const char * src);
    void SetCO(bool on, const char * src);
    void ToggleSmoke() { SetSmoke(!mSmoke, "button"); }
    void ToggleCO() { SetCO(!mCO, "button"); }
    void OnSelfTest(); // Matter thread, stack already locked

private:
    friend void SmokeCoUartTask(void *);
    void ParseLine(char * line);
    void Apply(const char * src);
    volatile bool mSmoke = false;
    volatile bool mCO    = false;
};

SmokeCoManager & SmokeCoMgr();
extern "C++" void SmokeCoBootMarker(void);
