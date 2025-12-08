#include "delay.h"
#include "Task.h"


void DelayUs(uint32_t nus)
{
	uint32_t ticks;
	uint32_t told,tnow,tcnt=0;
	uint32_t reload=SysTick->LOAD;
	ticks=nus*72; 
	told=SysTick->VAL; 
	while(1)
	{
		tnow=SysTick->VAL;
		if(tnow!=told)
		{
			if(tnow<told)tcnt+=told-tnow;
			else tcnt+=reload-tnow+told;
			told=tnow;
			if(tcnt>=ticks)break; 
		}
	}
}

void delay_us(uint32_t nus)
{
	DelayUs(nus);
}


void delay_ms(uint32_t nms)
{
	vTaskDelay(nms);
	// HAL_Delay(nms);
}


