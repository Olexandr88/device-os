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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HAL_PLATFORM_H
#define	HAL_PLATFORM_H

#include "platforms.h"

/* Include platform-specific configuration header */
#include "hal_platform_config.h"

#ifndef PRODUCT_SERIES
#error "PRODUCT_SERIES is not defined!"
#endif

/* Define the defaults */
#ifndef HAL_PLATFORM_WIFI
#define HAL_PLATFORM_WIFI 0
#endif /* HAL_PLATFORM_WIFI */

#ifndef HAL_PLATFORM_WIFI_NCP_SDIO
#define HAL_PLATFORM_WIFI_NCP_SDIO (0)
#endif // HAL_PLATFORM_WIFI_NCP_SDIO

#ifndef HAL_PLATFORM_ESP32_NCP
#define HAL_PLATFORM_ESP32_NCP (0)
#endif // HAL_PLATFORM_ESP32_NCP

#if HAL_PLATFORM_ESP32_NCP
#if HAL_PLATFORM_WIFI && HAL_PLATFORM_NCP_AT && !HAL_PLATFORM_WIFI_NCP_SDIO
# ifndef HAL_PLATFORM_WIFI_SERIAL
#  error "HAL_PLATFORM_WIFI_SERIAL is not defined"
# endif /* HAL_PLATFORM_WIFI_SERIAL */
#endif /* HAL_PLATFORM_WIFI */
#endif /* HAL_PLATFORM_ESP32_NCP */

#ifndef HAL_PLATFORM_CELLULAR
#define HAL_PLATFORM_CELLULAR 0
#endif /* HAL_PLATFORM_CELLULAR */

#if HAL_PLATFORM_CELLULAR && HAL_PLATFORM_NCP_AT
# ifndef HAL_PLATFORM_CELLULAR_SERIAL
#  error "HAL_PLATFORM_CELLULAR_SERIAL is not defined"
# endif /* HAL_PLATFORM_CELLULAR_SERIAL */
#endif /* HAL_PLATFORM_CELLULAR */

#ifndef HAL_PLATFORM_MESH_DEPRECATED
#define HAL_PLATFORM_MESH_DEPRECATED 0
#endif /* HAL_PLATFORM_MESH_DEPRECATED */

#ifndef HAL_PLATFORM_CLOUD_UDP
#define HAL_PLATFORM_CLOUD_UDP 0
#endif /* HAL_PLATFORM_CLOUD_UDP */

#ifndef HAL_PLATFORM_CLOUD_TCP
#define HAL_PLATFORM_CLOUD_TCP 0
#endif /* HAL_PLATFORM_CLOUD_TCP */

#ifndef HAL_PLATFORM_DCT
#define HAL_PLATFORM_DCT 0
#endif /* HAL_PLATFORM_DCT */

#ifndef PANIC_BUT_KEEP_CALM
#define PANIC_BUT_KEEP_CALM 0
#endif /* PANIC_BUT_KEEP_CALM */

#ifndef HAL_USE_SOCKET_HAL_COMPAT
#define HAL_USE_SOCKET_HAL_COMPAT (1)
#endif /* HAL_USE_SOCKET_HAL_COMPAT */

#ifndef HAL_USE_INET_HAL_COMPAT
#define HAL_USE_INET_HAL_COMPAT (1)
#endif /* HAL_USE_INET_HAL_COMPAT */

#ifndef HAL_USE_SOCKET_HAL_POSIX
#define HAL_USE_SOCKET_HAL_POSIX (0)
#endif /* HAL_USE_SOCKET_HAL_POSIX */

#ifndef HAL_USE_INET_HAL_POSIX
#define HAL_USE_INET_HAL_POSIX (0)
#endif /* HAL_USE_INET_HAL_POSIX */

#ifndef HAL_PLATFORM_BLE
#define HAL_PLATFORM_BLE (0)
#endif /* HAL_PLATFORM_BLE */

#if !HAL_PLATFORM_BLE || !defined(HAL_PLATFORM_BLE_SETUP)
#define HAL_PLATFORM_BLE_SETUP (0)
#endif /* HAL_PLATFORM_BLE_SETUP */

#ifndef HAL_PLATFORM_LWIP
#define HAL_PLATFORM_LWIP (0)
#endif /* HAL_PLATFORM_LWIP */

#ifndef HAL_PLATFORM_FILESYSTEM
#define HAL_PLATFORM_FILESYSTEM (0)
#endif /* HAL_PLATFORM_FILESYSTEM */

#ifndef HAL_PLATFORM_IFAPI
#define HAL_PLATFORM_IFAPI (0)
#endif /* HAL_PLATFORM_IFAPI */

#ifndef HAL_PLATFORM_STM32F2XX
#define HAL_PLATFORM_STM32F2XX (0)
#endif // HAL_PLATFORM_STM32F2XX

#ifndef HAL_PLATFORM_NRF52840
#define HAL_PLATFORM_NRF52840 (0)
#endif /* HAL_PLATFORM_NRF52840 */

#ifndef HAL_PLATFORM_RTL872X
#define HAL_PLATFORM_RTL872X (0)
#endif /* HAL_PLATFORM_RTL872X */

#ifndef HAL_PLATFORM_NCP
#define HAL_PLATFORM_NCP (0)
#endif /* HAL_PLATFORM_NCP */

#ifndef HAL_PLATFORM_NCP_AT
#define HAL_PLATFORM_NCP_AT (0)
#endif /* HAL_PLATFORM_NCP_AT */

#ifndef HAL_PLATFORM_NCP_UPDATABLE
#define HAL_PLATFORM_NCP_UPDATABLE (0)
#endif /* HAL_PLATFORM_NCP_UPDATABLE */

#ifndef HAL_PLATFORM_MCU_ANY
#define HAL_PLATFORM_MCU_ANY (0xFF)
#endif // HAL_PLATFORM_MCU_ANY

#ifndef HAL_PLATFORM_MCU_DEFAULT
#define HAL_PLATFORM_MCU_DEFAULT (0)
#endif // HAL_PLATFORM_MCU_DEFAULT

#ifndef HAL_PLATFORM_ETHERNET
#define HAL_PLATFORM_ETHERNET (0)
#endif /* HAL_PLATFORM_ETHERNET */

#ifndef HAL_PLATFORM_ETHERNET_FEATHERWING_SPI_CLOCK
#define HAL_PLATFORM_ETHERNET_FEATHERWING_SPI_CLOCK (32000000)
#endif /* HAL_PLATFORM_ETHERNET_FEATHERWING_SPI_CLOCK */

// FIXME: HAL_PLATFORM_I2C_NUM
#ifndef HAL_PLATFORM_I2C1
#define HAL_PLATFORM_I2C1 (1)
#endif /* HAL_PLATFORM_I2C1 */

#ifndef HAL_PLATFORM_I2C2
#define HAL_PLATFORM_I2C2 (0)
#endif /* HAL_PLATFORM_I2C2 */

#ifndef HAL_PLATFORM_I2C3
#define HAL_PLATFORM_I2C3 (0)
#endif /* HAL_PLATFORM_I2C3 */

// FIXME: HAL_PLATFORM_USART_NUM
#ifndef HAL_PLATFORM_USART2
#define HAL_PLATFORM_USART2 (0)
#endif /* HAL_PLATFORM_USART2 */

#ifndef HAL_PLATFORM_USART3
#define HAL_PLATFORM_USART3 (0)
#endif /* HAL_PLATFORM_USART3 */

#ifndef HAL_PLATFORM_POWER_MANAGEMENT
#define HAL_PLATFORM_POWER_MANAGEMENT (0)
#endif /* HAL_PLATFORM_POWER_MANAGEMENT */

#ifndef HAL_PLATFORM_PMIC_BQ24195
#define HAL_PLATFORM_PMIC_BQ24195 (0)
#endif /* HAL_PLATFORM_PMIC_BQ24195 */

#ifndef HAL_PLATFORM_PMIC_BQ24195_FAULT_COUNT_THRESHOLD
#define HAL_PLATFORM_PMIC_BQ24195_FAULT_COUNT_THRESHOLD (5)
#endif /* HAL_PLATFORM_PMIC_BQ24195_FAULT_COUNT_THRESHOLD */

#if HAL_PLATFORM_PMIC_BQ24195
# ifndef HAL_PLATFORM_PMIC_BQ24195_I2C
#  error "HAL_PLATFORM_PMIC_BQ24195_I2C is not defined"
# endif /* HAL_PLATFORM_PMIC_BQ24195_I2C */
#endif /* HAL_PLATFORM_PMIC_BQ24195 */

#ifndef HAL_PLATFORM_PMIC_INT_PIN_PRESENT
#define HAL_PLATFORM_PMIC_INT_PIN_PRESENT (0)
#endif // HAL_PLATFORM_PMIC_INT_PIN_PRESENT

#ifndef HAL_PLATFORM_FUELGAUGE_MAX17043
#define HAL_PLATFORM_FUELGAUGE_MAX17043 (0)
#endif /* HAL_PLATFORM_FUELGAUGE_MAX17043 */

#if HAL_PLATFORM_FUELGAUGE_MAX17043
# ifndef HAL_PLATFORM_FUELGAUGE_MAX17043_I2C
#  error "HAL_PLATFORM_FUELGAUGE_MAX17043_I2C is not defined"
# endif /* HAL_PLATFORM_FUELGAUGE_MAX17043_I2C */
#endif /* HAL_PLATFORM_FUELGAUGE_MAX17043 */

#ifndef HAL_PLATFORM_POWER_MANAGEMENT_STACK_SIZE
#define HAL_PLATFORM_POWER_MANAGEMENT_STACK_SIZE (1024)
#endif /* HAL_PLATFORM_POWER_MANAGEMENT_STACK_SIZE */

#ifndef HAL_PLATFORM_BUTTON_DEBOUNCE_IN_SYSTICK
#define HAL_PLATFORM_BUTTON_DEBOUNCE_IN_SYSTICK (0)
#endif /* HAL_PLATFORM_BUTTON_DEBOUNCE_IN_SYSTICK */

#ifndef HAL_PLATFORM_USB
#define HAL_PLATFORM_USB (1)
#endif // HAL_PLATFORM_USB

#ifndef HAL_PLATFORM_USB_CDC
#define HAL_PLATFORM_USB_CDC (0)
#endif // HAL_PLATFORM_USB_CDC

#ifndef HAL_PLATFORM_USB_CDC_TX_FAIL_TIMEOUT_MS
#define HAL_PLATFORM_USB_CDC_TX_FAIL_TIMEOUT_MS (2500)
#endif // HAL_PLATFORM_USB_CDC_TX_FAIL_TIMEOUT_MS

#ifndef HAL_PLATFORM_USB_HID
#define HAL_PLATFORM_USB_HID (0)
#endif // HAL_PLATFORM_USB_HID

#ifndef HAL_PLATFORM_USB_COMPOSITE
#define HAL_PLATFORM_USB_COMPOSITE (0)
#endif // HAL_PLATFORM_USB_COMPOSITE

#ifndef HAL_PLATFORM_USB_CONTROL_INTERFACE
#define HAL_PLATFORM_USB_CONTROL_INTERFACE (0)
#endif // HAL_PLATFORM_USB_CONTROL_INTERFACE

#ifndef HAL_PLATFORM_RNG
#define HAL_PLATFORM_RNG (0)
#endif // HAL_PLATFORM_RNG

#ifndef HAL_PLATFORM_SPI_DMA_SOURCE_RAM_ONLY
#define HAL_PLATFORM_SPI_DMA_SOURCE_RAM_ONLY (0)
#endif // HAL_PLATFORM_SPI_DMA_SOURCE_RAM_ONLY

#ifndef HAL_PLATFORM_SPI_HAL_THREAD_SAFETY
#define HAL_PLATFORM_SPI_HAL_THREAD_SAFETY (0)
#endif // HAL_PLATFORM_SPI_HAL_THREAD_SAFETY

#ifndef HAL_PLATFORM_KEEP_DEPRECATED_APP_USB_REQUEST_HANDLERS
#define HAL_PLATFORM_KEEP_DEPRECATED_APP_USB_REQUEST_HANDLERS (0)
#endif // HAL_PLATFORM_KEEP_DEPRECATED_APP_USB_REQUEST_HANDLERS

#ifndef HAL_PLATFORM_CORE_ENTER_PANIC_MODE
#define HAL_PLATFORM_CORE_ENTER_PANIC_MODE (0)
#endif // HAL_PLATFORM_CORE_ENTER_PANIC_MODE (0)

#ifndef HAL_PLATFORM_DEFAULT_CLOUD_KEEPALIVE_INTERVAL
#define HAL_PLATFORM_DEFAULT_CLOUD_KEEPALIVE_INTERVAL (25000)
#endif // HAL_PLATFORM_DEFAULT_CLOUD_KEEPALIVE_INTERVAL

#ifndef HAL_PLATFORM_CELLULAR_CLOUD_KEEPALIVE_INTERVAL
#define HAL_PLATFORM_CELLULAR_CLOUD_KEEPALIVE_INTERVAL (23 * 60 * 1000)
#endif

#ifndef HAL_SOCKET_HAL_COMPAT_NO_SOCKADDR
#define HAL_SOCKET_HAL_COMPAT_NO_SOCKADDR (0)
#endif // HAL_SOCKET_HAL_COMPAT_NO_SOCKADDR

#ifndef HAL_PLATFORM_COMPRESSED_OTA
#define HAL_PLATFORM_COMPRESSED_OTA (0)
#endif // HAL_PLATFORM_COMPRESSED_OTA

#ifndef HAL_PLATFORM_NETWORK_MULTICAST
#define HAL_PLATFORM_NETWORK_MULTICAST (0)
#endif // HAL_PLATFORM_NETWORK_MULTICAST

#ifndef HAL_PLATFORM_POWER_MANAGEMENT_OPTIONAL
#define HAL_PLATFORM_POWER_MANAGEMENT_OPTIONAL (0)
#endif // HAL_PLATFORM_POWER_MANAGEMENT_OPTIONAL

#ifndef HAL_PLATFORM_IPV6
#define HAL_PLATFORM_IPV6 (0)
#endif // HAL_PLATFORM_IPV6

#ifndef HAL_PLATFORM_NFC
#define HAL_PLATFORM_NFC 0
#endif /* HAL_PLATFORM_NFC */

#ifndef HAL_PLATFORM_POWER_WORKAROUND_USB_HOST_VIN_SOURCE
#define HAL_PLATFORM_POWER_WORKAROUND_USB_HOST_VIN_SOURCE (0)
#endif // HAL_PLATFORM_POWER_WORKAROUND_USB_HOST_VIN_SOURCE

#ifndef HAL_PLATFORM_RADIO_STACK
#define HAL_PLATFORM_RADIO_STACK (0)
#endif /* HAL_PLATFORM_RADIO_STACK */

#ifndef HAL_PLATFORM_RADIO_ANTENNA_INTERNAL
#define HAL_PLATFORM_RADIO_ANTENNA_INTERNAL (0)
#endif // HAL_PLATFORM_RADIO_ANTENNA_INTERNAL

#ifndef HAL_PLATFORM_RADIO_ANTENNA_EXTERNAL
#define HAL_PLATFORM_RADIO_ANTENNA_EXTERNAL (0)
#endif // HAL_PLATFORM_RADIO_ANTENNA_EXTERNAL

#ifndef HAL_PLATFORM_SETUP_BUTTON_UX
#define HAL_PLATFORM_SETUP_BUTTON_UX (0)
#endif /* HAL_PLATFORM_SETUP_BUTTON_UX */

#ifndef HAL_PLATFORM_BACKUP_RAM
#define HAL_PLATFORM_BACKUP_RAM (0)
#endif /* HAL_PLATFORM_BACKUP_RAM */

#ifndef HAL_PLATFORM_HW_FORM_FACTOR_SOM
#define HAL_PLATFORM_HW_FORM_FACTOR_SOM (0)
#endif // HAL_PLATFORM_HW_FORM_FACTOR_SOM

#ifndef HAL_PLATFORM_MAY_LEAK_SOCKETS
#define HAL_PLATFORM_MAY_LEAK_SOCKETS (0)
#endif // HAL_PLATFORM_MAY_LEAK_SOCKETS

#ifndef HAL_PLATFORM_PACKET_BUFFER_FLOW_CONTROL_THRESHOLD
#define HAL_PLATFORM_PACKET_BUFFER_FLOW_CONTROL_THRESHOLD (0)
#endif // HAL_PLATFORM_PACKET_BUFFER_FLOW_CONTROL_THRESHOLD

#ifndef HAL_PLATFORM_MUXER_MAY_NEED_DELAY_IN_TX
#define HAL_PLATFORM_MUXER_MAY_NEED_DELAY_IN_TX (0)
#endif //HAL_PLATFORM_MUXER_MAY_NEED_DELAY_IN_TX

#ifndef HAL_PLATFORM_GEN
#define HAL_PLATFORM_GEN (PLATFORM_GEN)
#endif // HAL_PLATFORM_GEN

#ifndef HAL_PLATFORM_IO_EXTENSION
#define HAL_PLATFORM_IO_EXTENSION (0)
#endif // HAL_PLATFORM_IO_EXTENSION

#ifndef HAL_PLATFORM_MCP23S17
#define HAL_PLATFORM_MCP23S17 (0)
#endif // HAL_PLATFORM_MCP23S17

#if HAL_PLATFORM_MCP23S17
# ifndef HAL_PLATFORM_MCP23S17_SPI
#  error "HAL_PLATFORM_MCP23S17_SPI is not defined"
# endif /* HAL_PLATFORM_MCP23S17_SPI */
# ifndef HAL_PLATFORM_MCP23S17_SPI_CLOCK
#  define HAL_PLATFORM_MCP23S17_SPI_CLOCK (32000000)
# endif // HAL_PLATFORM_MCP23S17_SPI_CLOCK
# ifndef HAL_PLATFORM_MCP23S17_MIRROR_INTERRUPTS
#  define HAL_PLATFORM_MCP23S17_MIRROR_INTERRUPTS (0)
# endif // HAL_PLATFORM_MCP23S17_MIRROR_INTERRUPTS
#endif /* HAL_PLATFORM_MCP23S17 */

#ifndef HAL_PLATFORM_DEMUX
#define HAL_PLATFORM_DEMUX (0)
#endif // HAL_PLATFORM_DEMUX

#ifndef HAL_PLATFORM_EXTERNAL_RTC
#define HAL_PLATFORM_EXTERNAL_RTC (0)
#endif // HAL_PLATFORM_EXTERNAL_RTC

#if HAL_PLATFORM_EXTERNAL_RTC
# ifndef HAL_PLATFORM_EXTERNAL_RTC_I2C
#  error "HAL_PLATFORM_EXTERNAL_RTC_I2C is not defined"
# endif /* HAL_PLATFORM_EXTERNAL_RTC_I2C */
# ifndef HAL_PLATFORM_EXTERNAL_RTC_I2C_ADDR
#  error "HAL_PLATFORM_EXTERNAL_RTC_I2C_ADDR is not defined"
# endif /* HAL_PLATFORM_EXTERNAL_RTC_I2C_ADDR */
# ifndef HAL_PLATFORM_EXTERNAL_RTC_CAL_XT
#  define HAL_PLATFORM_EXTERNAL_RTC_CAL_XT (0)
# endif /* HAL_PLATFORM_EXTERNAL_RTC_CAL_XT */
#endif /* HAL_PLATFORM_EXTERNAL_RTC */

#ifndef HAL_PLATFORM_FILE_MAXIMUM_FD
#define HAL_PLATFORM_FILE_MAXIMUM_FD (65535)
#endif // HAL_PLATFORM_FILE_MAXIMUM_FD

#ifndef HAL_PLATFORM_SOCKET_IOCTL_NOTIFY
#define HAL_PLATFORM_SOCKET_IOCTL_NOTIFY (0)
#endif // HAL_PLATFORM_SOCKET_IOCTL_NOTIFY

#ifndef HAL_PLATFORM_NEWLIB
#define HAL_PLATFORM_NEWLIB (0)
#endif // HAL_PLATFORM_NEWLIB

#ifndef HAL_PLATFORM_SPI_NUM
#define HAL_PLATFORM_SPI_NUM (0)
#endif // HAL_PLATFORM_SPI_NUM

#ifndef HAL_PLATFORM_I2C_NUM
#define HAL_PLATFORM_I2C_NUM (0)
#endif // HAL_PLATFORM_I2C_NUM

#ifndef HAL_PLATFORM_USART_NUM
#define HAL_PLATFORM_USART_NUM (0)
#endif // HAL_PLATFORM_USART_NUM

#ifndef HAL_PLATFORM_USART_DEFAULT_BUFFER_SIZE
#define HAL_PLATFORM_USART_DEFAULT_BUFFER_SIZE (64)
#endif // HAL_PLATFORM_USART_DEFAULT_BUFFER_SIZE

#ifndef HAL_PLATFORM_EXPORT_STDLIB_RT_DYNALIB
#define HAL_PLATFORM_EXPORT_STDLIB_RT_DYNALIB (0)
#endif // HAL_PLATFORM_EXPORT_STDLIB_RT_DYNALIB

#ifndef HAL_PLATFORM_POWER_MANAGEMENT_PMIC_WATCHDOG
#define HAL_PLATFORM_POWER_MANAGEMENT_PMIC_WATCHDOG (0)
#endif // HAL_PLATFORM_POWER_MANAGEMENT_PMIC_WATCHDOG

#ifndef HAL_PLATFORM_OTA_PROTOCOL_V3
#define HAL_PLATFORM_OTA_PROTOCOL_V3 (0)
#endif // HAL_PLATFORM_OTA_PROTOCOL_V3

#ifndef HAL_PLATFORM_RESUMABLE_OTA
#define HAL_PLATFORM_RESUMABLE_OTA (0)
#elif HAL_PLATFORM_RESUMABLE_OTA && !HAL_PLATFORM_OTA_PROTOCOL_V3
#error "HAL_PLATFORM_RESUMABLE_OTA requires HAL_PLATFORM_OTA_PROTOCOL_V3"
#endif

#ifndef HAL_PLATFORM_ERROR_MESSAGES
#define HAL_PLATFORM_ERROR_MESSAGES (0)
#endif // HAL_PLATFORM_ERROR_MESSAGES

#ifndef HAL_PLATFORM_MULTIPART_SYSTEM
#define HAL_PLATFORM_MULTIPART_SYSTEM (0)
#endif // HAL_PLATFORM_MULTIPART_SYSTEM

#ifndef HAL_PLATFORM_NCP_COUNT
#define HAL_PLATFORM_NCP_COUNT (0)
#endif // HAL_PLATFORM_NCP_COUNT

#ifndef HAL_PLATFORM_WIFI_COMPAT
#define HAL_PLATFORM_WIFI_COMPAT (0)
#endif // HAL_PLATFORM_WIFI_COMPAT

#ifndef HAL_PLATFORM_WIFI_SCAN_ONLY
#define HAL_PLATFORM_WIFI_SCAN_ONLY (0)
#endif // HAL_PLATFORM_WIFI_SCAN_ONLY

#ifndef HAL_PLATFORM_BROKEN_MTU
#define HAL_PLATFORM_BROKEN_MTU (0)
#endif // HAL_PLATFORM_BROKEN_MTU

#ifndef HAL_PLATFORM_PROHIBIT_XIP
#define HAL_PLATFORM_PROHIBIT_XIP (0)
#endif // HAL_PLATFORM_PROHIBIT_XIP

#ifndef HAL_PLATFORM_USB_DFU_INTERFACES
#define HAL_PLATFORM_USB_DFU_INTERFACES (0)
#endif /* HAL_PLATFORM_USB_DFU_INTERFACES */

#ifndef HAL_PLATFORM_BOOTLOADER_USB_PROCESS_IN_MAIN_THREAD
#define HAL_PLATFORM_BOOTLOADER_USB_PROCESS_IN_MAIN_THREAD (0)
#endif // HAL_PLATFORM_BOOTLOADER_USB_PROCESS_IN_MAIN_THREAD

#ifndef HAL_PLATFORM_USB_MAX_DEVICE_DESCRIPTOR_LEN
#define HAL_PLATFORM_USB_MAX_DEVICE_DESCRIPTOR_LEN (64)
#endif // HAL_PLATFORM_USB_MAX_DEVICE_DESCRIPTOR_LEN

#ifndef HAL_PLATFORM_USB_LANGID_LIST
#define HAL_PLATFORM_USB_LANGID_LIST "\x09\x04" // U.S. English
#endif // HAL_PLATFORM_USB_LANGID_LIST

#ifndef HAL_PLATFORM_USB_MANUFACTURER_STRING
#define HAL_PLATFORM_USB_MANUFACTURER_STRING "Particle"
#endif // HAL_PLATFORM_USB_MANUFACTURER_STRING

#ifndef HAL_PLATFORM_USB_PRODUCT_STRING
#define HAL_PLATFORM_USB_PRODUCT_STRING "Product"
#endif // HAL_PLATFORM_USB_PRODUCT_STRING

#ifndef HAL_PLATFORM_USB_CONFIGURATION_STRING
#define HAL_PLATFORM_USB_CONFIGURATION_STRING HAL_PLATFORM_USB_PRODUCT_STRING
#endif // HAL_PLATFORM_USB_CONFIGURATION_STRING

#ifndef HAL_PLATFORM_MODULE_SUFFIX_EXTENSIONS
#define HAL_PLATFORM_MODULE_SUFFIX_EXTENSIONS (0)
#endif // HAL_PLATFORM_MODULE_SUFFIX_EXTENSIONS

#ifndef HAL_PLATFORM_MODULE_DYNAMIC_LOCATION
#define HAL_PLATFORM_MODULE_DYNAMIC_LOCATION (0)
#endif // HAL_PLATFORM_MODULE_DYNAMIC_LOCATION

#ifndef HAL_PLATFORM_USB_SOF
#define HAL_PLATFORM_USB_SOF (1)
#endif // HAL_PLATFORM_USB_SOF

#ifndef HAL_PLATFORM_INTERNAL_LOW_SPEED_CLOCK
#define HAL_PLATFORM_INTERNAL_LOW_SPEED_CLOCK (0)
#endif // HAL_PLATFORM_INTERNAL_LOW_SPEED_CLOCK

#ifndef HAL_PLATFORM_USART_9BIT_SUPPORTED
#define HAL_PLATFORM_USART_9BIT_SUPPORTED (0)
#endif // HAL_PLATFORM_USART_9BIT_SUPPORTED

#ifndef HAL_PLATFORM_CELLULAR_CONN_TIMEOUT
#define HAL_PLATFORM_CELLULAR_CONN_TIMEOUT (9*60*1000)
#endif // HAL_PLATFORM_CELLULAR_CONN_TIMEOUT

#ifndef HAL_PLATFORM_MAX_CLOUD_CONNECT_TIME
#define HAL_PLATFORM_MAX_CLOUD_CONNECT_TIME (5*60*1000)
#endif // HAL_PLATFORM_MAX_CLOUD_CONNECT_TIME

#ifndef HAL_PLATFORM_CONCURRENT_DUMP_THREADS
#define HAL_PLATFORM_CONCURRENT_DUMP_THREADS (0)
#endif // HAL_PLATFORM_CONCURRENT_DUMP_THREADS

#ifndef HAL_PLATFORM_LED_THEME
#define HAL_PLATFORM_LED_THEME (0)
#endif // HAL_PLATFORM_LED_THEME

#ifndef HAL_PLATFORM_FREERTOS
#define HAL_PLATFORM_FREERTOS (1)
#endif // HAL_PLATFORM_FREERTOS

#ifndef HAL_PLATFORM_SYSTEM_POOL_SIZE
#define HAL_PLATFORM_SYSTEM_POOL_SIZE 512
#endif

#ifndef HAL_PLATFORM_CELLULAR_MODEM_VOLTAGE_TRANSLATOR
#define HAL_PLATFORM_CELLULAR_MODEM_VOLTAGE_TRANSLATOR (1)
#endif // HAL_PLATFORM_CELLULAR_MODEM_VOLTAGE_TRANSLATOR

#ifndef HAL_PLATFORM_BLE_ACTIVE_EVENT
#define HAL_PLATFORM_BLE_ACTIVE_EVENT (0)
#endif // HAL_PLATFORM_BLE_ACTIVE_EVENT

#endif /* HAL_PLATFORM_H */
