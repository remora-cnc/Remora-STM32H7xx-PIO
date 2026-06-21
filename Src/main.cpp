/*
  Remora is a free, opensource LinuxCNC component and Programmable Realtime
  Unit (PRU) firmware to implement a LinuxCNC based CNC controller.
  Copyright (C) 2025 Scott Alford (scotta)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "main.h"
#include "fatfs.h"

#include <stdio.h>
#include <cstring>
#include <sys/errno.h>

#include "remora-core/remora.h"
#include "remora-core/modules/comms/commsHandler.h"
#include "remora-hal/STM32H7_SPIComms.h"
#include "remora-hal/STM32H7_timer.h"
#include "remora-hal/STM32H7_UART.h"
#include "remora-hal/STM32H7_SDIO.h"

#ifdef SD_SPI
  #include "remora-hal/STM32H7_SPI_SDcard.h"
#endif

/* hsd1 is declared in STM32H7_SDIO.cpp and referenced by bsp_driver_sd.c
 * and stm32h7xx_it.c. Declared extern here to keep main.cpp clean.         */
extern SD_HandleTypeDef hsd1;

void SystemClock_Config(void);
static void MPU_Config(void);

/* printf retargeting via g_uart set during STM32H7_UART construction       */
extern "C" {
    int __io_putchar(int ch)
    {
        if (g_uart) g_uart->putChar((uint8_t)ch);
        return ch;
    }
}

int main(void)
{
#ifdef HAS_BOOTLOADER
    HAL_RCC_DeInit();
    HAL_DeInit();
    extern uint8_t _FLASH_VectorTable;
    __disable_irq();
    SCB->VTOR = (uint32_t)&_FLASH_VectorTable;
    __DSB();
    __enable_irq();
#endif

    MPU_Config();
    HAL_Init();
    SystemClock_Config();

    /* Enable instruction cache                                               */
    SCB_InvalidateICache();
    SCB_EnableICache();

    __HAL_RCC_DMA1_CLK_ENABLE();

    /* ── UART ────────────────────────────────────────────────────────────── */
    auto uart = std::make_unique<STM32H7_UART>(REMORA_UART_TX, REMORA_UART_RX);
    uart->init();

    /* ── SD card ─────────────────────────────────────────────────────────── */
#ifdef SD_SPI
    auto sdCard = std::make_unique<STM32H7_SPI_SDcard>(
                      SD_MOSI, SD_MISO, SD_CLK, SD_CS);
    sdCard->init();
#else
    auto sdCard = std::make_unique<STM32H7_SDIO>(
                      REMORA_SD_CMD, REMORA_SD_CLK,
                      REMORA_SD_D0,  REMORA_SD_D1,
                      REMORA_SD_D2,  REMORA_SD_D3);
    sdCard->init();
#endif

    MX_FATFS_Init();

    /* ── LinuxCNC comms SPI ───────────────────────────────────────────────── */
    auto comms = std::make_unique<STM32H7_SPIComms>(
                     &rxData, &txData,
                     SPI_MOSI, SPI_MISO, SPI_CLK, SPI_CS);
    auto commsHandler = std::make_shared<CommsHandler>();
    commsHandler->setInterface(std::move(comms));

    /* ── Timers ───────────────────────────────────────────────────────────── */
    auto baseTimer   = std::make_unique<STM32H7_timer>(
                           TIM3, TIM3_IRQn,
                           Config::pruBaseFreq, nullptr,
                           Config::baseThreadIrqPriority);
    auto servoTimer  = std::make_unique<STM32H7_timer>(
                           TIM2, TIM2_IRQn,
                           Config::pruServoFreq, nullptr,
                           Config::servoThreadIrqPriority);
    auto serialTimer = std::make_unique<STM32H7_timer>(
                           TIM4, TIM4_IRQn,
                           Config::pruSerialFreq, nullptr,
                           Config::serialThreadIrqPriority);

    /* ── Remora ───────────────────────────────────────────────────────────── */
    Remora* remora = new Remora(
        commsHandler,
        std::move(baseTimer),
        std::move(servoTimer),
        std::move(serialTimer)
    );

    remora->run();
}

/* ── System Clock Configuration ─────────────────────────────────────────── */
/*
 * Supports four configurations selected by preprocessor defines:
 *
 *   STM32H743xx + NUCLEO_H743  — 480 MHz, 8 MHz HSE (Nucleo STLink MCO)
 *   STM32H743xx                — 480 MHz, 25 MHz HSE (SKR3, WeAct H743)
 *   STM32H723xx + NUCLEO_H723  — 550 MHz, 8 MHz HSE (Nucleo STLink MCO)
 *   STM32H723xx                — 550 MHz, 25 MHz HSE (Manta M8P V2.0)
 *
 * PLL2 provides the kernel clock for SPI1/2/3 and SDMMC peripherals.
 * SDMMC is later reconfigured to PLL1 Q inside STM32H7_SDIO::mspInit().
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef       RCC_OscInitStruct   = {0};
    RCC_ClkInitTypeDef       RCC_ClkInitStruct   = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
    __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLRGE    = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN  = 0;

#if defined(STM32H743xx)

  #if defined(NUCLEO_H743)
    /* Nucleo H743 — 8 MHz HSE (STLink MCO) → 480 MHz SYSCLK               */
    RCC_OscInitStruct.PLL.PLLM = 2;
    RCC_OscInitStruct.PLL.PLLN = 240;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 20;
    RCC_OscInitStruct.PLL.PLLR = 2;

    PeriphClkInitStruct.PLL2.PLL2M      = 2;
    PeriphClkInitStruct.PLL2.PLL2N      = 240;
    PeriphClkInitStruct.PLL2.PLL2P      = 2;
    PeriphClkInitStruct.PLL2.PLL2Q      = 20;
    PeriphClkInitStruct.PLL2.PLL2R      = 80;
    PeriphClkInitStruct.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_2;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN  = 0;
    #define FLASH_LATENCY FLASH_LATENCY_4

  #else
    /* SKR3 / WeAct H743 — 25 MHz HSE → 480 MHz SYSCLK                      */
    /* PLL1: 25/5 = 5 MHz × 192 / 2 = 480 MHz                               */
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 192;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;

    /* PLL2: 25/2 = 12.5 MHz × 12 = 150 MHz VCO → /2 = 75 MHz SPI/SDMMC   */
    PeriphClkInitStruct.PLL2.PLL2M      = 2;
    PeriphClkInitStruct.PLL2.PLL2N      = 12;
    PeriphClkInitStruct.PLL2.PLL2P      = 1;
    PeriphClkInitStruct.PLL2.PLL2Q      = 10;
    PeriphClkInitStruct.PLL2.PLL2R      = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
    PeriphClkInitStruct.PLL2.PLL2FRACN  = 0;
    #define FLASH_LATENCY FLASH_LATENCY_4
  #endif

#elif defined(STM32H723xx)

  #if defined(NUCLEO_H723)
    /* Nucleo H723 — 8 MHz HSE (STLink MCO) → 550 MHz SYSCLK               */
    RCC_OscInitStruct.PLL.PLLM = 2;
    RCC_OscInitStruct.PLL.PLLN = 120;
    RCC_OscInitStruct.PLL.PLLP = 1;
    RCC_OscInitStruct.PLL.PLLQ = 10;
    RCC_OscInitStruct.PLL.PLLR = 1;

    PeriphClkInitStruct.PLL2.PLL2M      = 2;
    PeriphClkInitStruct.PLL2.PLL2N      = 120;
    PeriphClkInitStruct.PLL2.PLL2P      = 1;
    PeriphClkInitStruct.PLL2.PLL2Q      = 10;
    PeriphClkInitStruct.PLL2.PLL2R      = 40;
    PeriphClkInitStruct.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_2;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN  = 0;
    #define FLASH_LATENCY FLASH_LATENCY_3

  #else
    /* Manta M8P V2.0 — 25 MHz HSE → 550 MHz SYSCLK                         */
    /* PLL1: 25/5 = 5 MHz × 110 / 1 = 550 MHz                               */
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 110;
    RCC_OscInitStruct.PLL.PLLP = 1;
    RCC_OscInitStruct.PLL.PLLQ = 10;
    RCC_OscInitStruct.PLL.PLLR = 10;

    /* PLL2: 25/2 = 12.5 MHz × 16 = 200 MHz VCO → /2 = 100 MHz SPI/SDMMC  */
    PeriphClkInitStruct.PLL2.PLL2M      = 2;
    PeriphClkInitStruct.PLL2.PLL2N      = 16;
    PeriphClkInitStruct.PLL2.PLL2P      = 1;
    PeriphClkInitStruct.PLL2.PLL2Q      = 10;
    PeriphClkInitStruct.PLL2.PLL2R      = 2;
    PeriphClkInitStruct.PLL2.PLL2RGE    = RCC_PLL2VCIRANGE_3;
    PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
    PeriphClkInitStruct.PLL2.PLL2FRACN  = 0;
    #define FLASH_LATENCY FLASH_LATENCY_3
  #endif

#endif /* STM32H723xx */

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK    | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1   | RCC_CLOCKTYPE_PCLK2  |
                                  RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY) != HAL_OK)
        Error_Handler();

    /* Peripheral kernel clocks from PLL2
     * SDMMC is set here to PLL2 then overridden to PLL1 Q inside
     * STM32H7_SDIO::mspInit() when HAL_SD_Init() fires.                    */
    PeriphClkInitStruct.PeriphClockSelection  = RCC_PERIPHCLK_SDMMC |
                                                RCC_PERIPHCLK_SPI123;
    PeriphClkInitStruct.SdmmcClockSelection   = RCC_SDMMCCLKSOURCE_PLL2;
    PeriphClkInitStruct.Spi123ClockSelection  = RCC_SPI123CLKSOURCE_PLL2;

    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        Error_Handler();
}

/* ── MPU Configuration ───────────────────────────────────────────────────── */
void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct = {0};

    HAL_MPU_Disable();

    MPU_InitStruct.Enable            = MPU_REGION_ENABLE;
    MPU_InitStruct.Number            = MPU_REGION_NUMBER0;
    MPU_InitStruct.BaseAddress       = 0x0;
    MPU_InitStruct.Size              = MPU_REGION_SIZE_4GB;
    MPU_InitStruct.SubRegionDisable  = 0x87;
    MPU_InitStruct.TypeExtField      = MPU_TEX_LEVEL0;
    MPU_InitStruct.AccessPermission  = MPU_REGION_NO_ACCESS;
    MPU_InitStruct.DisableExec       = MPU_INSTRUCTION_ACCESS_DISABLE;
    MPU_InitStruct.IsShareable       = MPU_ACCESS_SHAREABLE;
    MPU_InitStruct.IsCacheable       = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsBufferable      = MPU_ACCESS_NOT_BUFFERABLE;

    HAL_MPU_ConfigRegion(&MPU_InitStruct);
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/* ── Error handler ───────────────────────────────────────────────────────── */
void Error_Handler(void)
{
    printf("error\n\r");
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
