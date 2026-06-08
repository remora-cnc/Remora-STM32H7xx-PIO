/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : fatfs_platform.c
  * @brief          : fatfs_platform source file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
*/
/* USER CODE END Header */
#include "fatfs_platform.h"


uint8_t BSP_PlatformIsDetected(void) {
#ifdef SD_SPI
    /* No card-detect pin on SPI (EXP2) — assume card is present.
        * BSP_SD_Init() will return MSD_ERROR if the card is actually
        * absent, which is caught by SD_SPI_initialize() in sd_spi_diskio.c */
    return SD_PRESENT;
#else
    uint8_t status = SD_PRESENT;
    if(HAL_GPIO_ReadPin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) != GPIO_PIN_RESET)
    {
        status = SD_NOT_PRESENT;
    }
    return status;
#endif
}
