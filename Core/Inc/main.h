/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define X1_Pin GPIO_PIN_5
#define X1_GPIO_Port GPIOE
#define X2_Pin GPIO_PIN_6
#define X2_GPIO_Port GPIOE
#define Y6_Pin GPIO_PIN_0
#define Y6_GPIO_Port GPIOA
#define Y7_Pin GPIO_PIN_1
#define Y7_GPIO_Port GPIOA
#define Y4_Pin GPIO_PIN_2
#define Y4_GPIO_Port GPIOA
#define Y5_Pin GPIO_PIN_3
#define Y5_GPIO_Port GPIOA
#define Y2_Pin GPIO_PIN_6
#define Y2_GPIO_Port GPIOA
#define Y3_Pin GPIO_PIN_7
#define Y3_GPIO_Port GPIOA
#define X15_Pin GPIO_PIN_0
#define X15_GPIO_Port GPIOB
#define Y1_Pin GPIO_PIN_1
#define Y1_GPIO_Port GPIOB
#define X13_Pin GPIO_PIN_9
#define X13_GPIO_Port GPIOE
#define X14_Pin GPIO_PIN_11
#define X14_GPIO_Port GPIOE
#define X11_Pin GPIO_PIN_13
#define X11_GPIO_Port GPIOE
#define X12_Pin GPIO_PIN_14
#define X12_GPIO_Port GPIOE
#define X9_Pin GPIO_PIN_12
#define X9_GPIO_Port GPIOD
#define X10_Pin GPIO_PIN_13
#define X10_GPIO_Port GPIOD
#define X7_Pin GPIO_PIN_14
#define X7_GPIO_Port GPIOD
#define X8_Pin GPIO_PIN_15
#define X8_GPIO_Port GPIOD
#define X5_Pin GPIO_PIN_6
#define X5_GPIO_Port GPIOC
#define X6_Pin GPIO_PIN_7
#define X6_GPIO_Port GPIOC
#define X3_Pin GPIO_PIN_8
#define X3_GPIO_Port GPIOC
#define X4_Pin GPIO_PIN_9
#define X4_GPIO_Port GPIOC
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
