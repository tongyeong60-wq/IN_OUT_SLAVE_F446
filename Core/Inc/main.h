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
#include "stm32f4xx_hal.h"

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
#define DIP0_Pin GPIO_PIN_0
#define DIP0_GPIO_Port GPIOC
#define DIP1_Pin GPIO_PIN_1
#define DIP1_GPIO_Port GPIOC
#define DIP2_Pin GPIO_PIN_2
#define DIP2_GPIO_Port GPIOC
#define DIP3_Pin GPIO_PIN_3
#define DIP3_GPIO_Port GPIOC
#define USAR2_TX_Pin GPIO_PIN_2
#define USAR2_TX_GPIO_Port GPIOA
#define USAR2_RX_Pin GPIO_PIN_3
#define USAR2_RX_GPIO_Port GPIOA
#define POTO1_Pin GPIO_PIN_4
#define POTO1_GPIO_Port GPIOC
#define POTO2_Pin GPIO_PIN_5
#define POTO2_GPIO_Port GPIOC
#define OUT5_Pin GPIO_PIN_10
#define OUT5_GPIO_Port GPIOB
#define LOAD_Pin GPIO_PIN_13
#define LOAD_GPIO_Port GPIOB
#define SCLK_Pin GPIO_PIN_14
#define SCLK_GPIO_Port GPIOB
#define SDI_Pin GPIO_PIN_15
#define SDI_GPIO_Port GPIOB
#define POTO3_Pin GPIO_PIN_6
#define POTO3_GPIO_Port GPIOC
#define POTO4_Pin GPIO_PIN_7
#define POTO4_GPIO_Port GPIOC
#define POTO5_Pin GPIO_PIN_8
#define POTO5_GPIO_Port GPIOC
#define USAT1_TX_Pin GPIO_PIN_9
#define USAT1_TX_GPIO_Port GPIOA
#define USAT1_RX_Pin GPIO_PIN_10
#define USAT1_RX_GPIO_Port GPIOA
#define SYS_SWDIO_Pin GPIO_PIN_13
#define SYS_SWDIO_GPIO_Port GPIOA
#define SYS_SWCLK_Pin GPIO_PIN_14
#define SYS_SWCLK_GPIO_Port GPIOA
#define OUT1_Pin GPIO_PIN_6
#define OUT1_GPIO_Port GPIOB
#define OUT2_Pin GPIO_PIN_7
#define OUT2_GPIO_Port GPIOB
#define OUT3_Pin GPIO_PIN_8
#define OUT3_GPIO_Port GPIOB
#define OUT4_Pin GPIO_PIN_9
#define OUT4_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
