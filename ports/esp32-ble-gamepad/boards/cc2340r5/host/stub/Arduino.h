#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <string>
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
