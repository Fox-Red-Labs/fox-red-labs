// Fox Red Labs: Smoke CO Alarm glue for the CC27xx build. SmokeAlarmSet may be called from any task.
#pragma once

enum class SmokeAlarmKind { kSmoke, kCO };

void SmokeAlarmSet(SmokeAlarmKind kind, bool on);
void SmokeAlarmInit();    // once, after Server::Init (takes the stack lock itself)
int SmokeUartStart();     // task reading `SMOKE n` / `CO n` lines on the XDS110 backchannel UART at 115200
