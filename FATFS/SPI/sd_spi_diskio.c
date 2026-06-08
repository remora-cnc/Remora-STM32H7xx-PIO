/* FATFS/Target/sd_spi_diskio.c
 *
 * FatFs disk I/O driver — SPI polling path.
 * Selected by -D SD_SPI in platformio.ini.
 *
 * Deliberately simple compared to sd_diskio.c:
 *   - No DMA, no callbacks, no ReadStatus / WriteStatus flags
 *   - No D-cache maintenance (SPI polling transfers do not use DMA)
 *   - No scratch buffer (SPI transfers are byte-wide; alignment irrelevant)
 *
 * All block I/O is delegated to bsp_driver_sd_spi.c which owns the
 * raw SPI SD protocol.  This file only adapts the FatFs diskio interface
 * to those BSP calls.
 */

#ifdef SD_SPI   /* entire file excluded when building for SDIO boards */

#include "ff_gen_drv.h"
#include "sd_spi_diskio.h"
#include "bsp_driver_sd_spi.h"

/* ── Private state ───────────────────────────────────────────────────────── */
static volatile DSTATUS Stat = STA_NOINIT;

/* ── Forward declarations ────────────────────────────────────────────────── */
static DSTATUS SD_SPI_initialize(BYTE lun);
static DSTATUS SD_SPI_status    (BYTE lun);
static DRESULT SD_SPI_read      (BYTE lun, BYTE *buff, DWORD sector, UINT count);
#if _USE_WRITE == 1
static DRESULT SD_SPI_write     (BYTE lun, const BYTE *buff, DWORD sector, UINT count);
#endif
#if _USE_IOCTL == 1
static DRESULT SD_SPI_ioctl     (BYTE lun, BYTE cmd, void *buff);
#endif

/* ── Driver table registered with FatFs ─────────────────────────────────── */
const Diskio_drvTypeDef SD_Driver =
{
    SD_SPI_initialize,
    SD_SPI_status,
    SD_SPI_read,
#if _USE_WRITE == 1
    SD_SPI_write,
#endif
#if _USE_IOCTL == 1
    SD_SPI_ioctl,
#endif
};

/* ── diskio interface implementations ───────────────────────────────────── */

/**
 * SD_SPI_initialize
 * Called by FatFs on f_mount().  Runs BSP_SD_Init() which executes the
 * full CMD0→CMD8→ACMD41→CMD58 sequence.  Safe to call repeatedly.
 */
static DSTATUS SD_SPI_initialize(BYTE lun)
{
    (void)lun;

    if (BSP_SD_IsDetected() != SD_PRESENT) {
        Stat = STA_NOINIT | STA_NODISK;
        return Stat;
    }

    if (BSP_SD_Init() == MSD_OK) {
        Stat = 0x00;   /* clear STA_NOINIT */
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
}

/**
 * SD_SPI_status
 * Returns current disk status.  Checks the card is still responding
 * via CMD13 (wrapped in BSP_SD_GetCardState).
 */
static DSTATUS SD_SPI_status(BYTE lun)
{
    (void)lun;

    if (BSP_SD_IsDetected() != SD_PRESENT) {
        Stat = STA_NOINIT | STA_NODISK;
        return Stat;
    }

    if (BSP_SD_GetCardState() == SD_TRANSFER_OK) {
        Stat &= ~STA_NOINIT;
    } else {
        Stat = STA_NOINIT;
    }

    return Stat;
}

/**
 * SD_SPI_read
 * Reads 'count' sectors from 'sector' into 'buff'.
 * Sector size is fixed at 512 bytes (ffconf.h: _MIN_SS = _MAX_SS = 512).
 */
static DRESULT SD_SPI_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
    (void)lun;

    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (!count)             return RES_PARERR;

    if (BSP_SD_ReadBlocks((uint32_t *)buff, (uint32_t)sector,
                          (uint32_t)count, 2000U) == MSD_OK) {
        return RES_OK;
    }
    return RES_ERROR;
}

#if _USE_WRITE == 1
/**
 * SD_SPI_write
 * Writes 'count' sectors from 'buff' to 'sector'.
 */
static DRESULT SD_SPI_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
    (void)lun;

    if (Stat & STA_NOINIT) return RES_NOTRDY;
    if (!count)             return RES_PARERR;

    if (BSP_SD_WriteBlocks((uint32_t *)buff, (uint32_t)sector,
                           (uint32_t)count, 2000U) == MSD_OK) {
        return RES_OK;
    }
    return RES_ERROR;
}
#endif /* _USE_WRITE */

#if _USE_IOCTL == 1
/**
 * SD_SPI_ioctl
 * Handles the FatFs control commands.  Card geometry comes from BSP_SD_GetCardInfo()
 * which decodes the CSD register captured during BSP_SD_Init().
 */
static DRESULT SD_SPI_ioctl(BYTE lun, BYTE cmd, void *buff)
{
    (void)lun;

    if (Stat & STA_NOINIT) return RES_NOTRDY;

    BSP_SD_SPI_CardInfo info;

    switch (cmd)
    {
    case CTRL_SYNC:
        /* No write cache in polling mode — always in sync                  */
        return RES_OK;

    case GET_SECTOR_COUNT:
        BSP_SD_GetCardInfo(&info);
        *(DWORD *)buff = (DWORD)info.LogBlockNbr;
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = (WORD)SD_DEFAULT_BLOCK_SIZE;
        return RES_OK;

    case GET_BLOCK_SIZE:
        /* Erase block size — report 1 sector (unknown without SCR decode)  */
        *(DWORD *)buff = 1UL;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}
#endif /* _USE_IOCTL */

#endif /* SD_SPI */
