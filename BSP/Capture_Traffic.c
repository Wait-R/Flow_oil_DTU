#include "Capture_Traffic.h"
#include "flowConfig.h"

__IO uint32_t CAPTURE_COUNT = 0;   // 流量数据保存处
double coefficient = 0.013723032826; // 流量系数

/**
 * @brief 清除流量数据
 * @param  NULL
 * @return NULL
 */
void Clear_Traffic(void)
{
	CAPTURE_COUNT = 0;
}

/**
 * @brief 获取流量数据
 * @param NULL
 * @return 流量值（L）
 */
double Get_Traffic(void)
{
	return (double)CAPTURE_COUNT * (double)coefficient;
}

/**
 * @brief       外部中断回调函数
 * @param       GPIO_Pin: GPIO口
 * @retval      无
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) // 每次捕获到下降沿就计数
{
	if(GPIO_Pin == GPIO_PIN_0){
		CAPTURE_COUNT ++;  
	}
	else if(GPIO_Pin == GPIO_PIN_1) {
		CAPTURE_COUNT ++;
	}
}
