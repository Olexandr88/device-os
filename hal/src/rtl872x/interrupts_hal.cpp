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

#ifdef __cplusplus
extern "C" {
#endif
#include "rtl8721d.h"
#ifdef __cplusplus
}
#endif
#include "interrupts_hal.h"
#include "interrupts_irq.h"
#include "pinmap_impl.h"
#include "gpio_hal.h"
#include "check.h"

#if HAL_PLATFORM_IO_EXTENSION && MODULE_FUNCTION != MOD_FUNC_BOOTLOADER
#if HAL_PLATFORM_MCP23S17
#include "mcp23s17.h"
#endif
using namespace particle;
#endif

extern uintptr_t link_ram_interrupt_vectors_location[];

namespace {

typedef enum interrupt_state_t {
    INT_STATE_DISABLED,
    INT_STATE_ENABLED,
    INT_STATE_SUSPENDED
} interrupt_state_t;

struct {
    interrupt_state_t           state;
    hal_interrupt_callback_t    callback;
    InterruptMode               mode;
} interruptsConfig[TOTAL_PINS];

int parseMode(InterruptMode mode, uint32_t* trigger, uint32_t* polarity) {
    switch(mode) {
        case RISING: {
            *trigger = GPIO_INT_Trigger_EDGE;
            *polarity = GPIO_INT_POLARITY_ACTIVE_HIGH;
            break;
        }
        case FALLING: {
            *trigger = GPIO_INT_Trigger_EDGE;
            *polarity = GPIO_INT_POLARITY_ACTIVE_LOW;
            break;
        }
        case CHANGE: {
            *trigger = GPIO_INT_Trigger_BOTHEDGE;
            *polarity = GPIO_INT_POLARITY_ACTIVE_LOW;
            break;
        }
        default: {
            return SYSTEM_ERROR_INVALID_ARGUMENT;
        }
    }
    return SYSTEM_ERROR_NONE;
}

void gpioIntHandler(void* data) {
    uint16_t pin = (uint32_t)data;
    if (!hal_pin_is_valid(pin)) {
        return;
    }
    if (interruptsConfig[pin].callback.handler) {
        interruptsConfig[pin].callback.handler(interruptsConfig[pin].callback.data);
    }
}

} // anonymous

void hal_interrupt_init(void) {
    // FIXME: when io expender is used, the TOTAL_PINS includes the pins on the expender
    for (int i = 0; i < TOTAL_PINS; i++) {
        interruptsConfig[i].state = INT_STATE_DISABLED;
        interruptsConfig[i].callback.handler = nullptr;
        interruptsConfig[i].callback.data = nullptr;
    }
#if defined (ARM_CORE_CM0)
    // Just in case
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
#endif
}

void hal_interrupt_uninit(void) {
    // FIXME: when io expender is used, the TOTAL_PINS includes the pins on the expender
    for (int i = 0; i < TOTAL_PINS; i++) {
        if (interruptsConfig[i].state == INT_STATE_ENABLED) {
            const uint32_t rtlPin = hal_pin_to_rtl_pin(i);
            GPIO_INTMode(rtlPin, DISABLE, 0, 0, 0);
            GPIO_INTConfig(rtlPin, DISABLE);
            interruptsConfig[i].state = INT_STATE_DISABLED;
            interruptsConfig[i].callback.handler = nullptr;
            interruptsConfig[i].callback.data = nullptr;
        }
    }
}

// FIXME: attaching interrupt on KM4 side will also enable interrupt on KM0 (since it can wake up KM0 for now)?
int hal_interrupt_attach(uint16_t pin, hal_interrupt_handler_t handler, void* data, InterruptMode mode, hal_interrupt_extra_configuration_t* config) {
    CHECK_TRUE(hal_pin_is_valid(pin), SYSTEM_ERROR_INVALID_ARGUMENT);
    hal_pin_info_t* pinInfo = hal_pin_map() + pin;

#if HAL_PLATFORM_IO_EXTENSION && MODULE_FUNCTION != MOD_FUNC_BOOTLOADER
    if (pinInfo->type == HAL_PIN_TYPE_MCU) {
#endif
        const uint32_t rtlPin = hal_pin_to_rtl_pin(pin);

        if ((pinInfo->gpio_port == RTL_PORT_A && pinInfo->gpio_pin == 27) ||
                (pinInfo->gpio_port == RTL_PORT_B && pinInfo->gpio_pin == 3)) {
            Pinmux_Swdoff();
        }

        GPIO_InitTypeDef  GPIO_InitStruct = {};
        GPIO_InitStruct.GPIO_Pin = rtlPin;
        GPIO_InitStruct.GPIO_Mode = GPIO_Mode_INT;
        parseMode(mode, &GPIO_InitStruct.GPIO_ITTrigger, &GPIO_InitStruct.GPIO_ITPolarity);
        GPIO_Init(&GPIO_InitStruct);

        if (pinInfo->gpio_port == RTL_PORT_A) {
            InterruptRegister(GPIO_INTHandler, GPIOA_IRQ, (u32)GPIOA_BASE, 5);		
            InterruptEn(GPIOA_IRQ, 5);
        } else if (pinInfo->gpio_port == RTL_PORT_B) {
            InterruptRegister(GPIO_INTHandler, GPIOB_IRQ, (u32)GPIOB_BASE, 5);		
            InterruptEn(GPIOB_IRQ, 5);
        } else {
            // Should not get here
            return SYSTEM_ERROR_INVALID_ARGUMENT;
        }

        GPIO_UserRegIrq(rtlPin, (VOID*)gpioIntHandler, (void*)((uint32_t)pin));

        GPIO_INTMode(rtlPin, ENABLE, GPIO_InitStruct.GPIO_ITTrigger, GPIO_InitStruct.GPIO_ITPolarity, GPIO_INT_DEBOUNCE_ENABLE);
        GPIO_INTConfig(rtlPin, ENABLE);

        interruptsConfig[pin].state = INT_STATE_ENABLED;
        interruptsConfig[pin].callback.handler = handler;
        interruptsConfig[pin].callback.data = data;
        interruptsConfig[pin].mode = mode;
        hal_pin_set_function(pin, PF_DIO);

        return SYSTEM_ERROR_NONE;
#if HAL_PLATFORM_IO_EXTENSION && MODULE_FUNCTION != MOD_FUNC_BOOTLOADER
    }
#if HAL_PLATFORM_MCP23S17
    else if (pinInfo->type == HAL_PIN_TYPE_IO_EXPANDER) {
        // TODO
        return SYSTEM_ERROR_NOT_SUPPORTED;
    }
#endif
    else {
        return SYSTEM_ERROR_NOT_SUPPORTED;
    }
#endif
}

int hal_interrupt_detach(uint16_t pin) {
    return hal_interrupt_detach_ext(pin, 0, nullptr);
}

int hal_interrupt_detach_ext(uint16_t pin, uint8_t keepHandler, void* reserved) {
    CHECK_TRUE(hal_pin_is_valid(pin), SYSTEM_ERROR_INVALID_ARGUMENT);
    
#if HAL_PLATFORM_IO_EXTENSION && MODULE_FUNCTION != MOD_FUNC_BOOTLOADER
    hal_pin_info_t* pinInfo = hal_pin_map() + pin;
    if (pinInfo->type == HAL_PIN_TYPE_MCU) {
#endif
        const uint32_t rtlPin = hal_pin_to_rtl_pin(pin);
        GPIO_INTMode(rtlPin, DISABLE, 0, 0, 0);
        GPIO_INTConfig(rtlPin, DISABLE);
        interruptsConfig[pin].state = INT_STATE_DISABLED;

        if (keepHandler) {
            return SYSTEM_ERROR_NONE;
        }

        interruptsConfig[pin].callback.handler = nullptr;
        interruptsConfig[pin].callback.data = nullptr;
        hal_pin_set_function(pin, PF_NONE);

        return SYSTEM_ERROR_NONE;
#if HAL_PLATFORM_IO_EXTENSION && MODULE_FUNCTION != MOD_FUNC_BOOTLOADER
    }
#if HAL_PLATFORM_MCP23S17
    else if (pinInfo->type == HAL_PIN_TYPE_IO_EXPANDER) {
        // TODO
        return SYSTEM_ERROR_NOT_SUPPORTED;
    } 
#endif
    else {
        return SYSTEM_ERROR_NOT_SUPPORTED;
    }
#endif
}

void hal_interrupt_enable_all(void) {
    // FIXME: this only enables GPIO interrupt, while the API name is ambiguous
    NVIC_ClearPendingIRQ(GPIOA_IRQ);
    NVIC_EnableIRQ(GPIOA_IRQ);
    NVIC_ClearPendingIRQ(GPIOB_IRQ);
    NVIC_EnableIRQ(GPIOB_IRQ);
}

void hal_interrupt_disable_all(void) {
    // FIXME: this only disables GPIO interrupt, while the API name is ambiguous
    NVIC_DisableIRQ(GPIOA_IRQ);
    NVIC_DisableIRQ(GPIOB_IRQ);
}

void hal_interrupt_suspend(void) {
    for (int i = 0; i < TOTAL_PINS; i++) {
        if (interruptsConfig[i].state == INT_STATE_ENABLED) {
            const uint32_t rtlPin = hal_pin_to_rtl_pin(i);
            GPIO_INTMode(rtlPin, DISABLE, 0, 0, 0);
            GPIO_INTConfig(rtlPin, DISABLE);
            interruptsConfig[i].state = INT_STATE_SUSPENDED;
        }
    }
}

void hal_interrupt_restore(void) {
    for (int i = 0; i < TOTAL_PINS; i++) {
        const uint32_t rtlPin = hal_pin_to_rtl_pin(i);
        // In case that interrupt is enabled by sleep
        GPIO_INTConfig(rtlPin, DISABLE);
        if (interruptsConfig[i].state == INT_STATE_SUSPENDED) {
            uint32_t trigger = 0, polarity = 0;
            parseMode(interruptsConfig[i].mode, &trigger, &polarity);
            GPIO_INTMode(rtlPin, ENABLE, trigger, polarity, GPIO_INT_DEBOUNCE_ENABLE);
            GPIO_INTConfig(rtlPin, ENABLE);
            interruptsConfig[i].state = INT_STATE_ENABLED;
        }
    }
}

int hal_interrupt_set_direct_handler(IRQn_Type irqn, hal_interrupt_direct_handler_t handler, uint32_t flags, void* reserved) {
    // FIXME: the maxinum IRQn for KM0 is not identical with KM4
    if (irqn < NonMaskableInt_IRQn || irqn > GDMA0_CHANNEL5_IRQ_S) {
        return 1;
    }

    int32_t state = HAL_disable_irq();
    volatile uint32_t* isrs = (volatile uint32_t*)&link_ram_interrupt_vectors_location;

    if (handler == nullptr && (flags & HAL_INTERRUPT_DIRECT_FLAG_RESTORE)) {
        // Restore
        // HAL_Core_Restore_Interrupt(irqn);
    } else {
        isrs[IRQN_TO_IDX(irqn)] = (uint32_t)handler;
    }

    if (flags & HAL_INTERRUPT_DIRECT_FLAG_DISABLE) {
        // Disable
        // sd_nvic_DisableIRQ(irqn);
    } else if (flags & HAL_INTERRUPT_DIRECT_FLAG_ENABLE) {
        // sd_nvic_EnableIRQ(irqn);
    }

    HAL_enable_irq(state);

    return 0;
}

bool hal_interrupt_is_isr() {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
}

int32_t hal_interrupt_serviced_irqn() {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) - 16;
}

uint32_t hal_interrupt_get_basepri() {
    return (__get_BASEPRI() >> (8 - __NVIC_PRIO_BITS));
}

bool hal_interrupt_is_irq_masked(int32_t irqn) {
    uint32_t basepri = hal_interrupt_get_basepri();
    return __get_PRIMASK() || (basepri > 0 && NVIC_GetPriority((IRQn_Type)irqn) >= basepri);
}

bool hal_interrupt_will_preempt(int32_t irqn1, int32_t irqn2) {
    if (irqn1 == irqn2) {
        return false;
    }

    uint32_t priorityGroup = NVIC_GetPriorityGrouping();
    uint32_t priority1 = NVIC_GetPriority((IRQn_Type)irqn1);
    uint32_t priority2 = NVIC_GetPriority((IRQn_Type)irqn2);
    uint32_t p1, sp1, p2, sp2;
    NVIC_DecodePriority(priority1, priorityGroup, &p1, &sp1);
    NVIC_DecodePriority(priority2, priorityGroup, &p2, &sp2);
    if (p1 < p2) {
        return true;
    }
    return false;
}
