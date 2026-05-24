#include <stdio.h>
#include "STM32H7_timer.h"
#include "../remora-core/thread/timerInterrupt.h"
#include "../remora-core/thread/pruThread.h"

STM32H7_timer::STM32H7_timer(TIM_TypeDef* _timer, IRQn_Type _irq, uint32_t _frequency, pruThread* _ownerPtr, int _irqPriority)
    : timer(_timer), irq(_irq), irqPriority(_irqPriority)
{
    frequency = _frequency;
    timerOwnerPtr = _ownerPtr;
    interruptPtr = std::make_unique<TimerInterrupt>(irq, this);
    timerRunning = false;
}

void STM32H7_timer::configTimer()
{
	uint32_t TIM_CLK;
	
	if (timer == TIM2)
    {
        printf("Power on Timer 2\n\r");
        __HAL_RCC_TIM2_CLK_ENABLE();
        TIM_CLK = APB1CLK;
    }
    else if (timer == TIM3)
    {
        printf("Power on Timer 3\n\r");
        __HAL_RCC_TIM3_CLK_ENABLE();
        TIM_CLK = APB1CLK;
    }
    else if (timer == TIM4)
    {
        printf("Power on Timer 4\n\r");
        __HAL_RCC_TIM4_CLK_ENABLE();
        TIM_CLK = APB1CLK;
    }

    //Note: timer update frequency = TIM_CLK/(TIM_PSC+1)/(TIM_ARR + 1)

    timer->CR2 &= 0;                                            // UG used as trigg output
    timer->PSC = TIM_PSC-1;                                     // prescaler
    timer->ARR = ((TIM_CLK / TIM_PSC / frequency) - 1);   		// period
    timer->EGR = TIM_EGR_UG;                                    // reinit the counter
    timer->DIER = TIM_DIER_UIE;                                 // enable update interrupts
	
    NVIC_SetPriority(irq, irqPriority);
}

void STM32H7_timer::startTimer()
{
    timer->CR1 |= TIM_CR1_CEN;
    NVIC_EnableIRQ(irq);
    timerRunning = true;
    printf("Timer started\n");
}

void STM32H7_timer::stopTimer()
{
    NVIC_DisableIRQ(irq);
    timer->CR1 &= ~TIM_CR1_CEN;
    timerRunning = false;
    printf("Timer stopped\n");
}

void STM32H7_timer::timerTick() {
    if (timerOwnerPtr) {
        timerOwnerPtr->update();
    }
}
