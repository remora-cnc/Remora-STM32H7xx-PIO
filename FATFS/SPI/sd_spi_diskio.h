/* FATFS/Target/sd_spi_diskio.h
 *
 * FatFs disk I/O driver for SPI-connected SD cards.
 * Selected by -D SD_SPI in platformio.ini.
 * Parallel to sd_diskio.h (SDIO/SDMMC path) — never include both.
 */

#ifndef SD_SPI_DISKIO_H
#define SD_SPI_DISKIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ff_gen_drv.h"

/* FatFs driver structure — registered with FATFS_LinkDriver() in fatfs.c.
 * The symbol name SD_Driver is the same as the SDIO version so fatfs.c
 * needs no change; the correct object is pulled in at link time based on
 * which diskio .c file is compiled (controlled by -D SD_SPI).              */
extern const Diskio_drvTypeDef SD_Driver;

#ifdef __cplusplus
}
#endif

#endif /* SD_SPI_DISKIO_H */
