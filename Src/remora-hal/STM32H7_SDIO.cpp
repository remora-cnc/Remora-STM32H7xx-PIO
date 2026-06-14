/**
 * STM32H7_SDIO.cpp
 *
 * HAL class for SDMMC-connected SD card on STM32H7.
 * Uses pinmap tables for GPIO/peripheral resolution.
 *
 * HAL_SD_MspInit() is implemented here as a non-weak override. It calls
 * back into the class instance via g_sdio to configure GPIO, clock and NVIC
 * using the pinmap-resolved pins rather than hardcoded values.
 *
 * hsd must be zero-initialised (hsd{} in initialiser list) so HAL_SD_Init()
 * sees hsd.State == HAL_SD_STATE_RESET and fires MspInit.
 *
 * hsd1 is declared here to satisfy bsp_driver_sd.c and stm32h7xx_it.c.
 * It is synced from hsd after HAL_SD_Init() and before MX_FATFS_Init().
 */

#include "STM32H7_SDIO.h"
#include "PinNamesTypes.h"   /* STM_PIN_AFNUM, STM_PIN_PUPD — .cpp only      */
#include "fatfs.h"
#include <cstdio>

/* ── Global instance pointer — used by HAL_SD_MspInit callback ──────────── */
STM32H7_SDIO *g_sdio = nullptr;

/* ── hsd1 global — referenced by bsp_driver_sd.c and stm32h7xx_it.c ─────── */
SD_HandleTypeDef hsd1;

/* ── HAL_SD_MspInit — non-weak override ─────────────────────────────────── */
/* Called automatically by HAL_SD_Init(). Delegates to the class instance   */
/* so GPIO, clock and NVIC are configured from pinmap-resolved data.         */
extern "C" void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    if (g_sdio != nullptr)
    {
        g_sdio->mspInit(hsd);
    }
}

/* ── Constructor ─────────────────────────────────────────────────────────── */
STM32H7_SDIO::STM32H7_SDIO(
    std::string cmd,
    std::string clk,
    std::string d0,
    std::string d1,
    std::string d2,
    std::string d3)
    : cmdPortAndPin(cmd)
    , clkPortAndPin(clk)
    , d0PortAndPin(d0)
    , d1PortAndPin(d1)
    , d2PortAndPin(d2)
    , d3PortAndPin(d3)
    , cmdPin(nullptr)
    , clkPin(nullptr)
    , d0Pin(nullptr)
    , d1Pin(nullptr)
    , d2Pin(nullptr)
    , d3Pin(nullptr)
    , hsd{}              /* zero-init: hsd.State == HAL_SD_STATE_RESET        */
    , _mounted(false)
{
    _cmdPinName = portAndPinToPinName(cmd.c_str());
    _clkPinName = portAndPinToPinName(clk.c_str());
    _d0PinName  = portAndPinToPinName(d0.c_str());
    _d1PinName  = portAndPinToPinName(d1.c_str());
    _d2PinName  = portAndPinToPinName(d2.c_str());
    _d3PinName  = portAndPinToPinName(d3.c_str());

    hsd.Instance = (SDMMC_TypeDef*)getSDMMCPeripheral();

    /* Register instance so HAL_SD_MspInit can call back into us             */
    g_sdio = this;
}

/* ── mspInit() — called from HAL_SD_MspInit callback ───────────────────── */
void STM32H7_SDIO::mspInit(SD_HandleTypeDef *hsd_ptr)
{
    if (hsd_ptr->Instance != hsd.Instance) return;

    /* ── SDMMC kernel clock ─────────────────────────────────────────────── */
    /* Reconfigure to PLL1 Q — PeriphCommonClock_Config sets PLL2 which     */
    /* does not work for SDMMC. Must use RCC_SDMMCCLKSOURCE_PLL (PLL1 Q).   */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
    PeriphClkInit.SdmmcClockSelection  = RCC_SDMMCCLKSOURCE_PLL;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
        return;
    }

    /* ── Peripheral clock ────────────────────────────────────────────────── */
    if      (hsd.Instance == SDMMC1) __HAL_RCC_SDMMC1_CLK_ENABLE();
#if defined(SDMMC2)
    else if (hsd.Instance == SDMMC2) __HAL_RCC_SDMMC2_CLK_ENABLE();
#endif

    /* ── GPIO — configure pins via pinmap-resolved Pin objects ─────────── */
    /* Pins are created here inside MspInit so they exist for the lifetime  */
    /* of the peripheral. Ownership stays with the class.                   */
    cmdPin = createPin(cmdPortAndPin, _cmdPinName, PinMap_SDMMC_CMD);
    clkPin = createPin(clkPortAndPin, _clkPinName, PinMap_SDMMC_CK);
    d0Pin  = createPin(d0PortAndPin,  _d0PinName,  PinMap_SDMMC_D0);
    if (_d1PinName != NC) d1Pin = createPin(d1PortAndPin, _d1PinName, PinMap_SDMMC_D1);
    if (_d2PinName != NC) d2Pin = createPin(d2PortAndPin, _d2PinName, PinMap_SDMMC_D2);
    if (_d3PinName != NC) d3Pin = createPin(d3PortAndPin, _d3PinName, PinMap_SDMMC_D3);

    /* ── NVIC ────────────────────────────────────────────────────────────── */
    if (hsd.Instance == SDMMC1)
    {
        HAL_NVIC_SetPriority(SDMMC1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
    }
#if defined(SDMMC2)
    else if (hsd.Instance == SDMMC2)
    {
        HAL_NVIC_SetPriority(SDMMC2_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(SDMMC2_IRQn);
    }
#endif
}

/* ── init() ──────────────────────────────────────────────────────────────── */
void STM32H7_SDIO::init()
{
    printf("STM32H7_SDIO: initialising\r\n");

    hsd.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    hsd.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_ENABLE;
    hsd.Init.BusWide             = (_d1PinName != NC) ? SDMMC_BUS_WIDE_4B
                                                       : SDMMC_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv            = 8;

    /* Fires HAL_SD_MspInit() → STM32H7_SDIO::mspInit() above               */
    if (HAL_SD_Init(&hsd) != HAL_OK)
    {
        printf("STM32H7_SDIO: HAL_SD_Init FAILED error=0x%08lX\r\n",
               hsd.ErrorCode);
        Error_Handler();
        return;
    }

    /* Sync hsd1 BEFORE MX_FATFS_Init so BSP_SD_GetCardState works          */
    hsd1 = hsd;

    MX_FATFS_Init();

    _mounted = true;
    printf("STM32H7_SDIO: SD card ready\r\n");
}

/* ── Private helpers ─────────────────────────────────────────────────────── */

uint32_t STM32H7_SDIO::getSDMMCPeripheral()
{
    uint32_t periph = pinmap_peripheral(_cmdPinName, PinMap_SDMMC_CMD);
    periph = pinmap_merge(periph, pinmap_peripheral(_clkPinName, PinMap_SDMMC_CK));
    periph = pinmap_merge(periph, pinmap_peripheral(_d0PinName,  PinMap_SDMMC_D0));
    if (_d1PinName != NC) periph = pinmap_merge(periph, pinmap_peripheral(_d1PinName, PinMap_SDMMC_D1));
    if (_d2PinName != NC) periph = pinmap_merge(periph, pinmap_peripheral(_d2PinName, PinMap_SDMMC_D2));
    if (_d3PinName != NC) periph = pinmap_merge(periph, pinmap_peripheral(_d3PinName, PinMap_SDMMC_D3));
    return periph;
}

Pin* STM32H7_SDIO::createPin(const std::string& portAndPin,
                              PinName pinName, const PinMap* map)
{
    uint32_t af   = STM_PIN_AFNUM(pinmap_function(pinName, map));
    uint32_t pull = STM_PIN_PUPD(pinmap_function(pinName, map));

    uint32_t halPull;
    switch (pull) {
        case 1:  halPull = GPIO_PULLUP;   break;
        case 2:  halPull = GPIO_PULLDOWN; break;
        default: halPull = GPIO_NOPULL;   break;
    }

    return new Pin(portAndPin, GPIO_MODE_AF_PP,
                   halPull, GPIO_SPEED_FREQ_VERY_HIGH, af);
}

SD_HandleTypeDef* STM32H7_SDIO::getHandle()
{
    return &hsd;
}