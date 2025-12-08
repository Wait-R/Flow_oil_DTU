/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "eeprom_drive.h"
#include "stdio.h"
#include "led.h"
#include "iwdg.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
osThreadId defaultTaskHandle;
osThreadId UploadDataTaskHandle;
osThreadId ProcesDataTaskHandle;
osThreadId StorageDataTaskHandle;
osThreadId oledTaskHandle;
osMessageQId SendQueueHandle;
osMessageQId UploadQueueHandle;
osMessageQId RequestQueueHandle;
osMessageQId DeleteQueueHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void UploadData_Task(void const * argument);
void ProcesData_Task(void const * argument);
void StorageData_Task(void const * argument);
void oled_Task(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of SendQueue */
  osMessageQDef(SendQueue, 4, float);
  SendQueueHandle = osMessageCreate(osMessageQ(SendQueue), NULL);

  /* definition and creation of UploadQueue */
  osMessageQDef(UploadQueue, 1, OilDataWithTime);
  UploadQueueHandle = osMessageCreate(osMessageQ(UploadQueue), NULL);

  /* definition and creation of RequestQueue */
  osMessageQDef(RequestQueue, 1, uint8_t);
  RequestQueueHandle = osMessageCreate(osMessageQ(RequestQueue), NULL);

  /* definition and creation of DeleteQueue */
  osMessageQDef(DeleteQueue, 1, uint8_t);
  DeleteQueueHandle = osMessageCreate(osMessageQ(DeleteQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityIdle, 0, 64);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* definition and creation of UploadDataTask */
  osThreadDef(UploadDataTask, UploadData_Task, osPriorityAboveNormal, 0, 256);
  UploadDataTaskHandle = osThreadCreate(osThread(UploadDataTask), NULL);

  /* definition and creation of ProcesDataTask */
  osThreadDef(ProcesDataTask, ProcesData_Task, osPriorityNormal, 0, 256);
  ProcesDataTaskHandle = osThreadCreate(osThread(ProcesDataTask), NULL);

  /* definition and creation of StorageDataTask */
  osThreadDef(StorageDataTask, StorageData_Task, osPriorityHigh, 0, 256);
  StorageDataTaskHandle = osThreadCreate(osThread(StorageDataTask), NULL);

  /* definition and creation of oledTask */
  osThreadDef(oledTask, oled_Task, osPriorityBelowNormal, 0, 512);
  oledTaskHandle = osThreadCreate(osThread(oledTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  HAL_IWDG_Init(&hiwdg);
  /* Infinite loop */
  for(;;)
  {
    vTaskDelay(4000);
    HAL_IWDG_Refresh(&hiwdg); // ι��
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_UploadData_Task */
/**
* @brief Function implementing the UploadDataTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_UploadData_Task */
__weak void UploadData_Task(void const * argument)
{
  /* USER CODE BEGIN UploadData_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END UploadData_Task */
}

/* USER CODE BEGIN Header_ProcesData_Task */
/**
* @brief Function implementing the ProcesDataTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_ProcesData_Task */
__weak void ProcesData_Task(void const * argument)
{
  /* USER CODE BEGIN ProcesData_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END ProcesData_Task */
}

/* USER CODE BEGIN Header_StorageData_Task */
/**
* @brief Function implementing the StorageDataTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StorageData_Task */
__weak void StorageData_Task(void const * argument)
{
  /* USER CODE BEGIN StorageData_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StorageData_Task */
}

/* USER CODE BEGIN Header_oled_Task */
/**
* @brief Function implementing the oledTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_oled_Task */
__weak void oled_Task(void const * argument)
{
  /* USER CODE BEGIN oled_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END oled_Task */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

