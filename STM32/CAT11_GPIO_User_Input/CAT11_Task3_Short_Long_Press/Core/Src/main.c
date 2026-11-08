/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 11 - Task 3 : Short / long / very-long press detection on B1
  *                    Short = toggle LD2, Long (>=0.6 s) = blink mode, Very long (>=3 s) = reset.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint8_t  stable;        /* debounced level, 1 = pressed */
  uint8_t  last_raw;
  uint32_t last_change;
} debounce_t;

typedef enum { BTN_NO_EVENT = 0, BTN_PRESS_EDGE, BTN_RELEASE_EDGE } btn_event_t;

typedef enum { PRESS_SHORT = 0, PRESS_LONG, PRESS_VERY_LONG } press_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* NUCLEO-F401RE on-board parts (UM1724)
 *   LD2 (green LED) : PA5,  ACTIVE-HIGH
 *   B1  (blue key)  : PC13, ACTIVE-LOW (external pull-up on the board)
 * Clock: 8 MHz from the ST-LINK MCO (HSE bypass) -> PLL M=4 N=84 P=2 -> 84 MHz */
#define LED_PORT          GPIOA
#define LED_PIN           GPIO_PIN_5
#define BTN_PORT          GPIOC
#define BTN_PIN           GPIO_PIN_13
#define EXPECTED_SYSCLK   84000000UL

#define DEBOUNCE_MS       30U
#define LONG_PRESS_MS     600U    /* >= 0.6 s : long press                 */
#define VERY_LONG_MS      3000U   /* >= 3 s   : reset                      */
#define BLINK_MS          150U    /* blink mode half-period                */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static debounce_t b1 = { 0 };
static uint32_t   press_start = 0;
static uint32_t   last_blink  = 0;
uint32_t last_duration = 0;      /* Live Expressions: length of the last press in ms */
uint8_t  led_state     = 0;
uint8_t  blink_mode    = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static inline uint8_t b1_pressed(void)
{
  return (HAL_GPIO_ReadPin(BTN_PORT, BTN_PIN) == GPIO_PIN_RESET);
}

static inline void ld2_write(uint8_t on)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Accept a new button level only after it has been stable for DEBOUNCE_MS */
static btn_event_t debounce_update(debounce_t *d, uint8_t raw, uint32_t now)
{
  if (raw != d->last_raw)
  {
    d->last_raw    = raw;
    d->last_change = now;
  }
  else if (raw != d->stable && (now - d->last_change) >= DEBOUNCE_MS)
  {
    d->stable = raw;
    return raw ? BTN_PRESS_EDGE : BTN_RELEASE_EDGE;
  }
  return BTN_NO_EVENT;
}

static press_t classify(uint32_t ms)
{
  if (ms >= VERY_LONG_MS)  return PRESS_VERY_LONG;
  if (ms >= LONG_PRESS_MS) return PRESS_LONG;
  return PRESS_SHORT;
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
  /* USER CODE BEGIN 2 */
  ld2_write(0);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now = HAL_GetTick();

    switch (debounce_update(&b1, b1_pressed(), now))
    {
      case BTN_PRESS_EDGE:
        press_start = now;                       /* record start time            */
        break;

      case BTN_RELEASE_EDGE:
        last_duration = now - press_start;       /* duration = release - start   */
        switch (classify(last_duration))
        {
          case PRESS_SHORT:                      /* short: toggle the LED          */
            if (!blink_mode)
            {
              led_state = !led_state;
              ld2_write(led_state);
            }
            break;
          case PRESS_LONG:                       /* long: blink mode ON / OFF      */
            blink_mode = !blink_mode;
            last_blink = now;
            if (!blink_mode)
              ld2_write(led_state);              /* go back to the remembered state */
            break;
          case PRESS_VERY_LONG:                  /* very long: reset everything    */
            blink_mode = 0;
            led_state  = 0;
            ld2_write(0);
            break;
        }
        break;

      default:
        break;
    }

    if (blink_mode && (now - last_blink) >= BLINK_MS)
    {
      last_blink = now;
      HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
    }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
