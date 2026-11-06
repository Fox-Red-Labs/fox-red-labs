// Fox Red Labs: thermostat state for the LP-EM-CC35X1 Matter thermostat example.
// Reads "T,seq,presence,zones,setpoint,temp" lines from UART2 (CONFIG_UART2_1, 115200 8N1, RX on LaunchPad header P1 pin 3)
// and writes the Thermostat and Occupancy Sensing clusters. Without lines it keeps 20.8 C and steps the setpoint with SW1/SW2.
#pragma once

#include <stdint.h>

class ThermostatManager
{
public:
    int Init();
    // Button path (any task): step the active setpoint by deltaCentiC (500 = 0.5 C).
    void StepSetpoint(int16_t deltaCentiC);
    // Called from MatterPostAttributeChangeCallback (Matter thread) when a controller wrote an attribute.
    void OnControllerWrite(uint32_t attributeId, int16_t value);
    bool LineSeen() const { return mLineSeen; }

private:
    friend void ThermostatUartTask(void *);
    friend void ThermostatWatchdog(void *);
    void ParseLine(char * line);
    void ApplyFromLine(int presence, int setpointTenths, int tempTenths);
    void SendToPeer();
    volatile bool mSuppress = false;
    volatile bool mLineSeen = false;
    volatile uint32_t mLastLineTick = 0;
};

ThermostatManager & ThermoMgr();
extern "C++" void ThermostatBootMarker(void);
