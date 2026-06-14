#ifndef PERIPHERALPINS_H
#define PERIPHERALPINS_H

#include "stm32h7xx_hal.h"
#include "pinNames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ADC_1 = (int)ADC1_BASE,
    ADC_2 = (int)ADC2_BASE,
#if ADC3_BASE
    ADC_3 = (int)ADC3_BASE
#endif
} ADCName;

typedef enum {
    SPI_1 = (int)SPI1_BASE,
    SPI_2 = (int)SPI2_BASE,
    SPI_3 = (int)SPI3_BASE,
    SPI_4 = (int)SPI4_BASE,
    SPI_5 = (int)SPI5_BASE,
    SPI_6 = (int)SPI6_BASE
} SPIName;

/* ─────────────────────────────────────────────
 * NEW: UART peripheral enum
 * ───────────────────────────────────────────── */
typedef enum {
    UART_1 = (int)USART1_BASE,
    UART_2 = (int)USART2_BASE,
    UART_3 = (int)USART3_BASE,
#if defined(UART4_BASE)
    UART_4 = (int)UART4_BASE,
#endif
#if defined(UART5_BASE)
    UART_5 = (int)UART5_BASE,
#endif
} UARTName;

/* ─────────────────────────────────────────────
 * NEW: SDMMC peripheral enum
 * ───────────────────────────────────────────── */
typedef enum {
    SDMMC_1 = (int)SDMMC1_BASE,
#if defined(SDMMC2_BASE)
    SDMMC_2 = (int)SDMMC2_BASE,
#endif
} SDMMCName;


/* ─────────────────────────────────────────────
 * Existing pin maps
 * ───────────────────────────────────────────── */
extern const PinMap PinMap_ADC[];

extern const PinMap PinMap_SPI_MOSI[];
extern const PinMap PinMap_SPI_MISO[];
extern const PinMap PinMap_SPI_SCLK[];
extern const PinMap PinMap_SPI_SSEL[];


/* ─────────────────────────────────────────────
 * NEW: UART pin maps
 * ───────────────────────────────────────────── */
extern const PinMap PinMap_UART_TX[];
extern const PinMap PinMap_UART_RX[];


/* ─────────────────────────────────────────────
 * NEW: SDMMC pin maps
 * ───────────────────────────────────────────── */
extern const PinMap PinMap_SDMMC_CMD[];
extern const PinMap PinMap_SDMMC_CK[];
extern const PinMap PinMap_SDMMC_D0[];
extern const PinMap PinMap_SDMMC_D1[];
extern const PinMap PinMap_SDMMC_D2[];
extern const PinMap PinMap_SDMMC_D3[];


/* ─────────────────────────────────────────────
 * Pinmap helpers (unchanged)
 * ───────────────────────────────────────────── */
uint32_t pinmap_peripheral(PinName pin, const PinMap *map);
uint32_t pinmap_function(PinName pin, const PinMap *map);
uint32_t pinmap_find_peripheral(PinName pin, const PinMap *map);
uint32_t pinmap_find_function(PinName pin, const PinMap *map);
uint32_t pinmap_merge(uint32_t a, uint32_t b);
PinName portAndPinToPinName(const char* portAndPin);

#ifdef __cplusplus
}
#endif

#endif // PERIPHERALPINS_H
