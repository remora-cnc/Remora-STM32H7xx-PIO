#ifndef IRQ_HANDLERS_H
#define IRQ_HANDLERS_H

#include "remora-core/interrupt/interrupt.h"
#include "stm32h7xx_hal.h"

extern "C" {

    /* ── SPI chip-select interrupt ───────────────────────────────────────────
     * The CS pin and its EXTI line are board-specific:
     *
     *   SKR3:           PA4  → EXTI4        → EXTI4_IRQHandler
     *   Manta M8P V2.0: PA15 → EXTI15_10   → EXTI15_10_IRQHandler
     *
     * SPI_CS_IRQ is set per-board in platformio.ini:
     *   -D SPI_CS_IRQ=EXTI4_IRQn           (SKR3)
     *   -D SPI_CS_IRQ=EXTI15_10_IRQn       (Manta)
     *
     * The GPIO pin mask is derived from SPI_CS_IRQ to avoid a second
     * board-specific define.
     * ─────────────────────────────────────────────────────────────────────── */

#if SPI_CS_IRQ == EXTI4_IRQn

    void EXTI4_IRQHandler()
    {
        if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_4) != RESET)
        {
            __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
            Interrupt::InvokeHandler(EXTI4_IRQn);
        }
    }

#elif SPI_CS_IRQ == EXTI15_10_IRQn

    void EXTI15_10_IRQHandler()
    {
        /* PA15 is in the EXTI15_10 group — check and clear pin 15 only     */
        if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_15) != RESET)
        {
            __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_15);
            Interrupt::InvokeHandler(EXTI15_10_IRQn);
        }
    }

#else
    #error "SPI_CS_IRQ not defined or not supported. Add a handler for your board."
#endif

    /* ── DMA handlers (comms SPI DMA — same on all boards) ─────────────────
     * STM32H7_SPIComms uses DMA1 Stream0 (TX) and Stream1 (RX).            */

    void DMA1_Stream0_IRQHandler()
    {
        Interrupt::InvokeHandler(DMA1_Stream0_IRQn);
    }

    void DMA1_Stream1_IRQHandler()
    {
        Interrupt::InvokeHandler(DMA1_Stream1_IRQn);
    }

    /* ── Timer handlers ──────────────────────────────────────────────────── */

    void TIM2_IRQHandler()
    {
        if (TIM2->SR & TIM_SR_UIF)
        {
            TIM2->SR &= ~TIM_SR_UIF;
            Interrupt::InvokeHandler(TIM2_IRQn);
        }
    }

    void TIM3_IRQHandler()
    {
        if (TIM3->SR & TIM_SR_UIF)
        {
            TIM3->SR &= ~TIM_SR_UIF;
            Interrupt::InvokeHandler(TIM3_IRQn);
        }
    }

    void TIM4_IRQHandler()
    {
        if (TIM4->SR & TIM_SR_UIF)
        {
            TIM4->SR &= ~TIM_SR_UIF;
            Interrupt::InvokeHandler(TIM4_IRQn);
        }
    }

} // extern "C"

#endif // IRQ_HANDLERS_H