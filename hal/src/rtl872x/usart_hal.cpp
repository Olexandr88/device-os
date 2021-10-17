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

#include "logging.h"
#include "usart_hal.h"
#include "ringbuffer.h"
#include "pinmap_hal.h"
#include "gpio_hal.h"
#include <algorithm>
#include "hal_irq_flag.h"
#include "delay_hal.h"
#include "interrupts_hal.h"
#include "usart_hal_private.h"
#include "timer_hal.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "rtl8721d.h"
#ifdef __cplusplus
}
#endif


class Usart {
public:
    struct Config {
        unsigned int baudRate;
        unsigned int config;
    };

    bool isEnabled() const {
        return state_ == HAL_USART_STATE_ENABLED;
    }

    bool isConfigured() const {
        return configured_;
    }

    int init(const hal_usart_buffer_config_t& conf) {
        if (isEnabled()) {
            flush();
            CHECK(end());
        }
        if (isConfigured()) {
            CHECK(deInit());
        }
        CHECK_TRUE(conf.rx_buffer, SYSTEM_ERROR_INVALID_ARGUMENT);
        CHECK_TRUE(conf.rx_buffer_size, SYSTEM_ERROR_INVALID_ARGUMENT);
        CHECK_TRUE(conf.tx_buffer, SYSTEM_ERROR_INVALID_ARGUMENT);
        CHECK_TRUE(conf.tx_buffer_size, SYSTEM_ERROR_INVALID_ARGUMENT);
        rxBuffer_.init((uint8_t*)conf.rx_buffer, conf.rx_buffer_size);
        txBuffer_.init((uint8_t*)conf.tx_buffer, conf.tx_buffer_size);
        configured_ = true;
        return SYSTEM_ERROR_NONE;
    }

    int deInit() {
        configured_ = false;
        return SYSTEM_ERROR_NONE;
    }

    int begin(const Config& conf) {
        CHECK_TRUE(isConfigured(), SYSTEM_ERROR_INVALID_STATE);
        CHECK_TRUE(validateConfig(conf.config), SYSTEM_ERROR_INVALID_ARGUMENT);
        if (isEnabled()) {
            flush();
            end();
        }
        auto uartInstance = uartTable_[index_].UARTx;
        // Enable peripheral clock
        if (uartInstance == UART0_DEV) {
            RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, ENABLE);
        }
        // Configur TX/RX pins
        if (uartInstance == UART2_DEV) {
            Pinmux_Config(hal_pin_to_rtl_pin(txPin_), PINMUX_FUNCTION_LOGUART);
            Pinmux_Config(hal_pin_to_rtl_pin(rxPin_), PINMUX_FUNCTION_LOGUART);
        } else {
            Pinmux_Config(hal_pin_to_rtl_pin(txPin_), PINMUX_FUNCTION_UART);
            Pinmux_Config(hal_pin_to_rtl_pin(rxPin_), PINMUX_FUNCTION_UART);
        }
        PAD_PullCtrl(hal_pin_to_rtl_pin(txPin_), GPIO_PuPd_UP);
        PAD_PullCtrl(hal_pin_to_rtl_pin(rxPin_), GPIO_PuPd_UP);
        // Configure CTS/RTS pins
        if (ctsPin_ != PIN_INVALID) {
            Pinmux_Config(hal_pin_to_rtl_pin(ctsPin_), PINMUX_FUNCTION_UART_RTSCTS);
            PAD_PullCtrl(hal_pin_to_rtl_pin(ctsPin_), GPIO_PuPd_UP);
        }
        if (rtsPin_ != PIN_INVALID) {
            Pinmux_Config(hal_pin_to_rtl_pin(rtsPin_), PINMUX_FUNCTION_UART_RTSCTS);
            PAD_PullCtrl(hal_pin_to_rtl_pin(rtsPin_), GPIO_PuPd_UP);
        }
        UART_InitTypeDef uartInitStruct = {};
        UART_StructInit(&uartInitStruct);
        UART_Init(uartInstance, &uartInitStruct);

        RCC_PeriphClockSource_UART(uartInstance, UART_RX_CLK_XTAL_40M);
        UART_SetBaud(uartInstance, conf.baudRate);
        UART_RxCmd(uartInstance, ENABLE);

        UART_RxCmd(uartInstance, DISABLE);
        // Data bits, stop bits and parity
        if ((conf.config & SERIAL_DATA_BITS) == SERIAL_DATA_BITS_8) {
            uartInitStruct.WordLen = RUART_WLS_8BITS;
        } else {
            uartInitStruct.WordLen = RUART_WLS_7BITS;
        }
        if ((conf.config & SERIAL_STOP_BITS) == SERIAL_STOP_BITS_2) {
            uartInitStruct.StopBit = RUART_STOP_BIT_2;
        } else {
            uartInitStruct.StopBit = RUART_STOP_BIT_1;
        }
        switch (conf.config & SERIAL_PARITY) {
            case SERIAL_PARITY_ODD: {
                uartInitStruct.Parity = RUART_PARITY_ENABLE;
                uartInitStruct.ParityType = RUART_ODD_PARITY;
                break;
            }
            case SERIAL_PARITY_EVEN: {
                uartInitStruct.Parity = RUART_PARITY_ENABLE;
                uartInitStruct.ParityType = RUART_EVEN_PARITY;
                break;
            }
            default: { // ParityNone
                uartInitStruct.Parity = RUART_PARITY_DISABLE;
                break;
            }
        }
        uartInstance->LCR = ((uartInitStruct.WordLen) | (uartInitStruct.StopBit << 2) |
                             (uartInitStruct.Parity << 3) | (uartInitStruct.ParityType << 4) | (uartInitStruct.StickParity << 5));
        UART_RxCmd(uartInstance, ENABLE);

        if ((ctsPin_ != PIN_INVALID || rtsPin_ != PIN_INVALID) &&
            ((conf.config & SERIAL_FLOW_CONTROL) != SERIAL_FLOW_CONTROL_NONE)) {
            UART_BreakCtl(uartInstance, 1);
        } else {
            UART_BreakCtl(uartInstance, 0);
        }

        if (uartInstance != UART2_DEV) {
            // Configuring DMA
            if (!initDmaChannels()) {
                end();
                return SYSTEM_ERROR_INTERNAL;
            }
        } else {
            InterruptRegister((IRQ_FUN)uartTxRxIntHandler, uartTable_[index_].IrqNum, (uint32_t)this, 5);
	        InterruptEn(uartTable_[index_].IrqNum, 5);
        }

        startReceiver();

        config_ = conf;
        state_ = HAL_USART_STATE_ENABLED;
        return SYSTEM_ERROR_NONE;
    }

    int end() {
        CHECK_TRUE(state_ != HAL_USART_STATE_DISABLED, SYSTEM_ERROR_INVALID_STATE);
        auto uartInstance = uartTable_[index_].UARTx;
        if (uartInstance != UART2_DEV) {
            deinitDmaChannels();
        } else {
            InterruptDis(uartTable_[index_].IrqNum);
	        InterruptUnRegister(uartTable_[index_].IrqNum);
            UART_INTConfig(uartInstance, RUART_IER_ETBEI, DISABLE);
            UART_INTConfig(uartInstance, (RUART_IER_ERBI | RUART_IER_ELSI | RUART_IER_ETOI), DISABLE);
        }
        UART_ClearTxFifo(uartInstance);
        UART_ClearRxFifo(uartInstance);
        UART_DeInit(uartInstance);
        if (uartInstance == UART0_DEV) {
            RCC_PeriphClockCmd(APBPeriph_UART0, APBPeriph_UART0_CLOCK, DISABLE);
        }
        // Do not change the pull ability to not mess up the peer device.
        Pinmux_Config(hal_pin_to_rtl_pin(txPin_), PINMUX_FUNCTION_GPIO);
        Pinmux_Config(hal_pin_to_rtl_pin(rxPin_), PINMUX_FUNCTION_GPIO);
        if (ctsPin_ != PIN_INVALID) {
            Pinmux_Config(hal_pin_to_rtl_pin(ctsPin_), PINMUX_FUNCTION_GPIO);
        }
        if (rtsPin_ != PIN_INVALID) {
            Pinmux_Config(hal_pin_to_rtl_pin(rtsPin_), PINMUX_FUNCTION_GPIO);
        }
        config_ = {};
        rxBuffer_.reset();
        txBuffer_.reset();
        curTxCount_ = 0;
        transmitting_ = false;
        receiving_ = false;
        state_ = HAL_USART_STATE_DISABLED;
        return SYSTEM_ERROR_NONE;
    }

    ssize_t space() {
        CHECK_TRUE(isEnabled(), SYSTEM_ERROR_INVALID_STATE);
        return txBuffer_.space();
    }

    ssize_t write(const uint8_t* buffer, size_t size) {
        CHECK_TRUE(buffer, SYSTEM_ERROR_INVALID_ARGUMENT);
        const ssize_t canWrite = CHECK(space());
        
        const size_t writeSize = std::min((size_t)canWrite, size);
        CHECK_TRUE(writeSize > 0, SYSTEM_ERROR_NO_MEMORY);
        
        ssize_t r = CHECK(txBuffer_.put(buffer, writeSize));

        startTransmission();
        return r;
    }

    ssize_t flush() {
        startTransmission();
        while (true) {
            if (!isEnabled() || txBuffer_.empty()) {
                break;
            }
            // Poll the status just in case that the interrupt handler is not invoked even if there is int pending.
            uartTxRxIntHandler(usart);
        }
        return 0;
    }

    ssize_t data() {
        CHECK_TRUE(isEnabled(), SYSTEM_ERROR_INVALID_STATE);
        auto uartInstance = uartTable_[index_].UARTx;
        ssize_t len = 0;
        if (receiving_) {
            if (uartInstance != UART2_DEV) {
                len = rxBuffer_.data();
                const uint32_t curAddr = GDMA_GetDstAddr(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum);
                GDMA_Cmd(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, DISABLE);
                const uint32_t receivedDma = curAddr - rxDmaInitStruct_.GDMA_DstAddr;
                uint32_t inFifo = UART_ReceiveDataTO(uartInstance, (uint8_t*)curAddr, rxDmaInitStruct_.GDMA_BlockSize - receivedDma, 1);
                uint32_t receivedTotal = receivedDma + inFifo;
                // Clean Auto Reload Bit
                GDMA_ChCleanAutoReload(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, CLEAN_RELOAD_DST);
                // Clear Pending ISR
                GDMA_ClearINT(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum);
                rxBuffer_.acquireCommit(receivedTotal, rxDmaInitStruct_.GDMA_BlockSize - receivedTotal/* cancel */);
                len += receivedTotal;
            } else {
                UART_INTConfig(uartInstance, (RUART_IER_ERBI | RUART_IER_ELSI | RUART_IER_ETOI), DISABLE);
                uint8_t temp[MAX_UART_FIFO_SIZE];
                uint32_t inFifo = UART_ReceiveDataTO(uartInstance, temp, MAX_UART_FIFO_SIZE, 1);
                rxBuffer_.put(temp, inFifo);
                len = rxBuffer_.data();
            }
            receiving_ = false;
            startReceiver();
        }
        return len;
    }

    ssize_t read(uint8_t* buffer, size_t size) {
        CHECK_TRUE(isEnabled(), SYSTEM_ERROR_INVALID_STATE);
        const ssize_t maxRead = CHECK(data());
        const size_t readSize = std::min((size_t)maxRead, size);
        CHECK_TRUE(readSize > 0, SYSTEM_ERROR_NO_MEMORY);
        ssize_t r = CHECK(rxBuffer_.get(buffer, readSize));
        if (!receiving_) {
            startReceiver();
        }
        return r;
    }

    ssize_t peek(uint8_t* buffer, size_t size) {
        CHECK_TRUE(isEnabled(), SYSTEM_ERROR_INVALID_STATE);
        const ssize_t maxRead = CHECK(data());
        const size_t peekSize = std::min((size_t)maxRead, size);
        CHECK_TRUE(peekSize > 0, SYSTEM_ERROR_NO_MEMORY);
        return rxBuffer_.peek(buffer, peekSize);
    }

    static Usart* getInstance(hal_usart_interface_t serial) {
        static Usart Usarts[] = {
            {2, TX,  RX,  PIN_INVALID, PIN_INVALID}, // LOG UART
            {0, TX1, RX1, CTS1,        RTS1} // UART0
        };
        CHECK_TRUE(serial < sizeof(Usarts) / sizeof(Usarts[0]), nullptr);
        return &Usarts[serial];
    }

    static uint32_t uartTxRxIntHandler(void* data) {
        auto uart = (Usart*)data;
        auto uartInstance = uart->uartTable_[uart->index_].UARTx;
        volatile uint8_t regIir = UART_IntStatus(uartInstance);
        if ((regIir & RUART_IIR_INT_PEND) != 0) {
            // No pending IRQ
            return 0;
        }
        uint8_t intId = (regIir & RUART_IIR_INT_ID) >> 1;
        switch (intId) {
            case RUART_LP_RX_MONITOR_DONE: {
                UART_RxMonitorSatusGet(uartInstance);
                break;
            }
            case RUART_MODEM_STATUS: {
                UART_ModemStatusGet(uartInstance);
                break;
            }
            case RUART_RECEIVE_LINE_STATUS: {
                UART_LineStatusGet(uartInstance);
                break;
            }
            case RUART_TX_FIFO_EMPTY: {
                UART_INTConfig(uartInstance, RUART_IER_ETBEI, DISABLE);
                if (uart->transmitting_) {
                    uart->transmitting_ = false;
                    uart->txBuffer_.consumeCommit(uart->curTxCount_);
                    uart->startTransmission();
                }
                break;
            }
            case RUART_RECEIVER_DATA_AVAILABLE:
            case RUART_TIME_OUT_INDICATION: {
                UART_INTConfig(uartInstance, (RUART_IER_ERBI | RUART_IER_ELSI | RUART_IER_ETOI), DISABLE);
                if (uart->receiving_) {
                    uart->receiving_ = false;
                    uint8_t temp[MAX_UART_FIFO_SIZE];
                    uint32_t inFifo = UART_ReceiveDataTO(uartInstance, temp, MAX_UART_FIFO_SIZE, 1);
                    uart->rxBuffer_.put(temp, inFifo);
                    uart->startReceiver();
                }
                break;
            }
            default: break;
        }
        return 0;
    }

private:
    Usart(uint8_t index, hal_pin_t txPin, hal_pin_t rxPin, hal_pin_t ctsPin, hal_pin_t rtsPin)
            : txPin_(txPin),
              rxPin_(rxPin),
              ctsPin_(ctsPin),
              rtsPin_(rtsPin),
              txDmaInitStruct_{},
              rxDmaInitStruct_{},
              config_(),
              curTxCount_(0),
              state_(HAL_USART_STATE_DISABLED),
              transmitting_(false),
              receiving_(false),
              configured_(false),
              index_(index) {
        // LOG UART is enabled on boot
        if (index_ == 2) {
            state_ = HAL_USART_STATE_ENABLED;
        }
    }
    ~Usart() = default;

    bool validateConfig(unsigned int config) {
        CHECK_TRUE((config & SERIAL_DATA_BITS) == SERIAL_DATA_BITS_7 ||
                   (config & SERIAL_DATA_BITS) == SERIAL_DATA_BITS_8, false);
        CHECK_TRUE((config & SERIAL_STOP_BITS) == SERIAL_STOP_BITS_1 ||
                   (config & SERIAL_STOP_BITS) == SERIAL_STOP_BITS_2, false);
        CHECK_TRUE((config & SERIAL_PARITY) == SERIAL_PARITY_NO ||
                   (config & SERIAL_PARITY) == SERIAL_PARITY_EVEN ||
                   (config & SERIAL_PARITY) == SERIAL_PARITY_ODD, false);
        CHECK_TRUE((config & SERIAL_HALF_DUPLEX) == 0, false);
        CHECK_TRUE((config & LIN_MODE) == 0, false);
        return true;
    }

    bool initDmaChannels() {
        auto uartInstance = uartTable_[index_].UARTx;
        UART_TXDMAConfig(uartInstance, 8);
        UART_TXDMACmd(uartInstance, ENABLE);
        UART_RXDMAConfig(uartInstance, 4);
	    UART_RXDMACmd(uartInstance, ENABLE);

        _memset(&txDmaInitStruct_, 0, sizeof(GDMA_InitTypeDef));
        _memset(&rxDmaInitStruct_, 0, sizeof(GDMA_InitTypeDef));
        txDmaInitStruct_.GDMA_ChNum = 0xFF;
        rxDmaInitStruct_.GDMA_ChNum = 0xFF;
        uint8_t txChannel = GDMA_ChnlAlloc(0, (IRQ_FUN)uartTxDmaCompleteHandler, (uint32_t)this, 12);//ACUT is 0x10, BCUT is 12
        if (txChannel == 0xFF) {
            return false;
        }
        uint8_t rxChannel = GDMA_ChnlAlloc(0, (IRQ_FUN)uartRxDmaCompleteHandler, (uint32_t)this, 12);
        if (rxChannel == 0xFF) {
            GDMA_ChnlFree(0, txChannel);
            txChannel = 0xFF;
            return false;
        }
        txDmaInitStruct_.MuliBlockCunt = 0;
        txDmaInitStruct_.MaxMuliBlock = 1;
        txDmaInitStruct_.GDMA_DIR = TTFCMemToPeri;
        txDmaInitStruct_.GDMA_DstHandshakeInterface = uartTable_[index_].Tx_HandshakeInterface;
        txDmaInitStruct_.GDMA_DstAddr = (u32)&uartInstance->RB_THR;
        txDmaInitStruct_.GDMA_Index = 0;
        txDmaInitStruct_.GDMA_ChNum = txChannel;
        txDmaInitStruct_.GDMA_IsrType = (BlockType|TransferType|ErrType);
        txDmaInitStruct_.GDMA_DstMsize = MsizeFour;
        txDmaInitStruct_.GDMA_DstDataWidth = TrWidthOneByte;
        txDmaInitStruct_.GDMA_DstInc = NoChange;
        txDmaInitStruct_.GDMA_SrcInc = IncType;

        rxDmaInitStruct_.MuliBlockCunt = 0;
        rxDmaInitStruct_.MaxMuliBlock = 1;
        rxDmaInitStruct_.GDMA_ReloadSrc = 0;
        rxDmaInitStruct_.GDMA_SrcHandshakeInterface = uartTable_[index_].Rx_HandshakeInterface;
        rxDmaInitStruct_.GDMA_SrcAddr = (u32)&uartInstance->RB_THR;
        rxDmaInitStruct_.GDMA_Index = 0;
        rxDmaInitStruct_.GDMA_ChNum = rxChannel;
        rxDmaInitStruct_.GDMA_IsrType = (BlockType|TransferType|ErrType);
        rxDmaInitStruct_.GDMA_SrcMsize = MsizeFour;
        rxDmaInitStruct_.GDMA_SrcDataWidth = TrWidthOneByte;
        rxDmaInitStruct_.GDMA_DstInc = IncType;
        rxDmaInitStruct_.GDMA_SrcInc = NoChange;

        NVIC_SetPriority(GDMA_GetIrqNum(0, txDmaInitStruct_.GDMA_ChNum), 12);
        NVIC_SetPriority(GDMA_GetIrqNum(0, rxDmaInitStruct_.GDMA_ChNum), 12);
        return true;
    }

    void deinitDmaChannels() {
        auto uartInstance = uartTable_[index_].UARTx;
        GDMA_Cmd(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum, DISABLE);
        GDMA_Cmd(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, DISABLE);
        // Clean Auto Reload Bit
        GDMA_ChCleanAutoReload(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum, CLEAN_RELOAD_DST);
        GDMA_ChCleanAutoReload(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, CLEAN_RELOAD_SRC);
        // Clear Pending ISR
        GDMA_ClearINT(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum);
        GDMA_ClearINT(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum);
        if (txDmaInitStruct_.GDMA_ChNum != 0xFF) {
            GDMA_ChnlFree(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum);
        }
        if (rxDmaInitStruct_.GDMA_ChNum != 0xFF) {
            GDMA_ChnlFree(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum);
        }
        UART_TXDMACmd(uartInstance, DISABLE);
        UART_RXDMACmd(uartInstance, DISABLE);
    }

    void startTransmission() {
        size_t consumable;
        auto uartInstance = uartTable_[index_].UARTx;
        if (!transmitting_ && (consumable = txBuffer_.consumable())) {
            transmitting_ = true;
            if (uartInstance != UART2_DEV) {
                if (txDmaInitStruct_.GDMA_ChNum == 0xFF) {
                    transmitting_ = false;
                    return;
                }
                consumable = std::min(MAX_DMA_BLOCK_SIZE, consumable);
                auto ptr = txBuffer_.consume(consumable);

                DCache_CleanInvalidate((uint32_t)ptr, consumable);
                
                if (((consumable & 0x03) == 0) && (((uint32_t)(ptr) & 0x03) == 0)) {
                    // 4-bytes aligned, move 4 bytes each transfer
                    txDmaInitStruct_.GDMA_SrcMsize   = MsizeOne;
                    txDmaInitStruct_.GDMA_SrcDataWidth = TrWidthFourBytes;
                    txDmaInitStruct_.GDMA_BlockSize = consumable >> 2;
                } else {
                    // Move 1 byte each transfer
                    txDmaInitStruct_.GDMA_SrcMsize   = MsizeFour;
                    txDmaInitStruct_.GDMA_SrcDataWidth = TrWidthOneByte;
                    txDmaInitStruct_.GDMA_BlockSize = consumable;
                }
                txDmaInitStruct_.GDMA_SrcAddr = (uint32_t)(ptr);
                GDMA_Init(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum, &txDmaInitStruct_);
                GDMA_Cmd(txDmaInitStruct_.GDMA_Index, txDmaInitStruct_.GDMA_ChNum, ENABLE);
            } else {
                // LOG UART doesn't support DMA transmission
                consumable = std::min(MAX_UART_FIFO_SIZE, consumable);
                auto ptr = txBuffer_.consume(consumable);
                for (size_t i = 0; i < consumable; i++, ptr++) {
                    UART_CharPut(uartInstance, *ptr);
                }
                UART_INTConfig(uartInstance, RUART_IER_ETBEI, ENABLE);
            }
            curTxCount_ = consumable;
        }
    }

    void startReceiver() {
        if (receiving_) {
            return;
        }
        receiving_ = true;
        auto uartInstance = uartTable_[index_].UARTx;
        if (uartInstance != UART2_DEV) {
            if (rxDmaInitStruct_.GDMA_ChNum == 0xFF) {
                receiving_ = false;
                return;
            }
            // Updates current size
            rxBuffer_.acquireBegin();
            const size_t acquirable = rxBuffer_.acquirable();
            const size_t acquirableWrapped = rxBuffer_.acquirableWrapped();
            size_t rxSize = std::max(acquirable, acquirableWrapped);
            if (rxSize == 0) {
                receiving_ = false;
                return;
            }
            rxSize = std::min(MAX_DMA_BLOCK_SIZE, rxSize);
            // Disable Rx interrupt
            UART_INTConfig(uartInstance, (RUART_IER_ERBI | RUART_IER_ELSI | RUART_IER_ETOI), DISABLE);
            auto ptr = rxBuffer_.acquire(rxSize);
            DCache_CleanInvalidate((uint32_t)ptr, rxSize);
            // Configure GDMA as the flow controller
            rxDmaInitStruct_.GDMA_DIR = TTFCPeriToMem;
            uartInstance->MISCR &= (~RUART_RXDMA_OWNER);
            // Move 1 byte each DMA transaction
            rxDmaInitStruct_.GDMA_DstMsize = MsizeFour;
            rxDmaInitStruct_.GDMA_DstDataWidth = TrWidthOneByte;
            rxDmaInitStruct_.GDMA_BlockSize = rxSize;
            rxDmaInitStruct_.GDMA_DstAddr = (uint32_t)(ptr);
            GDMA_Init(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, &rxDmaInitStruct_);
            GDMA_Cmd(rxDmaInitStruct_.GDMA_Index, rxDmaInitStruct_.GDMA_ChNum, ENABLE);
        } else {
            uint8_t temp[MAX_UART_FIFO_SIZE];
            uint32_t inFifo = UART_ReceiveDataTO(uartInstance, temp, MAX_UART_FIFO_SIZE, 1);
            rxBuffer_.put(temp, inFifo);
            UART_INTConfig(uartInstance, RUART_IER_ERBI | RUART_IER_ELSI | RUART_IER_ETOI, ENABLE);
        }
    }

    static uint32_t uartTxDmaCompleteHandler(void* data) {
        auto uart = (Usart*)data;
        if (!uart->transmitting_) {
            return 0;
        }
        uart->transmitting_ = false;
        auto txDmaInitStruct = &uart->txDmaInitStruct_;
        // Clean Auto Reload Bit
        GDMA_ChCleanAutoReload(txDmaInitStruct->GDMA_Index, txDmaInitStruct->GDMA_ChNum, CLEAN_RELOAD_DST);
        // Clear Pending ISR
        GDMA_ClearINT(txDmaInitStruct->GDMA_Index, txDmaInitStruct->GDMA_ChNum);
        GDMA_Cmd(txDmaInitStruct->GDMA_Index, txDmaInitStruct->GDMA_ChNum, DISABLE);
        uart->txBuffer_.consumeCommit(uart->curTxCount_);
        uart->startTransmission();
        return 0;
    }

    static uint32_t uartRxDmaCompleteHandler(void* data) {
        auto uart = (Usart*)data;
        if (!uart->receiving_) {
            return 0;
        }
        uart->receiving_ = false;
        auto rxDmaInitStruct = &uart->rxDmaInitStruct_;
        DCache_Invalidate((uint32_t)rxDmaInitStruct->GDMA_DstAddr, rxDmaInitStruct->GDMA_BlockSize);
        // Clean Auto Reload Bit
	    GDMA_ChCleanAutoReload(rxDmaInitStruct->GDMA_Index, rxDmaInitStruct->GDMA_ChNum, CLEAN_RELOAD_SRC);
        // Clear Pending ISR
	    GDMA_ClearINT(rxDmaInitStruct->GDMA_Index, rxDmaInitStruct->GDMA_ChNum);
        GDMA_Cmd(rxDmaInitStruct->GDMA_Index, rxDmaInitStruct->GDMA_ChNum, DISABLE);
        uart->rxBuffer_.acquireCommit(rxDmaInitStruct->GDMA_BlockSize);
        uart->startReceiver();
        return 0;
    }

private:
    hal_pin_t txPin_;
    hal_pin_t rxPin_;
    hal_pin_t ctsPin_;
    hal_pin_t rtsPin_;

    GDMA_InitTypeDef txDmaInitStruct_;
    GDMA_InitTypeDef rxDmaInitStruct_;

    Config config_;

    particle::services::RingBuffer<uint8_t> txBuffer_;
    particle::services::RingBuffer<uint8_t> rxBuffer_;
    volatile size_t curTxCount_;

    volatile hal_usart_state_t state_;
    volatile bool transmitting_;
    volatile bool receiving_;
    bool configured_;

    uint8_t index_; // of UART_DEV_TABLE that is defined in rtl8721d_uart.c
    const UART_DevTable* uartTable_ = UART_DEV_TABLE;

    static constexpr size_t MAX_DMA_BLOCK_SIZE = 4096;
    static constexpr size_t MAX_UART_FIFO_SIZE = 16;
};


int hal_usart_init_ex(hal_usart_interface_t serial, const hal_usart_buffer_config_t* config, void*) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    CHECK_TRUE(config, SYSTEM_ERROR_INVALID_ARGUMENT);
    return usart->init(*config);
}

void hal_usart_init(hal_usart_interface_t serial, hal_usart_ring_buffer_t* rxBuffer, hal_usart_ring_buffer_t* txBuffer) {
    hal_usart_buffer_config_t conf = {
        .size = sizeof(hal_usart_buffer_config_t),
        .rx_buffer = rxBuffer->buffer,
        .rx_buffer_size = sizeof(rxBuffer->buffer),
        .tx_buffer = txBuffer->buffer,
        .tx_buffer_size = sizeof(txBuffer->buffer)
    };

    hal_usart_init_ex(serial, &conf, nullptr);
}

void hal_usart_begin_config(hal_usart_interface_t serial, uint32_t baud, uint32_t config, void*) {
    auto usart = Usart::getInstance(serial);
    if (!usart) {
        return;
    }
    Usart::Config conf = {
        .baudRate = baud,
        .config = config
    };
    usart->begin(conf);
}

void hal_usart_begin(hal_usart_interface_t serial, uint32_t baud) {
    hal_usart_begin_config(serial, baud, SERIAL_8N1, nullptr);
}

void hal_usart_end(hal_usart_interface_t serial) {
    auto usart = Usart::getInstance(serial);
    if (!usart) {
        return;
    }
    usart->end();
}

int32_t hal_usart_available_data_for_write(hal_usart_interface_t serial) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    return usart->space();
}

int32_t hal_usart_available(hal_usart_interface_t serial) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    return usart->data();
}

void hal_usart_flush(hal_usart_interface_t serial) {
    auto usart = Usart::getInstance(serial);
    if (!usart) {
        return;
    }
    usart->flush();
}

bool hal_usart_is_enabled(hal_usart_interface_t serial) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), false);
    return usart->isEnabled();
}

ssize_t hal_usart_write_buffer(hal_usart_interface_t serial, const void* buffer, size_t size, size_t elementSize) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    CHECK_TRUE(elementSize == sizeof(uint8_t), SYSTEM_ERROR_INVALID_ARGUMENT);
    return usart->write((const uint8_t*)buffer, size);
}

ssize_t hal_usart_read_buffer(hal_usart_interface_t serial, void* buffer, size_t size, size_t elementSize) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    CHECK_TRUE(elementSize == sizeof(uint8_t), SYSTEM_ERROR_INVALID_ARGUMENT);
    return usart->read((uint8_t*)buffer, size);
}

ssize_t hal_usart_peek_buffer(hal_usart_interface_t serial, void* buffer, size_t size, size_t elementSize) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    CHECK_TRUE(elementSize == sizeof(uint8_t), SYSTEM_ERROR_INVALID_ARGUMENT);
    return usart->peek((uint8_t*)buffer, size);
}

int32_t hal_usart_read(hal_usart_interface_t serial) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    uint8_t c;
    CHECK(usart->read(&c, sizeof(c)));
    return c;
}

int32_t hal_usart_peek(hal_usart_interface_t serial) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    uint8_t c;
    CHECK(usart->peek(&c, sizeof(c)));
    return c;
}

uint32_t hal_usart_write_nine_bits(hal_usart_interface_t serial, uint16_t data) {
    return hal_usart_write(serial, data);
}

uint32_t hal_usart_write(hal_usart_interface_t serial, uint8_t data) {
    auto usart = CHECK_TRUE_RETURN(Usart::getInstance(serial), SYSTEM_ERROR_NOT_FOUND);
    // Blocking!
    while (usart->space() <= 0) {
        // Poll the status just in case that the interrupt handler is not invoked even if there is int pending.
        usart->uartTxRxIntHandler(usart);
    }
    return CHECK_RETURN(usart->write(&data, sizeof(data)), 0);
}

void hal_usart_send_break(hal_usart_interface_t serial, void* reserved) {
    // Unsupported
}

uint8_t hal_usart_break_detected(hal_usart_interface_t serial) {
    // Unsupported
    return SYSTEM_ERROR_NONE;
}

void hal_usart_half_duplex(hal_usart_interface_t serial, bool enable) {
    // Unsupported
}

int hal_usart_pvt_get_event_group_handle(hal_usart_interface_t serial, EventGroupHandle_t* handle) {
    return SYSTEM_ERROR_NONE;
}

int hal_usart_pvt_enable_event(hal_usart_interface_t serial, HAL_USART_Pvt_Events events) {
    return 0;
}

int hal_usart_pvt_disable_event(hal_usart_interface_t serial, HAL_USART_Pvt_Events events) {
    return 0;
}

int hal_usart_pvt_wait_event(hal_usart_interface_t serial, uint32_t events, system_tick_t timeout) {
    return 0;
}

int hal_usart_sleep(hal_usart_interface_t serial, bool sleep, void* reserved) {
    return 0;
}
