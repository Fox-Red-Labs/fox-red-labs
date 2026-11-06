/* CRC-32 (polynomial 0x04C11DB7, seed 0xFFFFFFFF, words fed as is) on the MSPM0L1306's CRC block. */
#include "demo_crc.h"
uint32_t CRC_32_CCITT(uint32_t *DataTab, uint32_t length)
{
    DL_CRC_reset(CRC); DL_CRC_enablePower(CRC); delay_cycles(16);
    DL_CRC_init(CRC, DL_CRC_32_POLYNOMIAL, DL_CRC_BIT_NOT_REVERSED, DL_CRC_INPUT_ENDIANESS_LITTLE_ENDIAN, DL_CRC_OUTPUT_BYTESWAP_DISABLED);
    DL_CRC_setSeed32(CRC, 0xFFFFFFFFu);
    return DL_CRC_calculateBlock32(CRC, 0xFFFFFFFFu, DataTab, length);
}
