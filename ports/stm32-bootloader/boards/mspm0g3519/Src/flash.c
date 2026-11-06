/**
 * @file    flash.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module handles the memory related functions.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "flash.h"
#include "uart.h"
#include <string.h>

/* Function pointer for jumping to user application. */
typedef void (*fnc_ptr)(void);

/**
 * @brief   This function erases the memory.
 * @param   address: First address to be erased (the last is the end of the flash).
 * @return  status: Report about the success of the erasing.
 */
flash_status flash_erase(uint32_t address)
{
  flash_status status = FLASH_OK;

  /* Never touch the bootloader, and only whole 1 kB sectors. */
  if ((address < FLASH_APP_START_ADDRESS) || (0u != (address % FLASH_SECTOR_SIZE)))
  {
    return FLASH_ERROR;
  }

  /* The erase runs from RAM; keep interrupts off so nothing fetches from flash meanwhile. */
  __disable_irq();
  for (; (address <= FLASH_BANK1_END) && (FLASH_OK == status); address += FLASH_SECTOR_SIZE)
  {
    DL_FlashCTL_unprotectSector(FLASHCTL, address, DL_FLASHCTL_REGION_SELECT_MAIN);
    if (DL_FLASHCTL_COMMAND_STATUS_PASSED != DL_FlashCTL_eraseMemoryFromRAM(FLASHCTL, address, DL_FLASHCTL_COMMAND_SIZE_SECTOR))
    {
      status = FLASH_ERROR;
    }
  }
  __enable_irq();

  return status;
}

/**
 * @brief   This function flashes the memory.
 * @param   address: First address to be written to.
 * @param   *data:   Array of the data that we want to write.
 * @param   *length: Size of the array (in 32 bit words).
 * @return  status: Report about the success of the writing.
 */
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length)
{
  flash_status status = FLASH_OK;

  /* The flash is programmed in 8 byte words, only once each, at 8 byte aligned addresses. */
  if ((0u != (address % 8u)) || (0u != (length % 2u)))
  {
    return FLASH_ERROR;
  }

  /* Loop through the array, two words at a time. */
  for (uint32_t i = 0u; (i < length) && (FLASH_OK == status); i += 2u)
  {
    /* If we reached the end of the memory, then report an error and don't do anything else.*/
    if (FLASH_APP_END_ADDRESS <= address)
    {
      status |= FLASH_ERROR_SIZE;
    }
    else
    {
      /* The packet buffer in the caller is only byte aligned, so copy it to an aligned pair. */
      uint32_t pair[2];
      (void)memcpy(pair, &((uint8_t*)data)[i * 4u], sizeof(pair));

      /* The actual flashing. If there is an error, then report it. */
      __disable_irq();
      DL_FlashCTL_unprotectSector(FLASHCTL, address, DL_FLASHCTL_REGION_SELECT_MAIN);
      if (DL_FLASHCTL_COMMAND_STATUS_PASSED != DL_FlashCTL_programMemoryFromRAM64WithECCGenerated(FLASHCTL, address, pair))
      {
        status |= FLASH_ERROR_WRITE;
      }
      __enable_irq();
      /* Read back the content of the memory. If it is wrong, then report an error. */
      if ((pair[0] != (*(volatile uint32_t*)address)) || (pair[1] != (*(volatile uint32_t*)(address + 4u))))
      {
        status |= FLASH_ERROR_READBACK;
      }

      /* Shift the address by a flash word. */
      address += 8u;
    }
  }

  return status;
}

/**
 * @brief   Actually jumps to the user application.
 * @param   void
 * @return  void
 */
void flash_jump_to_app(void)
{
  /* Function pointer to the address of the user application. */
  fnc_ptr jump_to_app;
  jump_to_app = (fnc_ptr)(*(volatile uint32_t*) (FLASH_APP_START_ADDRESS+4u));
  __disable_irq();
  uart_deinit();
  NVIC->ICER[0u] = 0xFFFFFFFFu;
  NVIC->ICPR[0u] = 0xFFFFFFFFu;
  /* The application has its own vector table. */
  SCB->VTOR = FLASH_APP_START_ADDRESS;
  /* Change the main stack pointer. */
  __set_MSP(*(volatile uint32_t*)FLASH_APP_START_ADDRESS);
  __enable_irq();
  jump_to_app();
}
