#include "led.h"
#include "gpio.h"

void LED_OFF(void){
	HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin, (GPIO_PinState)1);
	
}

void LED_ON(void){
	HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin, (GPIO_PinState)0);
	
}

void LED_Toggle(void){
	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
}
