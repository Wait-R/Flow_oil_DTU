#include "Capture_Traffic.h"
#include "flowConfig.h"
#include "tim.h"

__IO uint64_t CAPTURE_COUNT = 0;   // 流量数据保存处
double coefficient = 0.013723032826; // 流量系数（服务器端调整，无需修改）

int16_t Encoder_Get(void);
void Encoder_Init(void);

void Traffic_Init(void){
	Encoder_Init();
	Clear_Traffic();
}

/**
 * @brief 清除流量数据
 * @param  NULL
 * @return NULL
 */
void Clear_Traffic(void)
{
	CAPTURE_COUNT = 0;
	__HAL_TIM_SetCounter(&htim2, 0);//清空计数值	
}

/**
 * @brief 获取流量数据
 * @param NULL
 * @return 流量值（L）
 */
double Get_Traffic(void)
{
	CAPTURE_COUNT += Encoder_Get();
	return (double)CAPTURE_COUNT;
}

void Encoder_Init(void)
{
	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL); //开启编码器模式
	HAL_TIM_Base_Start_IT(&htim2);                  //开启编码器的中断               //开启编码器的中断
}

/**
  * 函    数：获取编码器计数值
  * 参    数：无
  * 返 回 值：自上此调用此函数后，编码器的增量值
  */
int16_t Encoder_Get(void)
{
	int16_t temp;

	temp = __HAL_TIM_GetCounter(&htim2);//获取计数值
	__HAL_TIM_SetCounter(&htim2, 0);//清空计数值	

	return temp;
}
