/**
 * STM32H7_SDIO.h
 *
 * HAL class for SDMMC-connected SD card on STM32H7.
 * Takes pin strings and resolves GPIO/peripheral via pinmap — consistent
 * with STM32H7_SPIComms and STM32H7_SPI_SDcard.
 *
 * HAL_SD_MspInit() is implemented in STM32H7_SDIO.cpp as a non-weak
 * override. It calls g_sdio->mspInit() to configure GPIO (via pinmap),
 * SDMMC kernel clock (PLL1 Q), and NVIC — all from class state.
 *
 * NOTE: PinNamesTypes.h has no include guard — include in .cpp only.
 */

#pragma once

#include "stm32h7xx_hal.h"
#include "peripheralPins.h"
#include "pin/pin.h"
#include "fatfs.h"

#include <string>

class STM32H7_SDIO
{
public:
    STM32H7_SDIO(std::string cmd,
                 std::string clk,
                 std::string d0,
                 std::string d1,
                 std::string d2,
                 std::string d3);

    void init();

    /* Called from HAL_SD_MspInit() — public so the C callback can reach it */
    void mspInit(SD_HandleTypeDef *hsd);

    bool isMounted() const { return _mounted; }

    SD_HandleTypeDef* getHandle();

private:
    std::string cmdPortAndPin;
    std::string clkPortAndPin;
    std::string d0PortAndPin;
    std::string d1PortAndPin;
    std::string d2PortAndPin;
    std::string d3PortAndPin;

    PinName _cmdPinName;
    PinName _clkPinName;
    PinName _d0PinName;
    PinName _d1PinName;
    PinName _d2PinName;
    PinName _d3PinName;

    Pin *cmdPin;
    Pin *clkPin;
    Pin *d0Pin;
    Pin *d1Pin;
    Pin *d2Pin;
    Pin *d3Pin;

    SD_HandleTypeDef hsd;
    bool           _mounted;

    uint32_t getSDMMCPeripheral();
    Pin*     createPin(const std::string& portAndPin, PinName pinName,
                       const PinMap* map);
};

/* Global instance pointer — set in constructor, used by HAL_SD_MspInit()   */
extern STM32H7_SDIO *g_sdio;