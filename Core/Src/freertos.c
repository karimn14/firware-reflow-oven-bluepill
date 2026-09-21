/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "hardware_test.h"
#include "cdc_console.h"
#include "reflow.h"

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
osThreadId_t inputTaskHandle;
const osThreadAttr_t inputTask_attributes = {
  .name = "inputTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
static StaticTask_t cdcTaskControlBlock;
static StackType_t cdcTaskStack[256];
osThreadId_t cdcTaskHandle;
const osThreadAttr_t cdcTask_attributes = {
  .name = "cdcTask",
  .cb_mem = &cdcTaskControlBlock,
  .cb_size = sizeof(cdcTaskControlBlock),
  .stack_mem = cdcTaskStack,
  .stack_size = sizeof(cdcTaskStack),
  .priority = (osPriority_t) osPriorityLow,
};
static StaticTask_t reflowTaskControlBlock;
static StackType_t reflowTaskStack[128];
osThreadId_t reflowTaskHandle;
const osThreadAttr_t reflowTask_attributes = {
  .name = "reflowTask",
  .cb_mem = &reflowTaskControlBlock,
  .cb_size = sizeof(reflowTaskControlBlock),
  .stack_mem = reflowTaskStack,
  .stack_size = sizeof(reflowTaskStack),
  .priority = (osPriority_t) osPriorityNormal,
};

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void StartInputTask(void *argument);
void StartCdcTask(void *argument);
void StartReflowTask(void *argument);

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

extern void MX_USB_DEVICE_Init(void);
void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
  Reflow_Init();

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

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  inputTaskHandle = osThreadNew(StartInputTask, NULL, &inputTask_attributes);
  cdcTaskHandle = osThreadNew(StartCdcTask, NULL, &cdcTask_attributes);
  reflowTaskHandle = osThreadNew(StartReflowTask, NULL,
                                 &reflowTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* init code for USB_DEVICE */
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN StartDefaultTask */
  HardwareTest_Init();

  /* Infinite loop */
  for(;;)
  {
    HardwareTest_Run();
    osDelay(10);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void StartInputTask(void *argument)
{
  (void)argument;
  for (;;)
  {
    HardwareTest_InputRun();
    osDelay(10);
  }
}

void StartCdcTask(void *argument)
{
  CDC_Console_Task(argument);
}

void StartReflowTask(void *argument)
{
  Reflow_Task(argument);
}

/* USER CODE END Application */

