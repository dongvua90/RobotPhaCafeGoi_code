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
#include "AccelStepper.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define VALSV_KEO_IDLE 		1000		// lưỡi kéo ở vị trí nghỉ
#define VALSV_KEO_OPEN		1000		// Lưỡi kéo mở ra
#define VALSV_KEO_CLOSE		2300		// Lưỡi kéo khép lại
#define VALSV_THOTHUT_IDLE	2000	// Vị trí nghỉ để tránh va chạm
#define VALSV_THOTHUT_LV0	2000		// Vị trí kéo thụt vào hẳn
#define VALSV_THOTHUT_LV1	1400		// Vị trí kéo ở giữa
#define VALSV_THOTHUT_LV2	900		// Vị trí kéo thò ra xa nhất	max=900
#define VALSV_LATGOI_IDLE	1830		// Vị trí nghỉ để tránh va chạm
#define VALSV_LATGOI_LOCK	2200		// Vị trí Servo giữ khay đựng gói cafe để cắt miệng gói, tránh gói bị kéo đẩy nghiêng
#define VALSV_LATGOI_LATMAX	600			// Vị trí Servo đẩy gói cafe đổ lớn nhất
#define VALSV_LATGOI_LATMIN	800			// Vị trí Servo đẩy gói cafe đổ nhỏ nhất, dùng cho lắc gói

#define PUMP_SPEED			30				// % tốc độ của bơm
#define PUMP_TIMERUN_HIGH	18000			// Thời gian bật bơm cho cốc high
#define PUMP_TIMERUN_MEDIUM	13000			// Thời gian bật bơm cho cốc medium
#define PUMP_TIMERUN_SMALL	8000			// Thời gian bật bơm cho cốc small

#define MIXING_RANGE 	40000			// Số step cho 1 lượt quay Mixing
#define MIXING_TIMES 	8				// Số lần mixing đảo


#define J1_SPEED              12000
#define J1_ACCEL_MAX          50000
#define J1_HOME_SPEED         2000
#define J2_SPEED              15000
#define J2_ACCEL_MAX          20000
#define J2_HOME_SPEED         2000
#define J1_MOVE_TIMEOUT_MS    15000U
#define J2_MOVE_TIMEOUT_MS    15000U
#define HOME_SEEK_DISTANCE    25600



/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
const long offsetToHome = 380;			// số step từ vị trí homming đến khay đầu tiên
const long stepSpaceKhay = 1333;		// số step để di chuyển tới khay tiếp theo
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

struct AccelStepperData motor_j1_data,motor_j2_data,motor_j3_data;// Variable of Stepper
volatile bool j1IsHoming = false;
uint8_t countPacket =18;	// số lượng gói cafe
int8_t levelWater = 1;		// lượng nước pha cafe gói


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// for Motor J1 J2
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance==TIM2)								// Stepper J1
	{
		if(j1IsHoming){
			if(HAL_GPIO_ReadPin(LIM_M1_GPIO_Port, LIM_M1_Pin)==GPIO_PIN_RESET)
			{
				motor_j1_data._currentPos=0;
				motor_j1_data._targetPos =0;
				j1IsHoming = false;
			}
		}
		run(&motor_j1_data);							// Update position & speed each pulse
	}
	if(htim->Instance==TIM3)							// Stepper J2
	{
		run(&motor_j2_data);
	}
}
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

void InitPump()
{
	HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
	TIM4->CCR1 = 0;
}
void AddingWater()
{
    TIM4->CCR1 = (uint16_t)((PUMP_SPEED * 2000U) / 100U);	// Bật bơm nước
    HAL_GPIO_WritePin(BOIL_GPIO_Port, BOIL_Pin, GPIO_PIN_SET);		// Bật thanh gia nhiệt, cần kiểm tra cẩn thận bước này
    switch(levelWater)
    {
    case 0:
    	 HAL_Delay(PUMP_TIMERUN_SMALL);
    	break;
    case 1:
    	HAL_Delay(PUMP_TIMERUN_MEDIUM);
    	break;
    case 2:
    	HAL_Delay(PUMP_TIMERUN_HIGH);
    	break;
    }
    TIM4->CCR1=0;
    HAL_GPIO_WritePin(BOIL_GPIO_Port, BOIL_Pin, GPIO_PIN_RESET);	// Tắt thanh gia nhiệt, cần kiểm tra cẩn thận bước này
}


void khaylatup()
{
	TIM1->CCR3 = VALSV_LATGOI_LATMAX;
	HAL_Delay(1700);
}

void khayLatngua()
{
	TIM1->CCR3=VALSV_LATGOI_LOCK; //2100
	HAL_Delay(1500);
	TIM1->CCR3=0;
}
void khayLock()
{
	TIM1->CCR3=VALSV_LATGOI_LOCK;
	HAL_Delay(1000);
	TIM1->CCR3 = 0;
}

void khayIdle()
{
	TIM1->CCR3=VALSV_LATGOI_IDLE;
	HAL_Delay(500);
}
void keocat()
{
	TIM1->CCR1 =VALSV_KEO_CLOSE; //2300;
	HAL_Delay(900);
	TIM1->CCR1 = VALSV_KEO_OPEN;//1200;
	HAL_Delay(900);
}
void catMiengGoi()
{
	  TIM1->CCR2 = VALSV_THOTHUT_LV0; //2300;
	  HAL_Delay(500);
	  TIM1->CCR2 =  VALSV_THOTHUT_LV1;//1600;
	  HAL_Delay(500);
	  keocat();
	  TIM1->CCR2 = VALSV_THOTHUT_LV2;//1250;
	  HAL_Delay(500);
	  keocat();
	  TIM1->CCR2 = VALSV_THOTHUT_LV0;//2300;
	  HAL_Delay(500);
	  TIM1->CCR1 = 0;
	  TIM1->CCR2 = 0;
}

void J1MoveTo(long pos){
	enableStepper(&motor_j1_data, ON);
	moveTo(&motor_j1_data, pos);
	run(&motor_j1_data);										// Prime first step & set ARR
	while( isRunning(&motor_j1_data)>0){						// run() called from TIM2 interrupt
		__NOP();
	}
	enableStepper(&motor_j1_data, OFF);
}

void J2MoveTo(long pos){
	enableStepper(&motor_j2_data, ON);
	moveTo(&motor_j2_data, pos);
	run(&motor_j2_data);										// Prime first step & set ARR
	while( isRunning(&motor_j2_data)>0){						// run() called from TIM3 interrupt
		__NOP();
	}
	enableStepper(&motor_j2_data, OFF);
}

void khayRung()
{
	setMaxSpeed(&motor_j1_data, J1_SPEED);
	long currentpos = motor_j1_data._currentPos;
	for(int i=0;i<20;i++)
	{
		TIM1->CCR3 = 600;
		J1MoveTo(currentpos + 70);

		TIM1->CCR3 = 800;
		J1MoveTo(currentpos-70);
	}
}

void InitMotorJ1()
{
	motor_j1_data.GPIO_PIN_Dir		= J1_DIR_Pin;
	motor_j1_data.GPIO_PORT_Dir		= J1_DIR_GPIO_Port;
	motor_j1_data.GPIO_PORT_Enable	= J1_EN_GPIO_Port;
	motor_j1_data.GPIO_PIN_Enable	= J1_EN_Pin;
	motor_j1_data.USER_TIMER		= TIM2;
	motor_j1_data.TIM_CHANEL		= TIM_CHANNEL_1;
	motor_j1_data.isStop			= false;
	AccelStepper_init(&motor_j1_data, htim2, 0, J1_SPEED, J1_ACCEL_MAX);
	HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);	// Start interrupt on global htim2 handle
	enableStepper(&motor_j1_data, OFF);
}

void InitMotorJ2()
{
	motor_j2_data.GPIO_PIN_Dir		= J2_DIR_Pin;
	motor_j2_data.GPIO_PORT_Dir		= J2_DIR_GPIO_Port;
	motor_j2_data.GPIO_PORT_Enable	= J2_EN_GPIO_Port;
	motor_j2_data.GPIO_PIN_Enable	= J2_EN_Pin;
	motor_j2_data.USER_TIMER		= TIM3;
	motor_j2_data.TIM_CHANEL		= TIM_CHANNEL_1;
	motor_j2_data.isStop			= false;
	AccelStepper_init(&motor_j2_data, htim3, 0, J2_SPEED, J2_ACCEL_MAX);
	HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_1);	// Start interrupt on global htim3 handle
	enableStepper(&motor_j2_data, OFF);
}

void J1Homing()
{
	j1IsHoming = true;
	setMaxSpeed(&motor_j1_data, J1_HOME_SPEED);
	J1MoveTo(-HOME_SEEK_DISTANCE);
}

void Mixing()
{
	for(int i=0;i<MIXING_TIMES;i++)
	{
		J2MoveTo(MIXING_RANGE);
		HAL_Delay(500);
		J2MoveTo(0);
		HAL_Delay(500);
	}
	HAL_Delay(1000);
}
void Brewing()
{
	// Bước 0:
	UI_Brewing(0);
	khayIdle();
	// Bước 1: di chuyển bệ khay đến vị trí gói cafe cần thao tác
	J1Homing();
	// tính khoảng cách đến gói cafe
	long packetIdx = PACKET_MAX - (int32_t)countPacket;
	if (packetIdx < 0 || packetIdx >=PACKET_MAX) {
		return;											// nếu countPacket<=0 hoặc số gói lớn hơn PACKET_MAX thì return
	}
	long packetTarget = (int32_t)offsetToHome + (packetIdx * (int32_t)stepSpaceKhay);
	setMaxSpeed(&motor_j1_data, J1_SPEED);
	J1MoveTo(packetTarget);
	HAL_Delay(300);

	// Bước 2: Cắt miệng gói cafe
	UI_Brewing(1);
	khayLock();		// sử dụng động cơ lật gói tì vào khay chứa gói cafe ngăn không cho khay chứa di chuyển trong quá trình cắt miệng gói
	catMiengGoi();

	// Bước 3: Đổ cafe vào cốc
	UI_Brewing(2);
	khaylatup();
	khayRung();
	khayLatngua();
	khayIdle();

	// Bước 4: Đun sôi nước và Bơm nước vào cốc
	UI_Brewing(3);
	AddingWater();

	// Bước 5: Khuấy
	UI_Brewing(4);
	Mixing();

	countPacket--;
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
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  InitPump();
  ssd1306_Init();
  Ui_RenderBoot();
  InitRcServos();
  InitMotorJ1();
  InitMotorJ2();



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
	  	  }else if(!SENSOR_CUP_IS_OK){
	  		  WarningScreen(NO_CUP);
	  	  }else if(countPacket <=0){
	  		  WarningScreen(NO_COFFEE);
	  	  }else{
	  		  Brewing();
	  		  BrewCompleteScreen();
	  	  }
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
