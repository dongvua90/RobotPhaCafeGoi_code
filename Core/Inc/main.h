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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum {
    LOW_WATER,
    NO_CUP,
    NO_COFFEE,
} WarningState_t;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
#define F_CPU 72000000
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
extern uint8_t countPacket;	// số lượng gói cafe
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define J1_DIR_Pin GPIO_PIN_1
#define J1_DIR_GPIO_Port GPIOA
#define J1_EN_Pin GPIO_PIN_2
#define J1_EN_GPIO_Port GPIOA
#define SENSOR_COC_Pin GPIO_PIN_3
#define SENSOR_COC_GPIO_Port GPIOA
#define SENSOR_MUCNUOC_Pin GPIO_PIN_4
#define SENSOR_MUCNUOC_GPIO_Port GPIOA
#define J2_DIR_Pin GPIO_PIN_5
#define J2_DIR_GPIO_Port GPIOA
#define J2_EN_Pin GPIO_PIN_7
#define J2_EN_GPIO_Port GPIOA
#define LIM_M1_Pin GPIO_PIN_0
#define LIM_M1_GPIO_Port GPIOB
#define LIM_M2_Pin GPIO_PIN_1
#define LIM_M2_GPIO_Port GPIOB
#define BOIL_Pin GPIO_PIN_13
#define BOIL_GPIO_Port GPIOB
#define MOTOR_KHUAY_Pin GPIO_PIN_14
#define MOTOR_KHUAY_GPIO_Port GPIOB
#define BTN_DOWN_Pin GPIO_PIN_15
#define BTN_DOWN_GPIO_Port GPIOB
#define TIM1_CH1_KEO_Pin GPIO_PIN_8
#define TIM1_CH1_KEO_GPIO_Port GPIOA
#define TIM1_CH2_KEOTHOTHUT_Pin GPIO_PIN_9
#define TIM1_CH2_KEOTHOTHUT_GPIO_Port GPIOA
#define BTN_ENTER_Pin GPIO_PIN_4
#define BTN_ENTER_GPIO_Port GPIOB
#define BTN_BACK_Pin GPIO_PIN_5
#define BTN_BACK_GPIO_Port GPIOB
#define TIM4_CH1_PUMP_Pin GPIO_PIN_6
#define TIM4_CH1_PUMP_GPIO_Port GPIOB
#define BTN_UP_Pin GPIO_PIN_7
#define BTN_UP_GPIO_Port GPIOB
#define OLED_I2C1_SCL_Pin GPIO_PIN_8
#define OLED_I2C1_SCL_GPIO_Port GPIOB
#define OLED_I2C1_SDA_Pin GPIO_PIN_9
#define OLED_I2C1_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
#define PACKET_MAX 18			// Số gói cafe tối đa

#define BTN_UP_PRESS 	HAL_GPIO_ReadPin(BTN_UP_GPIO_Port, 	 BTN_UP_Pin)	== GPIO_PIN_RESET
#define BTN_DOWN_PRESS 	HAL_GPIO_ReadPin(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin)	== GPIO_PIN_RESET
#define BTN_BACK_PRESS 	HAL_GPIO_ReadPin(BTN_BACK_GPIO_Port, BTN_BACK_Pin)	== GPIO_PIN_RESET
#define BTN_ENTER_PRESS HAL_GPIO_ReadPin(BTN_ENTER_GPIO_Port,BTN_ENTER_Pin)	== GPIO_PIN_RESET

#define SENSOR_WATER_IS_OK 	(HAL_GPIO_ReadPin(SENSOR_MUCNUOC_GPIO_Port, SENSOR_MUCNUOC_Pin)==GPIO_PIN_SET)
#define SENSOR_CUP_IS_OK 	(HAL_GPIO_ReadPin(SENSOR_COC_GPIO_Port, SENSOR_COC_Pin)==GPIO_PIN_RESET)
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
