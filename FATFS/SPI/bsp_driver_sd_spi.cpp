/* FATFS/SPI/bsp_driver_sd_spi.cpp
 *
 * BSP SD driver — SPI path, thin adapter over STM32H7_SPI_SDcard class.
 * Selected by -D SD_SPI in platformio.ini.
 *
 * Compiled as C++ (.cpp) so it can call methods on STM32H7_SPI_SDcard
 * via the g_sdCard global pointer. The public API is declared extern "C"
 * so sd_spi_diskio.c (compiled as C) can link against it without mangling.
 *
 * Address translation (block vs byte addressing for SDHC vs SDSC) lives
 * here — it belongs at the BSP layer, not in the protocol class.
 */

#ifdef SD_SPI

#include "STM32H7_SPI_SDcard.h"   /* g_sdCard, sd_card_type_t, sd_result_t  */
#include "bsp_driver_sd_spi.h"

/* ── Private state ───────────────────────────────────────────────────────── */
static uint8_t _initialised = 0;

/* ── Public API (extern "C" so sd_spi_diskio.c can link) ────────────────── */
extern "C" {

uint8_t BSP_SD_Init(void)
{
    if (_initialised) return MSD_OK;
    if (!g_sdCard)    return MSD_ERROR;

    sd_card_type_t ct;
    if (g_sdCard->cardInit(&ct) != SD_OK) return MSD_ERROR;

    _initialised = 1;
    return MSD_OK;
}

uint8_t BSP_SD_GetCardState(void)
{
    return _initialised ? SD_TRANSFER_OK : SD_TRANSFER_BUSY;
}

uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr,
                          uint32_t NumOfBlocks, uint32_t Timeout)
{
    (void)Timeout;
    if (!_initialised || !g_sdCard) return MSD_ERROR;

    uint8_t  *buf  = (uint8_t *)pData;
    uint32_t  addr = (g_sdCard->getCardType() == SD_CARD_SDV2_HC) ?
                     ReadAddr : ReadAddr * SD_DEFAULT_BLOCK_SIZE;

    for (uint32_t b = 0; b < NumOfBlocks; b++) {
        if (g_sdCard->readBlock(buf, addr) != SD_OK) return MSD_ERROR;
        buf  += SD_DEFAULT_BLOCK_SIZE;
        addr += (g_sdCard->getCardType() == SD_CARD_SDV2_HC) ?
                1UL : SD_DEFAULT_BLOCK_SIZE;
    }
    return MSD_OK;
}

uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr,
                           uint32_t NumOfBlocks, uint32_t Timeout)
{
    (void)Timeout;
    if (!_initialised || !g_sdCard) return MSD_ERROR;

    const uint8_t *buf  = (const uint8_t *)pData;
    uint32_t       addr = (g_sdCard->getCardType() == SD_CARD_SDV2_HC) ?
                          WriteAddr : WriteAddr * SD_DEFAULT_BLOCK_SIZE;

    for (uint32_t b = 0; b < NumOfBlocks; b++) {
        if (g_sdCard->writeBlock(buf, addr) != SD_OK) return MSD_ERROR;
        buf  += SD_DEFAULT_BLOCK_SIZE;
        addr += (g_sdCard->getCardType() == SD_CARD_SDV2_HC) ?
                1UL : SD_DEFAULT_BLOCK_SIZE;
    }
    return MSD_OK;
}

void BSP_SD_GetCardInfo(BSP_SD_SPI_CardInfo *CardInfo)
{
    if (!g_sdCard) return;
    CardInfo->LogBlockNbr  = g_sdCard->getBlockCount();
    CardInfo->LogBlockSize = SD_DEFAULT_BLOCK_SIZE;
    CardInfo->CardType     = (uint8_t)g_sdCard->getCardType();
}

uint8_t BSP_SD_IsDetected(void)
{
    return (BSP_PlatformIsDetected() == 1U) ? SD_PRESENT : SD_NOT_PRESENT;
}

} /* extern "C" */

#endif /* SD_SPI */
