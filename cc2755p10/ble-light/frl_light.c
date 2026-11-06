/******************************************************************************

 @file  frl_light.c

 @brief Bluetooth light service for the basic_ble example: one custom GATT
        service (colour, pattern, status) that drives the LaunchPad kit's user LEDs.

        Derived from TI's simple_gatt_profile.c and app_simple_gatt.c in the
        SimpleLink Low Power F3 SDK. Changes by Fox Red Labs; TI's notice stays.

 ******************************************************************************
 
 Copyright (c) 2010-2026, Texas Instruments Incorporated
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

 *  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

 *  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 *  Neither the name of Texas Instruments Incorporated nor the names of
    its contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include "ti/ble/stack_util/icall/app/icall.h"
#include "ti/ble/stack_util/icall/app/icall_ble_api.h"
#include "ti/ble/app_util/framework/bleapputil_api.h"
#include "ti/ble/app_util/framework/bleapputil_timers.h"
#include <ti/drivers/GPIO.h>
#include "ti_drivers_config.h"
#include "frl_light.h"

/* Service 8a570001-4724-4979-ab46-406d217495e5; characteristics differ in the first 16 bits. Wire order is little endian. */
#define FRL_UUID(n) { 0xe5, 0x95, 0x74, 0x21, 0x6d, 0x40, 0x46, 0xab, 0x79, 0x49, 0x24, 0x47, (n), 0x00, 0x57, 0x8a }

#define FRL_PATTERN_STEP_BYTES  5
#define FRL_PATTERN_MAX_STEPS   16
#define FRL_PATTERN_MAX_BYTES   (FRL_PATTERN_STEP_BYTES * FRL_PATTERN_MAX_STEPS)
#define FRL_STATUS_MAX          24
#define FRL_MIN_STEP_MS         20

static const uint8 frlServUUID[ATT_UUID_SIZE]    = FRL_UUID(1);
static const uint8 frlColourUUID[ATT_UUID_SIZE]  = FRL_UUID(2);
static const uint8 frlPatternUUID[ATT_UUID_SIZE] = FRL_UUID(3);
static const uint8 frlStatusUUID[ATT_UUID_SIZE]  = FRL_UUID(4);

static const gattAttrType_t frlService = { ATT_UUID_SIZE, frlServUUID };

static uint8 frlColourProps  = GATT_PROP_READ | GATT_PROP_WRITE;
static uint8 frlPatternProps = GATT_PROP_WRITE;
static uint8 frlStatusProps  = GATT_PROP_READ | GATT_PROP_NOTIFY;

/* Values. The stack writes colour and pattern from its own context; the LEDs are only touched from the BLEAppUtil context. */
static uint8 frlColour[3] = { 0, 255, 0 };               /* R, G, B; green until a page says otherwise */
static uint8 frlPattern[FRL_PATTERN_MAX_BYTES];
static uint16 frlPatternBytes = 0;
static char frlStatus[FRL_STATUS_MAX] = "up=0s solid";
static gattCharCfg_t *frlStatusConfig;

static gattAttribute_t frlAttrTbl[] =
{
  GATT_BT_ATT( primaryServiceUUID, GATT_PERMIT_READ,                     (uint8 *) &frlService ),

  GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,                     &frlColourProps ),
  GATT_ATT(    frlColourUUID,      GATT_PERMIT_READ | GATT_PERMIT_WRITE, frlColour ),

  GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,                     &frlPatternProps ),
  GATT_ATT(    frlPatternUUID,     GATT_PERMIT_WRITE,                    frlPattern ),

  GATT_BT_ATT( characterUUID,      GATT_PERMIT_READ,                     &frlStatusProps ),
  GATT_ATT(    frlStatusUUID,      GATT_PERMIT_READ,                     (uint8 *) frlStatus ),
  GATT_BT_ATT( clientCharCfgUUID,  GATT_PERMIT_READ | GATT_PERMIT_WRITE, (uint8 *) &frlStatusConfig ),
};

static bStatus_t frlReadCB(uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t *pLen,
                           uint16_t offset, uint16_t maxLen, uint8_t method);
static bStatus_t frlWriteCB(uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t len,
                            uint16_t offset, uint8_t method);

static const gattServiceCBs_t frlCBs = { frlReadCB, frlWriteCB, NULL };

/* Player state, BLEAppUtil context only */
static uint8_t  playSteps = 0;                           /* 0 means solid colour */
static uint8_t  playIdx = 0;
static BLEAppUtil_timerHandle stepTimer = BLEAPPUTIL_TIMER_INVALID_HANDLE;
static BLEAppUtil_timerHandle statusTimer = BLEAPPUTIL_TIMER_INVALID_HANDLE;
static uint32_t uptimeS = 0;

static void setLeds(uint8_t r, uint8_t g)
{
  GPIO_write(CONFIG_GPIO_LED_RED,   r >= 128 ? CONFIG_LED_ON : CONFIG_LED_OFF);
  GPIO_write(CONFIG_GPIO_LED_GREEN, g >= 128 ? CONFIG_LED_ON : CONFIG_LED_OFF);
}

static void stepTimerCB(BLEAppUtil_timerHandle h, BLEAppUtil_timerTermReason_e reason, void *pData);

static void showStep(void)
{
  const uint8_t *s = &frlPattern[playIdx * FRL_PATTERN_STEP_BYTES];
  uint16_t ms = (uint16_t)(s[3] | (s[4] << 8));
  setLeds(s[0], s[1]);
  if (ms < FRL_MIN_STEP_MS) ms = FRL_MIN_STEP_MS;
  stepTimer = BLEAppUtil_startTimer(stepTimerCB, ms, false, NULL);
}

static void stepTimerCB(BLEAppUtil_timerHandle h, BLEAppUtil_timerTermReason_e reason, void *pData)
{
  (void) pData;
  if (reason != BLEAPPUTIL_TIMER_TIMEOUT || h != stepTimer || playSteps == 0) return;
  stepTimer = BLEAPPUTIL_TIMER_INVALID_HANDLE;
  playIdx = (uint8_t)((playIdx + 1) % playSteps);
  showStep();
}

/* Re-reads colour and pattern and starts playing. Runs in the BLEAppUtil context. */
static void applyAll(void)
{
  uint16_t bytes = frlPatternBytes;
  if (stepTimer != BLEAPPUTIL_TIMER_INVALID_HANDLE)
  {
    BLEAppUtil_timerHandle h = stepTimer;
    stepTimer = BLEAPPUTIL_TIMER_INVALID_HANDLE;
    BLEAppUtil_abortTimer(h);
  }
  playSteps = (uint8_t)(bytes / FRL_PATTERN_STEP_BYTES);
  playIdx = 0;
  if (playSteps == 0)
  {
    setLeds(frlColour[0], frlColour[1]);
  }
  else
  {
    showStep();
  }
}

static void applyFromFW(char *pData) { (void) pData; applyAll(); }

static void statusTick(BLEAppUtil_timerHandle h, BLEAppUtil_timerTermReason_e reason, void *pData)
{
  (void) h; (void) pData;
  if (reason != BLEAPPUTIL_TIMER_TIMEOUT) return;
  uptimeS++;
  if (playSteps == 0)
    snprintf(frlStatus, sizeof(frlStatus), "up=%lus solid", (unsigned long) uptimeS);
  else
    snprintf(frlStatus, sizeof(frlStatus), "up=%lus step=%u/%u", (unsigned long) uptimeS, playIdx + 1, playSteps);
  GATTServApp_ProcessCharCfg(frlStatusConfig, (uint8 *) frlStatus, FALSE, frlAttrTbl, GATT_NUM_ATTRS(frlAttrTbl),
                             INVALID_TASK_ID, frlReadCB);
}

static bStatus_t frlReadCB(uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t *pLen,
                           uint16_t offset, uint16_t maxLen, uint8_t method)
{
  const uint8_t *src;
  uint16_t total;

  if (pAttr->type.len != ATT_UUID_SIZE)
  {
    *pLen = 0;
    return ATT_ERR_ATTR_NOT_FOUND;
  }
  if (pAttr->pValue == frlColour)
  {
    src = frlColour; total = sizeof(frlColour);
  }
  else if (pAttr->pValue == (uint8 *) frlStatus)
  {
    src = (const uint8_t *) frlStatus; total = (uint16_t) strlen(frlStatus);
  }
  else
  {
    *pLen = 0;
    return ATT_ERR_ATTR_NOT_FOUND;
  }
  if (offset > total) return ATT_ERR_INVALID_OFFSET;
  *pLen = (uint16_t)((total - offset) < maxLen ? (total - offset) : maxLen);
  memcpy(pValue, src + offset, *pLen);
  return SUCCESS;
}

static bStatus_t frlWriteCB(uint16_t connHandle, gattAttribute_t *pAttr, uint8_t *pValue, uint16_t len,
                            uint16_t offset, uint8_t method)
{
  if (pAttr->type.len == ATT_BT_UUID_SIZE &&
      BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]) == GATT_CLIENT_CHAR_CFG_UUID)
  {
    return GATTServApp_ProcessCCCWriteReq(connHandle, pAttr, pValue, len, offset, GATT_CLIENT_CFG_NOTIFY);
  }
  if (pAttr->type.len != ATT_UUID_SIZE) return ATT_ERR_ATTR_NOT_FOUND;

  if (pAttr->pValue == frlColour)
  {
    if (offset != 0) return ATT_ERR_ATTR_NOT_LONG;
    if (len != sizeof(frlColour)) return ATT_ERR_INVALID_VALUE_SIZE;
    memcpy(frlColour, pValue, sizeof(frlColour));
    frlPatternBytes = 0;                                 /* a colour write ends any pattern */
  }
  else if (pAttr->pValue == frlPattern)
  {
    /* A long write arrives as chunks at rising offsets; the pattern is whole steps of the bytes seen so far. */
    if (offset == 0) frlPatternBytes = 0;
    if (offset != frlPatternBytes) return ATT_ERR_INVALID_OFFSET;
    if (offset + len > FRL_PATTERN_MAX_BYTES) return ATT_ERR_INVALID_VALUE_SIZE;
    memcpy(&frlPattern[offset], pValue, len);
    frlPatternBytes = (uint16_t)(offset + len);
  }
  else
  {
    return ATT_ERR_ATTR_NOT_FOUND;
  }
  BLEAppUtil_invokeFunction(applyFromFW, NULL);
  return SUCCESS;
}

bStatus_t FrlLight_start(void)
{
  bStatus_t status;

  frlStatusConfig = (gattCharCfg_t *) ICall_malloc(sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
  if (frlStatusConfig == NULL) return bleMemAllocError;
  GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, frlStatusConfig);

  status = GATTServApp_RegisterService(frlAttrTbl, GATT_NUM_ATTRS(frlAttrTbl), GATT_MAX_ENCRYPT_KEY_SIZE, &frlCBs);
  if (status != SUCCESS) return status;

  applyAll();
  statusTimer = BLEAppUtil_startTimer(statusTick, 1000, true, NULL);
  return SUCCESS;
}
