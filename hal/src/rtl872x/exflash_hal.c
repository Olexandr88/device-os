/*
 * Copyright (c) 2020 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHAN'TABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "exflash_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/param.h>
#include "hw_config.h"
#include "logging.h"
#include "flash_common.h"
#include "concurrent_hal.h"
#include "system_error.h"
#include "rtl8721d.h"

static bool is_block_erased(uintptr_t addr, size_t size);

__attribute__((section(".ram.text"), noinline))
static int perform_write(uintptr_t addr, const uint8_t* data, size_t size) {
    // XXX: No way of knowing whether the write operation succeeded or not
    for (size_t b = 0; b < size;) {
        size_t rem = MIN(8, (size - b));
        // XXX: do not use 12 byte writes, sometimes we get deadlocked
        // TxData256 doesn't seem to work
        FLASH_TxData12B(addr + b, (uint8_t)rem, (uint8_t*)data + b);
        b += rem;
    }
    return 0;
}

int hal_exflash_init(void) {
    return 0;
}

int hal_exflash_uninit(void) {
    return 0;
}

__attribute__((section(".ram.text"), noinline))
int hal_exflash_write(uintptr_t addr, const uint8_t* data_buf, size_t data_size) {
    hal_exflash_lock();
    int ret = hal_flash_common_write(addr, data_buf, data_size,
                                     &perform_write, &hal_flash_common_dummy_read);
    DCache_CleanInvalidate(SPI_FLASH_BASE + addr, data_size);
    hal_exflash_unlock();
    return ret;
}

__attribute__((section(".ram.text"), noinline))
int hal_exflash_read(uintptr_t addr, uint8_t* data_buf, size_t data_size) {
    int ret = 0;
    addr += SPI_FLASH_BASE;
    hal_exflash_lock();
    memcpy(data_buf, (void*)addr, data_size);
    hal_exflash_unlock();
    return ret;
}

__attribute__((section(".ram.text"), noinline))
static bool is_block_erased(uintptr_t addr, size_t size) {
    uint8_t* ptr = (uint8_t*)(SPI_FLASH_BASE + addr);
    for (size_t i = 0; i < size; i++) {
        if (ptr[i] != 0xff) {
            return false;
        }
    }
    return true;
}

__attribute__((section(".ram.text"), noinline))
static int erase_common(uintptr_t start_addr, size_t num_blocks, int len) {
    hal_exflash_lock();
    int err_code = 0;

    const size_t block_length = len == EraseSector ? 4096 : 64 * 1024;

    /* Round down to the nearest block */
    start_addr = ((start_addr / block_length) * block_length);

    for (size_t i = 0; i < num_blocks; i++) {
        if (!is_block_erased(start_addr + i * block_length, block_length)) {
            FLASH_Erase(len, start_addr + i * block_length);
        }
    }

    // LOG_DEBUG(TRACE, "Erased %lu %lukB blocks starting from %" PRIxPTR,
    //           num_blocks, block_length / 1024, start_addr);

    DCache_CleanInvalidate(SPI_FLASH_BASE + start_addr, block_length * num_blocks);
    hal_exflash_unlock();
    return err_code;
}

__attribute__((section(".ram.text"), noinline))
int hal_exflash_erase_sector(uintptr_t start_addr, size_t num_sectors) {
    return erase_common(start_addr, num_sectors, EraseSector);
}

__attribute__((section(".ram.text"), noinline))
int hal_exflash_erase_block(uintptr_t start_addr, size_t num_blocks) {
    return erase_common(start_addr, num_blocks, EraseBlock);
}

__attribute__((section(".ram.text"), noinline))
int hal_exflash_copy_sector(uintptr_t src_addr, uintptr_t dest_addr, size_t data_size) {
    hal_exflash_lock();
    int ret = -1;
    if ((src_addr % sFLASH_PAGESIZE) ||
        (dest_addr % sFLASH_PAGESIZE) ||
        !(IS_WORD_ALIGNED(data_size))) {
        goto hal_exflash_copy_sector_done;
    }

    // erase sectors
    uint16_t sector_num = CEIL_DIV(data_size, sFLASH_PAGESIZE);
    if (hal_exflash_erase_sector(dest_addr, sector_num)) {
        goto hal_exflash_copy_sector_done;
    }

    // memory copy
    uint8_t data_buf[FLASH_OPERATION_TEMP_BLOCK_SIZE] __attribute__((aligned(4)));
    unsigned index = 0;
    uint16_t copy_len;

    for (index = 0; index < data_size; index += copy_len) {
        copy_len = MIN((data_size - index), sizeof(data_buf));
        if (hal_exflash_read(src_addr + index, data_buf, copy_len)) {
            goto hal_exflash_copy_sector_done;
        }
        if (hal_exflash_write(dest_addr + index, data_buf, copy_len)) {
            goto hal_exflash_copy_sector_done;
        }
    }

    ret = 0;

hal_exflash_copy_sector_done:
    hal_exflash_unlock();
    return ret;
}

int hal_exflash_read_special(hal_exflash_special_sector_t sp, uintptr_t addr, uint8_t* data_buf, size_t data_size) {
    return SYSTEM_ERROR_NOT_SUPPORTED;
}

int hal_exflash_write_special(hal_exflash_special_sector_t sp, uintptr_t addr, const uint8_t* data_buf, size_t data_size) {
    return SYSTEM_ERROR_NOT_SUPPORTED;
}

int hal_exflash_erase_special(hal_exflash_special_sector_t sp, uintptr_t addr, size_t size) {
    return SYSTEM_ERROR_NOT_SUPPORTED;
}

int hal_exflash_special_command(hal_exflash_special_sector_t sp, hal_exflash_command_t cmd, const uint8_t* data, uint8_t* result, size_t size) {
    return SYSTEM_ERROR_NOT_SUPPORTED;
}

int hal_exflash_sleep(bool sleep, void* reserved) {
    return SYSTEM_ERROR_NOT_SUPPORTED;
}
