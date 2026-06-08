/* FATFS/SPI/bsp_driver_sd_spi.h
 *
 * BSP SD driver for SPI-connected SD cards.
 * Thin adapter between FatFs and STM32H7_SPI_SDcard class.
 * Selected by -D SD_SPI in platformio.ini.
 */

#ifndef BSP_DRIVER_SD_SPI_H
#define BSP_DRIVER_SD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "fatfs_platform.h"

/* ── Return / status codes (mirror bsp_driver_sd.h) ────────────────────── */
#define MSD_OK                   ((uint8_t)0x00)
#define MSD_ERROR                ((uint8_t)0x01)
#define MSD_ERROR_SD_NOT_PRESENT ((uint8_t)0x02)

#define SD_TRANSFER_OK           ((uint8_t)0x00)
#define SD_TRANSFER_BUSY         ((uint8_t)0x01)

#define SD_PRESENT               ((uint8_t)0x01)
#define SD_NOT_PRESENT           ((uint8_t)0x00)

#define SD_DATATIMEOUT           ((uint32_t)100000000)
#define SD_DEFAULT_BLOCK_SIZE    512U

/* ── Card info (used by sd_spi_diskio.c ioctl) ──────────────────────────── */
typedef struct {
    uint32_t LogBlockNbr;
    uint32_t LogBlockSize;
    uint8_t  CardType;
} BSP_SD_SPI_CardInfo;

/* ── Public API ─────────────────────────────────────────────────────────── */
uint8_t BSP_SD_Init(void);
uint8_t BSP_SD_GetCardState(void);
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr,
                          uint32_t NumOfBlocks, uint32_t Timeout);
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr,
                           uint32_t NumOfBlocks, uint32_t Timeout);
void    BSP_SD_GetCardInfo(BSP_SD_SPI_CardInfo *CardInfo);
uint8_t BSP_SD_IsDetected(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_DRIVER_SD_SPI_H */
