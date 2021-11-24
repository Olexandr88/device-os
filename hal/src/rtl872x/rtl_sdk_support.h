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

#ifndef RTL_SDK_SUPPORT_H
#define RTL_SDK_SUPPORT_H

#include "stdint.h"

#define BOOTLOADER_UPDATE_IPC_CHANNEL       0

typedef void (*rtl_ipc_callback_t)(void *data, uint32_t irq_status, uint32_t channel);

#ifdef __cplusplus
extern "C" {
#endif

int ipc_channel_init(uint8_t channel, rtl_ipc_callback_t callback);
void ipc_send_message(uint8_t channel, uint32_t message);
uint32_t ipc_get_message(uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif // RTL_SDK_SUPPORT_H
