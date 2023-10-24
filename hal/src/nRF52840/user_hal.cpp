/*
 * Copyright (c) 2021 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "hal_platform.h"
#include "user_hal.h"
#include "system_error.h"
#include "ota_module.h"
#include "flash_mal.h"

#if defined(MODULAR_FIRMWARE)

namespace {

const uint8_t USER_PART_COMPAT_INDEX = 1;
const uint8_t USER_PART_CURRENT_INDEX = 2;

bool validUserModuleInfoAtIndex(uint8_t function, uint8_t index, module_info_t* info) {
    const auto bounds = find_module_bounds(function, index, HAL_PLATFORM_MCU_DEFAULT);
    if (!bounds) {
        return false;
    }

    if (!FLASH_isUserModuleInfoValid(FLASH_INTERNAL, bounds->start_address, bounds->start_address)) {
        return false;
    }

    if (!FLASH_VerifyCRC32(FLASH_INTERNAL, bounds->start_address, FLASH_ModuleLength(FLASH_INTERNAL, bounds->start_address))) {
        return false;
    }

    if (!validate_module_dependencies(bounds, false, false)) {
        return false;
    }

    return locate_module(bounds, info) == SYSTEM_ERROR_NONE;
}

} // anonymous

#if defined(MODULAR_FIRMWARE)

extern "C" {
void* module_user_pre_init();
void module_user_init();
void module_user_loop();
void module_user_setup();

// Compat 128KB module initialization functions
void* module_user_pre_init_compat();
void module_user_init_compat();
void module_user_loop_compat();
void module_user_setup_compat();
} // extern "C"

#endif // defined(MODULAR_FIRMWARE)

int hal_user_module_get_descriptor(hal_user_module_descriptor* desc) {
#if defined(MODULAR_FIRMWARE)
    module_info_t info = {};
    // Check compat 128KB user application first as it takes precedence
    if (validUserModuleInfoAtIndex(MODULE_FUNCTION_USER_PART, USER_PART_COMPAT_INDEX, &info)) {
        if (desc) {
            desc->info = info;
            desc->pre_init = &module_user_pre_init_compat;
            desc->init = &module_user_init_compat;
            desc->loop = &module_user_loop_compat;
            desc->setup = &module_user_setup_compat;
        }
        return 0;
    }

    if (!validUserModuleInfoAtIndex(MODULE_FUNCTION_USER_PART, USER_PART_CURRENT_INDEX, &info)) {
        return SYSTEM_ERROR_NOT_FOUND;
    }

    if (desc) {
        desc->info = info;
        desc->pre_init = &module_user_pre_init;
        desc->init = &module_user_init;
        desc->loop = &module_user_loop;
        desc->setup = &module_user_setup;
    }
#endif
    return 0;
}

#else

int hal_user_module_get_descriptor(hal_user_module_descriptor* desc) {
    module_info_t info = {};
    if (!validUserModuleInfoAtIndex(MODULE_FUNCTION_MONO_FIRMWARE, 0, &info)) {
        return SYSTEM_ERROR_NOT_FOUND;
    }
    if (desc) {
        desc->info = info;
    }
    return 0;
}

#endif // defined(MODULAR_FIRMWARE)
