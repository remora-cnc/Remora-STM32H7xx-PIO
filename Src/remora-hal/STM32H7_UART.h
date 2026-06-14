/**
 * STM32H7_UART.h
 *
 * HAL class for UART on STM32H7.
 * Follows the same pattern as STM32H7_SPIComms and STM32H7_SPI_SDcard.
 *
 * printf retargeting: __io_putchar() in main.cpp uses g_uart to transmit,
 * set automatically when the class is constructed.
 */

#pragma once

#include "stm32h7xx_hal.h"
#include "remora-core/configuration.h"
#include "peripheralPins.h"
#include "pin/pin.h"

#include <string>

class STM32H7_UART
{
public:
    STM32H7_UART(std::string tx, std::string rx);
    ~STM32H7_UART();

    void init();

    UART_HandleTypeDef* getHandle();

    /* Transmit a single byte — called by __io_putchar                       */
    void putChar(uint8_t ch);

private:
    std::string txPortAndPin;
    std::string rxPortAndPin;

    PinName txPinName;
    PinName rxPinName;

    Pin *txPin;
    Pin *rxPin;

    UART_HandleTypeDef huart;

    USART_TypeDef* getUARTPeripheral(PinName tx, PinName rx);
    Pin*           createPin(const std::string& portAndPin, PinName pinName,
                             const PinMap* map);
    void           enableClock(USART_TypeDef* instance);
};

/* Global pointer set by constructor — used by __io_putchar in main.cpp     */
extern STM32H7_UART *g_uart;
