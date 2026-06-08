/**
 * STM32H7_SPI_SDcard.h
 *
 * HAL class for SPI-connected SD card on STM32H7.
 * Fully self-contained — owns GPIO init, SPI peripheral, SD card protocol,
 * and FatFs mount. No separate sd_spi.c/h required.
 *
 * The SD protocol (CMD0→CMD8→ACMD41→CMD58→CMD16→CMD9, CMD17, CMD24) is
 * implemented as private methods. bsp_driver_sd_spi.c calls the public
 * cardInit / readBlock / writeBlock / getBlockCount interface.
 *
 * Selected by -D SD_SPI in platformio.ini.
 *
 * NOTE: PinNamesTypes.h has no include guard — include it in .cpp only.
 */

#pragma once

#ifdef SD_SPI

#include "stm32h7xx_hal.h"
#include "peripheralPins.h"
#include "pin/pin.h"
#include "fatfs.h"

#include <string>
#include <cstdint>

/* ── Card type (was sd_card_type_t in sd_spi.h) ────────────────────────── */
typedef enum {
    SD_CARD_UNKNOWN  = 0x00,
    SD_CARD_SDV1     = 0x01,
    SD_CARD_SDV2_SC  = 0x02,
    SD_CARD_SDV2_HC  = 0x04,
    SD_CARD_MMC      = 0x08,
} sd_card_type_t;

/* ── Result codes (was sd_result_t in sd_spi.h) ────────────────────────── */
typedef enum {
    SD_OK            =  0,
    SD_ERR_NO_RESP   = -1,
    SD_ERR_CMD0      = -2,
    SD_ERR_ACMD41    = -3,
    SD_ERR_CMD16     = -4,
} sd_result_t;

class STM32H7_SPI_SDcard
{
public:
    /**
     * @param mosi  e.g. "PA_7"
     * @param miso  e.g. "PA_6"
     * @param clk   e.g. "PA_5"
     * @param cs    e.g. "PA_4"  — GPIO output, active low
     */
    STM32H7_SPI_SDcard(std::string mosi,
                       std::string miso,
                       std::string clk,
                       std::string cs);

    ~STM32H7_SPI_SDcard();

    /**
     * init()
     * Configures GPIO, SPI peripheral, runs SD card init sequence, mounts FatFs.
     * Call once from main() after SystemClock_Config() / PeriphCommonClock_Config().
     * @return true on successful FatFs mount.
     */
    bool init();

    bool isMounted() const { return _mounted; }

    /* ── Interface for bsp_driver_sd_spi.c ─────────────────────────────── */
    sd_result_t cardInit(sd_card_type_t *cardType);
    sd_result_t readBlock(uint8_t *buf, uint32_t addr);
    sd_result_t writeBlock(const uint8_t *buf, uint32_t addr);
    uint32_t    getBlockCount() const { return _blockCount; }
    sd_card_type_t getCardType() const { return _cardType; }

private:
    /* ── Pin configuration ──────────────────────────────────────────────── */
    std::string _mosiPortAndPin;
    std::string _misoPortAndPin;
    std::string _clkPortAndPin;
    std::string _csPortAndPin;

    PinName _mosiPinName;
    PinName _misoPinName;
    PinName _clkPinName;
    PinName _csPinName;

    Pin *_mosiPin;
    Pin *_misoPin;
    Pin *_clkPin;
    Pin *_csPin;

    /* ── SPI peripheral ─────────────────────────────────────────────────── */
    SPI_HandleTypeDef _spiHandle;

    /* ── Card state ─────────────────────────────────────────────────────── */
    sd_card_type_t _cardType;
    uint32_t       _blockCount;
    bool           _mounted;

    /* ── GPIO / SPI setup helpers ───────────────────────────────────────── */
    SPIName getSPIPeripheralName(PinName mosi, PinName miso, PinName clk);
    Pin*    createPin(const std::string& portAndPin, PinName pinName,
                      const PinMap* map);
    void    enableSPIClock(SPI_TypeDef* instance);

    /* ── STM32H7 baud-rate switching ────────────────────────────────────── */
    void fclkSlow();
    void fclkFast();

    /* ── Low-level SPI ──────────────────────────────────────────────────── */
    void    csAssert();
    void    csDeassert();
    uint8_t spiXchg(uint8_t tx);

    /* ── SD command ─────────────────────────────────────────────────────── */
    uint8_t sdCmd(uint8_t cmd, uint32_t arg, uint8_t crc);
};

/* ── Global instance pointer — set by the class, used by bsp_driver_sd_spi.c */
extern STM32H7_SPI_SDcard *g_sdCard;

#endif /* SD_SPI */
