/* Tool tag: UART lines from the tool MCU in, two advertising sets out. See tooltag.c. */
#ifndef TOOLTAG_H
#define TOOLTAG_H

#include "ti/ble/stack_util/bcomdef.h"

/* Call once from App_StackInitDoneHandler, after Peripheral_start(). */
bStatus_t ToolTag_start(void);

#endif
