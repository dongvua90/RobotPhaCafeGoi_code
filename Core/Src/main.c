/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"
#include "i2c.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "OledUI.h"
#include "ssd1306_fonts.h"
#include <stdbool.h>
#include <stdio.h>
#include "stepper_drv.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VALSV_KEO_IDLE 		1200		// lưỡi kéo ở vị trí nghỉ
#define VALSV_KEO_OPEN		1200		// Lưỡi kéo mở ra
#define VALSV_KEO_CLOSE		2300		// Lưỡi kéo khép lại
#define VALSV_THOTHUT_IDLE	2300		// Vị trí nghỉ để tránh va chạm
#define VALSV_THOTHUT_LV0	2300		// Vị trí kéo thụt vào hẳn
#define VALSV_THOTHUT_LV1	1600		// Vị trí kéo ở giữa
#define VALSV_THOTHUT_LV2	1250		// Vị trí kéo thò ra xa nhất
#define VALSV_LATGOI_IDLE	1800		// Vị trí nghỉ để tránh va chạm
#define VALSV_LATGOI_LOCK	2300		// Vị trí Servo giữ khay đựng gói cafe để cắt miệng gói, tránh gói bị kéo đẩy nghiêng
#define VALSV_LATGOI_LATMAX	600			// Vị trí Servo đẩy gói cafe đổ lớn nhất
#define VALSV_LATGOI_LATMIN	800			// Vị trí Servo đẩy gói cafe đổ nhỏ nhất, dùng cho lắc gói


#define J1_SPEED              3000
#define J1_ACCEL_MAX          20000
#define J1_HOME_SPEED         3000
#define J2_SPEED              30000
#define J2_ACCEL_MAX          20000
#define J2_HOME_SPEED         2000
#define J1_MOVE_TIMEOUT_MS    15000U
#define J2_MOVE_TIMEOUT_MS    15000U
#define HOME_SEEK_DISTANCE    25600

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
const long offsetToHome = 420;			// số step từ vị trí homming đến khay đầu tiên
const long stepSpaceKhay = 1333;		// số step để di chuyển tới khay tiếp theo
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

static StepperAxis motor_j1;
static StepperAxis motor_j2;

uint8_t countPacket =18;	// số lượng gói cafe
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Pulse Width for Servo: 600->2500

void InitRcServos()
{
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);	// kéo
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);	// thò thụt
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);	// lật gói
	TIM1->CCR1 = VALSV_KEO_IDLE;
	TIM1->CCR2 = VALSV_THOTHUT_IDLE;
	TIM1->CCR3 = VALSV_LATGOI_IDLE;
	HAL_Delay(2000);
	TIM1->CCR1 = 0;
	TIM1->CCR2 = 0;
	TIM1->CCR3 = 0;
}

void khaylatup()
{
	TIM1->CCR3 = 600;
	HAL_Delay(1700);
}

void khayLatngua()
{
	TIM1->CCR3=2100;
	HAL_Delay(1500);
	TIM1->CCR3=0;
}
void khayLock()
{
	TIM1->CCR3=2300;
	HAL_Delay(1000);
	TIM1->CCR3 = 0;
}
void keocat()
{
	TIM1->CCR1 = 2300;
	HAL_Delay(900);
	TIM1->CCR1 = 1200;
	HAL_Delay(900);
}
void catMiengGoi()
{
	  TIM1->CCR2 = 2300;
	  HAL_Delay(500);
	  TIM1->CCR2 = 1600;
	  HAL_Delay(500);
	  keocat();
	  TIM1->CCR2 = 1250;
	  HAL_Delay(500);
	  keocat();
	  TIM1->CCR2 = 2300;
	  HAL_Delay(500);
	  TIM1->CCR1 = 0;
	  TIM1->CCR2 = 0;
}
void khayRung()
{
//	Stepper_Enable(&motor_j1, true);
//	long currentpos = motor_j1.position;
//	for(int i=0;i<50;i++)
//	{
//		TIM1->CCR3 = 600;
//		(void)Stepper_MoveTo(&motor_j1, currentpos + 70);
//
//		TIM1->CCR3 = 800;
//		(void)Stepper_MoveTo(&motor_j1, currentpos-70);
//        Motion_WaitJ1DoneExclusive();
//	}
}

void InitMotorJ1()
{
	StepperHwConfig hw;
	StepperProfile profile;

	hw.htim = &htim2;
	hw.tim = TIM2;
    hw.channel = TIM_CHANNEL_1;
    hw.dirPort = J1_DIR_GPIO_Port;
    hw.dirPin = J1_DIR_Pin;
    hw.enPort = J1_EN_GPIO_Port;
    hw.enPin = J1_EN_Pin;
    hw.limPort = LIM_M1_GPIO_Port;
    hw.limPin = LIM_M1_Pin;
    hw.dirCwLevel = GPIO_PIN_RESET;
    hw.enableActiveLow = true;
    profile.maxSpeed = J1_SPEED;
    profile.accel = J1_ACCEL_MAX;
    profile.homeSpeed = J1_HOME_SPEED;
    profile.homeSeekDistance = HOME_SEEK_DISTANCE;
    profile.homeOffset = offsetToHome;
    profile.timeoutMs = J1_MOVE_TIMEOUT_MS;
    Stepper_Init(&motor_j1, &hw, &profile);
    Stepper_StartPwm(&motor_j1);
}

void Brewing()
{
	motor_j1.cfg.homeOffset = offsetToHome;
  if (!Stepper_Home(&motor_j1)) {
    return;
  }
	while(Stepper_IsBusy(&motor_j1)){
		Stepper_Process1ms(&motor_j1);
//		HAL_Delay(1);
	}
}


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  Ui_RenderBoot();
  InitRcServos();
  InitMotorJ1();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  int next = HomeScreen();
	  if(next==1)	// Brewing
	  {
	  	  if(!SENSOR_WATER_IS_OK)
	  	  {
	  		  WarningScreen(LOW_WATER);
	  	  }
	  	  if(!SENSOR_CUP_IS_OK){
	  		  WarningScreen(NO_CUP);
	  	  }
	  	  if(countPacket <=0){
	  		  WarningScreen(NO_COFFEE);
	  	  }
	  	  for(int i=0;i<6;i++)
	  	  {
	  		  UI_Brewing(i);
	  		  HAL_Delay(2000);
	  	  }
	  	  Brewing();
	  	  BrewCompleteScreen();
	  }else if(next==2)
	  {
	  	  PacketEditorScreen();
	  }

	  HAL_Delay(1);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2) {
    Stepper_OnPulseISR(&motor_j1);
  }
  if (htim->Instance == TIM3) {
    Stepper_OnPulseISR(&motor_j2);
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
