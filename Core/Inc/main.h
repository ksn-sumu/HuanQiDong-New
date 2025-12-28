/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.h
 * @brief          : Header for main.c file.
 *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

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
#define VTG_IN_Pin GPIO_PIN_0
#define VTG_IN_GPIO_Port GPIOA
#define CRT1_Pin GPIO_PIN_1
#define CRT1_GPIO_Port GPIOA
#define CRT4_Pin GPIO_PIN_2
#define CRT4_GPIO_Port GPIOA
#define CRT2_Pin GPIO_PIN_3
#define CRT2_GPIO_Port GPIOA
#define CRT3_Pin GPIO_PIN_4
#define CRT3_GPIO_Port GPIOA
#define CTRL0_Pin GPIO_PIN_0
#define CTRL0_GPIO_Port GPIOB
#define CTRL1_Pin GPIO_PIN_1
#define CTRL1_GPIO_Port GPIOB
#define CTRL2_Pin GPIO_PIN_2
#define CTRL2_GPIO_Port GPIOB
#define CTRL3_Pin GPIO_PIN_10
#define CTRL3_GPIO_Port GPIOB
#define CTRL4_Pin GPIO_PIN_11
#define CTRL4_GPIO_Port GPIOB
#define D1_Pin GPIO_PIN_5
#define D1_GPIO_Port GPIOB
#define D1_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
