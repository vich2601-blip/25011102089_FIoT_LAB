/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 9 - Task 9B : String-based application logic over UART
  *                    Commands LED ON / LED OFF / LED TOGGLE (+ STATUS, CRLF TEST, HELP) control PC13.
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
#include <string.h>
#include <ctype.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PC_UART          (&huart2)       /* USART2 PA2/PA3, 115200-8-N-1 */
#define CMD_MAX_LEN      32U

#define LED_PORT         GPIOC           /* PC13, active-low */
#define LED_PIN          GPIO_PIN_13
#define LED_ON_LEVEL     GPIO_PIN_RESET
#define LED_OFF_LEVEL    GPIO_PIN_SET
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static char    cmd_buf[CMD_MAX_LEN + 1];
static uint8_t cmd_len = 0;
static uint8_t led_on  = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void uart_puts(const char *s)
{
  HAL_UART_Transmit(PC_UART, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

static void uart_putc(char c)
{
  HAL_UART_Transmit(PC_UART, (uint8_t *)&c, 1, HAL_MAX_DELAY);
}

static void led_apply(void)
{
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, led_on ? LED_ON_LEVEL : LED_OFF_LEVEL);
}

/* Upper-case the command and remove leading/trailing spaces, so
 * "led on", "LED ON " and " Led On" are all accepted. */
static char *normalise(char *s)
{
  char *end;
  while (*s == ' ') s++;
  for (char *p = s; *p; p++) *p = (char)toupper((unsigned char)*p);
  end = s + strlen(s);
  while (end > s && end[-1] == ' ') *--end = '\0';
  return s;
}

/* Demonstrates the effect of CR and LF on the terminal (lab sheet table) */
static void crlf_demo(void)
{
  uart_puts("HELLO\r\n");               /* new line, start of line         */
  uart_puts("HELLO\r");                 /* back to start: next text overwrites */
  uart_puts("HELLO\n");                 /* down one line, same column       */
  uart_puts("\r\nHELLO\r\nWORLD\r\n");  /* two separate lines               */
}

static void handle_command(char *raw)
{
  char *cmd = normalise(raw);

  if (strcmp(cmd, "LED ON") == 0)          { led_on = 1;       led_apply(); uart_puts("OK: LED is ON\r\n"); }
  else if (strcmp(cmd, "LED OFF") == 0)    { led_on = 0;       led_apply(); uart_puts("OK: LED is OFF\r\n"); }
  else if (strcmp(cmd, "LED TOGGLE") == 0) { led_on = !led_on; led_apply(); uart_puts(led_on ? "OK: LED toggled -> ON\r\n" : "OK: LED toggled -> OFF\r\n"); }
  else if (strcmp(cmd, "STATUS") == 0)     { uart_puts(led_on ? "LED is ON\r\n" : "LED is OFF\r\n"); }
  else if (strcmp(cmd, "CRLF TEST") == 0)  { crlf_demo(); }
  else if (strcmp(cmd, "HELP") == 0)       { uart_puts("Commands: LED ON | LED OFF | LED TOGGLE | STATUS | CRLF TEST | HELP\r\n"); }
  else                                     { uart_puts("ERR: unknown command '"); uart_puts(cmd); uart_puts("' (type HELP)\r\n"); }
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
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  led_on = 0;
  led_apply();
  uart_puts("\r\nCAT9B - UART command interface. Type HELP and press Enter.\r\n> ");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t ch;

    if (HAL_UART_Receive(PC_UART, &ch, 1, HAL_MAX_DELAY) == HAL_OK)
    {
      if (ch == '\r' || ch == '\n')              /* end of command */
      {
        if (cmd_len > 0)
        {
          cmd_buf[cmd_len] = '\0';
          uart_puts("\r\n");
          handle_command(cmd_buf);
          cmd_len = 0;
          uart_puts("> ");
        }
      }
      else if (ch == '\b' || ch == 0x7F)         /* backspace / delete */
      {
        if (cmd_len > 0)
        {
          cmd_len--;
          uart_puts("\b \b");
        }
      }
      else if (cmd_len < CMD_MAX_LEN)            /* store and echo */
      {
        cmd_buf[cmd_len++] = (char)ch;
        uart_putc((char)ch);
      }
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

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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
