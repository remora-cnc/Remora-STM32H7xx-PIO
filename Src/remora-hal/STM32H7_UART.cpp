/**
 * STM32H7_UART.cpp
 */

#include "STM32H7_UART.h"
#include "PinNamesTypes.h"   /* STM_PIN_AFNUM — .cpp only, no include guard  */

#include <cstdio>

/* ── Global instance pointer — used by __io_putchar in main.cpp ─────────── */
STM32H7_UART *g_uart = nullptr;

STM32H7_UART::STM32H7_UART(std::string tx, std::string rx)
    : txPortAndPin(tx)
    , rxPortAndPin(rx)
    , txPin(nullptr)
    , rxPin(nullptr)
{
    txPinName = portAndPinToPinName(tx.c_str());
    rxPinName = portAndPinToPinName(rx.c_str());
    huart.Instance = getUARTPeripheral(txPinName, rxPinName);

    /* Register this instance immediately so printf works as soon as        */
    /* init() is called — even before the first character is sent           */
    g_uart = this;
}

STM32H7_UART::~STM32H7_UART()
{
    delete txPin;
    delete rxPin;
    if (g_uart == this) g_uart = nullptr;
}

void STM32H7_UART::init()
{
    txPin = createPin(txPortAndPin, txPinName, PinMap_UART_TX);
    rxPin = createPin(rxPortAndPin, rxPinName, PinMap_UART_RX);

    enableClock(huart.Instance);

    huart.Init.BaudRate       = Config::pcBaud;
    huart.Init.WordLength     = UART_WORDLENGTH_8B;
    huart.Init.StopBits       = UART_STOPBITS_1;
    huart.Init.Parity         = UART_PARITY_NONE;
    huart.Init.Mode           = UART_MODE_TX_RX;
    huart.Init.HwFlowCtl      = UART_HWCONTROL_NONE;
    huart.Init.OverSampling   = UART_OVERSAMPLING_16;
    huart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;  // ← add
    huart.Init.ClockPrescaler = UART_PRESCALER_DIV1;           // ← critical

    huart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT; // ← add

    if (HAL_UART_Init(&huart) != HAL_OK)
    {
        while (1) { ; }
    }

    // H7 FIFO mode — must be called after HAL_UART_Init
    HAL_UARTEx_SetTxFifoThreshold(&huart, UART_TXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_SetRxFifoThreshold(&huart, UART_RXFIFO_THRESHOLD_1_8);
    HAL_UARTEx_DisableFifoMode(&huart);
}

void STM32H7_UART::putChar(uint8_t ch)
{
    HAL_UART_Transmit(&huart, &ch, 1, 0xFFFF);
}

UART_HandleTypeDef* STM32H7_UART::getHandle()
{
    return &huart;
}

USART_TypeDef* STM32H7_UART::getUARTPeripheral(PinName tx, PinName rx)
{
    uint32_t tx_periph = pinmap_peripheral(tx, PinMap_UART_TX);
    uint32_t rx_periph = pinmap_peripheral(rx, PinMap_UART_RX);
    return (USART_TypeDef*)pinmap_merge(tx_periph, rx_periph);
}

Pin* STM32H7_UART::createPin(const std::string& portAndPin,
                              PinName pinName, const PinMap* map)
{
    uint32_t af = STM_PIN_AFNUM(pinmap_function(pinName, map));
    return new Pin(portAndPin, GPIO_MODE_AF_PP,
                   GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, af);
}

void STM32H7_UART::enableClock(USART_TypeDef* instance)
{
    if      (instance == USART1) __HAL_RCC_USART1_CLK_ENABLE();
    else if (instance == USART2) __HAL_RCC_USART2_CLK_ENABLE();
    else if (instance == USART3) __HAL_RCC_USART3_CLK_ENABLE();
    else if (instance == UART4)  __HAL_RCC_UART4_CLK_ENABLE();
    else if (instance == UART5)  __HAL_RCC_UART5_CLK_ENABLE();
    else if (instance == USART6) __HAL_RCC_USART6_CLK_ENABLE();
    else if (instance == UART7)  __HAL_RCC_UART7_CLK_ENABLE();
    else if (instance == UART8)  __HAL_RCC_UART8_CLK_ENABLE();
}
