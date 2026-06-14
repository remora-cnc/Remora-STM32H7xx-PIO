/**
 * STM32H7_SPI_SDcard.cpp
 *
 * Fully integrated SPI SD card HAL class for STM32H7.
 * Owns all layers: GPIO, SPI peripheral, SD card protocol, FatFs mount.
 * sd_spi.c and sd_spi.h are no longer needed and can be removed.
 *
 * bsp_driver_sd_spi.c calls this class via g_sdCard global pointer:
 *   g_sdCard->cardInit()
 *   g_sdCard->readBlock()
 *   g_sdCard->writeBlock()
 *   g_sdCard->getBlockCount()
 *   g_sdCard->getCardType()
 */

#ifdef SD_SPI

#include "STM32H7_SPI_SDcard.h"
#include "PinNamesTypes.h"   /* STM_PIN_AFNUM — .cpp only, no include guard  */

#include <cstdio>

/* ── Constants ───────────────────────────────────────────────────────────── */
#define SPI_TIMEOUT_MS   100U
#define ACMD41_RETRIES   200U       /* 200 × 10 ms = 2 s max                 */
#define SD_SPI_BAUD_MASK (0x38UL)   /* CFG1[5:3] baud rate field on H7       */

/* ── Global instance pointer consumed by bsp_driver_sd_spi.c ────────────── */
STM32H7_SPI_SDcard *g_sdCard = nullptr;

/* ── Constructor ─────────────────────────────────────────────────────────── */
STM32H7_SPI_SDcard::STM32H7_SPI_SDcard(std::string mosi,
                                        std::string miso,
                                        std::string clk,
                                        std::string cs)
    : _mosiPortAndPin(mosi)
    , _misoPortAndPin(miso)
    , _clkPortAndPin(clk)
    , _csPortAndPin(cs)
    , _mosiPin(nullptr)
    , _misoPin(nullptr)
    , _clkPin(nullptr)
    , _csPin(nullptr)
    , _cardType(SD_CARD_UNKNOWN)
    , _blockCount(0)
    , _mounted(false)
{
    _mosiPinName = portAndPinToPinName(_mosiPortAndPin.c_str());
    _misoPinName = portAndPinToPinName(_misoPortAndPin.c_str());
    _clkPinName  = portAndPinToPinName(_clkPortAndPin.c_str());
    _csPinName   = portAndPinToPinName(_csPortAndPin.c_str());

    _spiHandle.Instance = (SPI_TypeDef*)getSPIPeripheralName(_mosiPinName,
                                                              _misoPinName,
                                                              _clkPinName);
    /* Register this instance for the C BSP layer                            */
    g_sdCard = this;
}

STM32H7_SPI_SDcard::~STM32H7_SPI_SDcard()
{
    delete _mosiPin;
    delete _misoPin;
    delete _clkPin;
    delete _csPin;
    if (g_sdCard == this) g_sdCard = nullptr;
}

/* ── init() ──────────────────────────────────────────────────────────────── */
bool STM32H7_SPI_SDcard::init()
{
    printf("STM32H7_SPI_SDcard: configuring GPIO\r\n");

    /* CS — plain output, deasserted (HIGH) before anything else             */
    _csPin = new Pin(_csPortAndPin, GPIO_MODE_OUTPUT_PP,
                     GPIO_NOPULL, GPIO_SPEED_FREQ_LOW, 0);
    _csPin->set(1);

    /* MOSI / MISO / CLK — SPI alternate function                            */
    _mosiPin = createPin(_mosiPortAndPin, _mosiPinName, PinMap_SPI_MOSI);
    _misoPin = createPin(_misoPortAndPin, _misoPinName, PinMap_SPI_MISO);
    _clkPin  = createPin(_clkPortAndPin,  _clkPinName,  PinMap_SPI_SCLK);

    printf("STM32H7_SPI_SDcard: initialising SPI\r\n");

    enableSPIClock(_spiHandle.Instance);

    _spiHandle.Init.Mode              = SPI_MODE_MASTER;
    _spiHandle.Init.Direction         = SPI_DIRECTION_2LINES;
    _spiHandle.Init.DataSize          = SPI_DATASIZE_8BIT;
    _spiHandle.Init.CLKPolarity       = SPI_POLARITY_LOW;          /* CPOL=0 */
    _spiHandle.Init.CLKPhase          = SPI_PHASE_1EDGE;           /* CPHA=0 */
    _spiHandle.Init.NSS               = SPI_NSS_SOFT;
    _spiHandle.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256; /* ~468kHz*/
    _spiHandle.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    /* H7-specific — do NOT omit                                             */
    _spiHandle.Init.TIMode                  = SPI_TIMODE_DISABLE;
    _spiHandle.Init.CRCCalculation          = SPI_CRCCALCULATION_DISABLE;
    _spiHandle.Init.CRCPolynomial           = 0x0007UL;
    _spiHandle.Init.CRCLength               = SPI_CRC_LENGTH_DATASIZE;
    _spiHandle.Init.NSSPMode                = SPI_NSS_PULSE_DISABLE;
    _spiHandle.Init.NSSPolarity             = SPI_NSS_POLARITY_LOW;
    _spiHandle.Init.FifoThreshold           = SPI_FIFO_THRESHOLD_01DATA;
    _spiHandle.Init.MasterSSIdleness        = SPI_MASTER_SS_IDLENESS_00CYCLE;
    _spiHandle.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
    _spiHandle.Init.MasterReceiverAutoSusp  = SPI_MASTER_RX_AUTOSUSP_DISABLE;
    _spiHandle.Init.MasterKeepIOState       = SPI_MASTER_KEEP_IO_STATE_ENABLE;
    _spiHandle.Init.IOSwap                  = SPI_IO_SWAP_DISABLE;

    if (HAL_SPI_Init(&_spiHandle) != HAL_OK)
    {
        printf("STM32H7_SPI_SDcard: HAL_SPI_Init FAILED\r\n");
        return false;
    }

    printf("STM32H7_SPI_SDcard: mounting FatFs\r\n");

    MX_FATFS_Init();

    printf("STM32H7_SPI_SDcard: SD card ready\r\n");
    return true;
}

/* ── Public SD protocol interface ────────────────────────────────────────── */

sd_result_t STM32H7_SPI_SDcard::cardInit(sd_card_type_t *cardType)
{
    uint8_t r1;
    _cardType   = SD_CARD_UNKNOWN;
    _blockCount = 0;

    fclkSlow();
    csDeassert();

    /* 80 init clocks with CS deasserted — SD spec requirement               */
    for (uint8_t i = 0; i < 10; i++) spiXchg(0xFF);

    /* CMD0 — GO_IDLE_STATE — CRC 0x95 mandatory                             */
    csAssert();
    r1 = sdCmd(0, 0x00000000UL, 0x95U);
    csDeassert();
    spiXchg(0xFF);

    if (r1 == 0xFF) return SD_ERR_NO_RESP;
    if ((r1 & ~0x09U) != 0x00U) return SD_ERR_CMD0;

    /* CMD8 — SEND_IF_COND — CRC 0x87 mandatory                             */
    csAssert();
    r1 = sdCmd(8, 0x000001AAUL, 0x87U);
    if (r1 == 0x01U) {
        uint8_t r7[4];
        for (uint8_t i = 0; i < 4; i++) r7[i] = spiXchg(0xFF);
        csDeassert();
        spiXchg(0xFF);
        _cardType = (r7[3] == 0xAAU && (r7[2] & 0x0FU) == 0x01U) ?
                    SD_CARD_SDV2_SC : SD_CARD_SDV1;
    } else {
        csDeassert();
        spiXchg(0xFF);
        _cardType = SD_CARD_SDV1;
    }

    /* ACMD41 — SD_SEND_OP_COND                                              */
    {
        uint32_t hcs   = (_cardType != SD_CARD_SDV1) ? 0x40000000UL : 0UL;
        uint8_t  ready = 0;

        for (uint32_t n = 0; n < ACMD41_RETRIES; n++) {
            csAssert();
            sdCmd(55, 0UL, 0x01U);
            csDeassert();
            spiXchg(0xFF);

            csAssert();
            r1 = sdCmd(41, hcs, 0x01U);
            csDeassert();
            spiXchg(0xFF);

            if (r1 == 0x00U) { ready = 1; break; }
            if (r1 == 0x01U) { HAL_Delay(10); continue; }

            /* SDv1 fallback to CMD1 (MMC)                                   */
            if (_cardType == SD_CARD_SDV1) {
                for (uint32_t m = 0; m < ACMD41_RETRIES; m++) {
                    csAssert();
                    r1 = sdCmd(1, 0UL, 0x01U);
                    csDeassert();
                    spiXchg(0xFF);
                    if (r1 == 0x00U) { ready = 1; _cardType = SD_CARD_MMC; break; }
                    HAL_Delay(10);
                }
                break;
            }
            break;
        }
        if (!ready) return SD_ERR_ACMD41;
    }

    /* CMD58 — READ_OCR — refine SDv2 SC vs HC                              */
    if (_cardType == SD_CARD_SDV2_SC) {
        csAssert();
        r1 = sdCmd(58, 0UL, 0x01U);
        if (r1 == 0x00U) {
            uint8_t ocr[4];
            for (uint8_t i = 0; i < 4; i++) ocr[i] = spiXchg(0xFF);
            csDeassert();
            spiXchg(0xFF);
            uint32_t ocr_val = ((uint32_t)ocr[0] << 24) |
                               ((uint32_t)ocr[1] << 16) |
                               ((uint32_t)ocr[2] <<  8) |
                               ((uint32_t)ocr[3]);
            _cardType = (ocr_val & 0x40000000UL) ? SD_CARD_SDV2_HC : SD_CARD_SDV2_SC;
        } else {
            csDeassert();
            spiXchg(0xFF);
        }
    }

    /* CMD16 — SET_BLOCKLEN 512 — SDv1 / MMC only                           */
    if (_cardType == SD_CARD_SDV1 || _cardType == SD_CARD_MMC) {
        csAssert();
        r1 = sdCmd(16, 512UL, 0x01U);
        csDeassert();
        spiXchg(0xFF);
        if (r1 != 0x00U) return SD_ERR_CMD16;
    }

    /* CMD9 — SEND_CSD — decode block count                                  */
    csAssert();
    r1 = sdCmd(9, 0UL, 0x01U);
    if (r1 == 0x00U) {
        uint32_t t0  = HAL_GetTick();
        uint8_t  tok = 0xFF;
        while (tok != 0xFEU && (HAL_GetTick() - t0) < 200U)
            tok = spiXchg(0xFF);

        if (tok == 0xFEU) {
            uint8_t csd[16];
            for (uint8_t i = 0; i < 16; i++) csd[i] = spiXchg(0xFF);
            spiXchg(0xFF); spiXchg(0xFF);   /* CRC discard                  */

            uint8_t csd_ver = (csd[0] >> 6) & 0x03U;
            if (csd_ver == 1U) {
                uint32_t c_size = ((uint32_t)(csd[7] & 0x3FU) << 16) |
                                  ((uint32_t) csd[8]           <<  8) |
                                  ((uint32_t) csd[9]);
                _blockCount = (c_size + 1UL) * 1024UL;
            } else {
                uint32_t c_size = ((uint32_t)(csd[6]  & 0x03U) << 10) |
                                  ((uint32_t) csd[7]            <<  2) |
                                  ((uint32_t)(csd[8]  >> 6)          );
                uint32_t c_mult = ((uint32_t)(csd[9]  & 0x03U) <<  1) |
                                  ((uint32_t)(csd[10] >> 7)          );
                uint32_t bl_len =  (uint32_t)(csd[5]  & 0x0FU);
                _blockCount = (c_size + 1UL) *
                              (1UL << (c_mult + 2UL)) *
                              (1UL << bl_len) / 512UL;
            }
        }
    }
    csDeassert();
    spiXchg(0xFF);

    fclkFast();

    if (cardType) *cardType = _cardType;
    return SD_OK;
}

sd_result_t STM32H7_SPI_SDcard::readBlock(uint8_t *buf, uint32_t addr)
{
    csAssert();
    uint8_t r1 = sdCmd(17, addr, 0x01U);
    if (r1 != 0x00U) { csDeassert(); return SD_ERR_NO_RESP; }

    uint32_t t0  = HAL_GetTick();
    uint8_t  tok = 0xFF;
    while (tok != 0xFEU && (HAL_GetTick() - t0) < 200U)
        tok = spiXchg(0xFF);

    if (tok != 0xFEU) { csDeassert(); return SD_ERR_NO_RESP; }

    for (uint16_t i = 0; i < 512; i++) buf[i] = spiXchg(0xFF);
    spiXchg(0xFF); spiXchg(0xFF);   /* CRC discard                          */

    csDeassert();
    spiXchg(0xFF);
    return SD_OK;
}

sd_result_t STM32H7_SPI_SDcard::writeBlock(const uint8_t *buf, uint32_t addr)
{
    csAssert();
    uint8_t r1 = sdCmd(24, addr, 0x01U);
    if (r1 != 0x00U) { csDeassert(); return SD_ERR_NO_RESP; }

    spiXchg(0xFF);
    spiXchg(0xFEU);   /* data token                                          */

    for (uint16_t i = 0; i < 512; i++) spiXchg(buf[i]);
    spiXchg(0xFFU); spiXchg(0xFFU);   /* dummy CRC                          */

    if ((spiXchg(0xFF) & 0x1FU) != 0x05U) { csDeassert(); return SD_ERR_NO_RESP; }

    uint32_t t0 = HAL_GetTick();
    while (spiXchg(0xFF) == 0x00U) {
        if ((HAL_GetTick() - t0) >= 500U) { csDeassert(); return SD_ERR_NO_RESP; }
    }

    csDeassert();
    spiXchg(0xFF);
    return SD_OK;
}

/* ── Private: baud-rate switching ────────────────────────────────────────── */

void STM32H7_SPI_SDcard::fclkSlow()
{
    CLEAR_BIT(_spiHandle.Instance->CR1, SPI_CR1_SPE);
    MODIFY_REG(_spiHandle.Instance->CFG1,
               SD_SPI_BAUD_MASK, SPI_BAUDRATEPRESCALER_256);
    SET_BIT(_spiHandle.Instance->CR1, SPI_CR1_SPE);
}

void STM32H7_SPI_SDcard::fclkFast()
{
    CLEAR_BIT(_spiHandle.Instance->CR1, SPI_CR1_SPE);
    MODIFY_REG(_spiHandle.Instance->CFG1,
               SD_SPI_BAUD_MASK, SPI_BAUDRATEPRESCALER_8);
    SET_BIT(_spiHandle.Instance->CR1, SPI_CR1_SPE);
}

/* ── Private: CS and SPI byte exchange ──────────────────────────────────── */

void STM32H7_SPI_SDcard::csAssert()
{
    _csPin->set(0);
}

void STM32H7_SPI_SDcard::csDeassert()
{
    _csPin->set(1);
}

uint8_t STM32H7_SPI_SDcard::spiXchg(uint8_t tx)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(&_spiHandle, &tx, &rx, 1, SPI_TIMEOUT_MS);
    return rx;
}

/* ── Private: SD command ─────────────────────────────────────────────────── */

uint8_t STM32H7_SPI_SDcard::sdCmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t pkt[6];
    pkt[0] = 0x40U | cmd;
    pkt[1] = (uint8_t)(arg >> 24);
    pkt[2] = (uint8_t)(arg >> 16);
    pkt[3] = (uint8_t)(arg >>  8);
    pkt[4] = (uint8_t)(arg      );
    pkt[5] = crc;

    spiXchg(0xFF);   /* Ncr guard                                            */
    for (uint8_t i = 0; i < 6; i++) spiXchg(pkt[i]);

    uint8_t r1 = 0xFF;
    for (uint8_t i = 0; i < 10; i++) {
        r1 = spiXchg(0xFF);
        if (!(r1 & 0x80)) break;
    }
    return r1;
}

/* ── Private: GPIO / SPI setup ──────────────────────────────────────────── */

SPIName STM32H7_SPI_SDcard::getSPIPeripheralName(PinName mosi,
                                                   PinName miso,
                                                   PinName clk)
{
    SPIName spi_mosi = (SPIName)pinmap_peripheral(mosi, PinMap_SPI_MOSI);
    SPIName spi_miso = (SPIName)pinmap_peripheral(miso, PinMap_SPI_MISO);
    SPIName spi_clk  = (SPIName)pinmap_peripheral(clk,  PinMap_SPI_SCLK);
    SPIName spi_data = (SPIName)pinmap_merge(spi_mosi, spi_miso);
    return  (SPIName)pinmap_merge(spi_data, spi_clk);
}

Pin* STM32H7_SPI_SDcard::createPin(const std::string& portAndPin,
                                    PinName pinName,
                                    const PinMap* map)
{
    uint32_t function = STM_PIN_AFNUM(pinmap_function(pinName, map));
    return new Pin(portAndPin, GPIO_MODE_AF_PP,
                   GPIO_PULLUP, GPIO_SPEED_FREQ_HIGH, function);
}

void STM32H7_SPI_SDcard::enableSPIClock(SPI_TypeDef* instance)
{
    if      (instance == SPI1) __HAL_RCC_SPI1_CLK_ENABLE();
    else if (instance == SPI2) __HAL_RCC_SPI2_CLK_ENABLE();
    else if (instance == SPI3) __HAL_RCC_SPI3_CLK_ENABLE();
    else if (instance == SPI4) __HAL_RCC_SPI4_CLK_ENABLE();
    else if (instance == SPI5) __HAL_RCC_SPI5_CLK_ENABLE();
#if defined(SPI6)
    else if (instance == SPI6) __HAL_RCC_SPI6_CLK_ENABLE();
#endif
}

#endif /* SD_SPI */
