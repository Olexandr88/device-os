/**
 ******************************************************************************
 * @file    flash_mal.h
 * @author  Satish Nair
 * @version V1.0.0
 * @date    30-Jan-2015
 * @brief   Header for flash media access layer
 ******************************************************************************
  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, see <http://www.gnu.org/licenses/>.
  ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __FLASH_MAL_H
#define __FLASH_MAL_H

#include "hw_config.h"
#include "flash_device_hal.h"
#include "flash_hal.h"
#include "module_info.h"
#include "module_info_hal.h"

/* Includes ------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/* Exported types ------------------------------------------------------------*/

/* Exported constants --------------------------------------------------------*/

/* Exported macros ------------------------------------------------------------*/
#ifndef INTERNAL_FLASH_SIZE
#   error "INTERNAL_FLASH_SIZE not defined"
#endif

/* Internal Flash memory address where various firmwares are located */
#ifndef INTERNAL_FLASH_START
#define INTERNAL_FLASH_START            ((uint32_t)0x08000000)
#endif

//Bootloader firmware at the start of internal flash
#define USB_DFU_ADDRESS                 INTERNAL_FLASH_START

//Main firmware begin address after 24 + 64KB from start of flash
#define CORE_FW_ADDRESS                 ((uint32_t)0x08060000)
#define KM0_MBR_START_ADDRESS           ((uint32_t)0x08000000)
#define KM0_PART1_START_ADDRESS         ((uint32_t)0x08014000)
#define BOOT_INFO_FLASH_XIP_START_ADDR  ((uint32_t)0x0805F000)

#define KM0_MBR_IMAGE_SIZE              (0x2000)
#define KM0_PART1_IMAGE_SIZE            (0x4B000)

/* Internal Flash page size */
#define INTERNAL_FLASH_PAGE_SIZE        ((uint32_t)sFLASH_PAGESIZE) //4K (256 sectors of 4K each used by main firmware)

// Byte 0 of the user key 0 will hold flags used for encrypted part1 status
#define USER_KEY_0_EFUSE_ADDRESS 0x130
// Bit 0 of byte 0 denotes if part1 is encrypted or not (set = not encrypted, cleared = is encrypted)
#define PART1_ENCRYPTED_BIT (BIT(0))

#ifdef USE_SERIAL_FLASH
#define EXTERNAL_FLASH_SIZE             (sFLASH_PAGESIZE * sFLASH_PAGECOUNT)
#define EXTERNAL_FLASH_XIP_BASE         (INTERNAL_FLASH_START)
#endif

#include "flash_access.h"

#ifdef __cplusplus
}
#endif

#endif  /*__FLASH_MAL_H*/
