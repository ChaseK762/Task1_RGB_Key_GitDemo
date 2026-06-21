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
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum
{
  RGB_STATE_GREEN = 0,
  RGB_STATE_RED,
  RGB_STATE_BREATH
} RGB_StateTypeDef;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RGB_ON          GPIO_PIN_SET
#define RGB_OFF         GPIO_PIN_RESET
#define KEY_DOWN        GPIO_PIN_RESET
#define RGB_G_CHANNEL   TIM_CHANNEL_2
#define BEEP_CHANNEL    TIM_CHANNEL_4

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static RGB_StateTypeDef rgb_state = RGB_STATE_GREEN;
static int key2_off = 0;
static int breath = 0;
static int breath_dir = 1;
static int beep_intermit_on = 0;
static uint32_t beep_last_toggle_tick = 0;

static const int key_debounce_ms = 20;
static const int key2_refresh_ms = 10;
static const int breath_delay_ms = 10;
static const int breath_step = 10;
static const int green_pwm_max = 999;
static const int beep_pwm_compare = 100;
static const int beep_intermit_ms = 500;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static int Key_DebouncePressed(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
static void Key_WaitRelease(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
static void RGB_G_Set(int light);
static void BEEP_On(void);
static void BEEP_Off(void);
static void BEEP_Intermit(void);
static void KEY1_Process(void);
static void KEY2_Process(void);
static void RGB_ShowState(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int Key_DebouncePressed(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
  if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == KEY_DOWN) {
    HAL_Delay(key_debounce_ms);
    if (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == KEY_DOWN) {
      return 1;
    }
  }
  return 0;
}

static void Key_WaitRelease(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
  while (HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == KEY_DOWN) {}
  HAL_Delay(key_debounce_ms);
}

static void RGB_G_Set(int light)
{
  if (light < 0) {
    light = 0;
  }
  if (light > green_pwm_max) {
    light = green_pwm_max;
  }

  __HAL_TIM_SET_COMPARE(&htim3, RGB_G_CHANNEL, light);
}

static void BEEP_On(void)
{
  __HAL_TIM_SET_COMPARE(&htim4, BEEP_CHANNEL, beep_pwm_compare);
}

static void BEEP_Off(void)
{
  __HAL_TIM_SET_COMPARE(&htim4, BEEP_CHANNEL, 0);
}

static void BEEP_Intermit(void)
{
  uint32_t now = HAL_GetTick();

  if ((now - beep_last_toggle_tick) >= beep_intermit_ms) {
    beep_last_toggle_tick = now;
    beep_intermit_on = !beep_intermit_on;
  }

  if (beep_intermit_on == 1) {
    BEEP_On();
  } else {
    BEEP_Off();
  }
}

static void KEY1_Process(void)
{
  if (Key_DebouncePressed(KEY1_GPIO_Port, KEY1_Pin)) {
    Key_WaitRelease(KEY1_GPIO_Port, KEY1_Pin);

    if (key2_off == 1) {
      rgb_state = RGB_STATE_GREEN;
      key2_off = 0;
    } else {
      rgb_state++;
      if (rgb_state > RGB_STATE_BREATH) {
        rgb_state = RGB_STATE_GREEN;
      }
    }

    if (rgb_state == RGB_STATE_BREATH) {
      breath = 0;
      breath_dir = 1;
      beep_intermit_on = 0;
      beep_last_toggle_tick = HAL_GetTick();
    }
  }
}

static void KEY2_Process(void)
{
  if (Key_DebouncePressed(KEY2_GPIO_Port, KEY2_Pin)) {
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
    RGB_G_Set(green_pwm_max);
    BEEP_Off();

    while (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == KEY_DOWN) {
      HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
      HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
      RGB_G_Set(green_pwm_max);
      BEEP_Off();
      HAL_Delay(key2_refresh_ms);
    }

    HAL_Delay(key_debounce_ms);
    BEEP_Off();
    key2_off = 1;
  }
}

static void RGB_ShowState(void)
{
  if (key2_off == 1) {
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
    RGB_G_Set(0);
    BEEP_Off();
  } else if (rgb_state == RGB_STATE_GREEN) {
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
    RGB_G_Set(green_pwm_max);
    BEEP_On();
  } else if (rgb_state == RGB_STATE_RED) {
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_ON);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
    RGB_G_Set(0);
    BEEP_Off();
  } else if (rgb_state == RGB_STATE_BREATH) {
    HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
    HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
    RGB_G_Set(breath);
    BEEP_Intermit();

    breath += breath_dir * breath_step;
    if (breath >= green_pwm_max) {
      breath = green_pwm_max;
      breath_dir = -1;
    }
    if (breath <= 0) {
      breath = 0;
      breath_dir = 1;
    }

    HAL_Delay(breath_delay_ms);
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
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_PWM_Start(&htim3, RGB_G_CHANNEL);
  HAL_TIM_PWM_Start(&htim4, BEEP_CHANNEL);

  HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, RGB_OFF);
  HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, RGB_OFF);
  RGB_G_Set(green_pwm_max);
  BEEP_On();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    KEY2_Process();
    KEY1_Process();
    RGB_ShowState();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
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
