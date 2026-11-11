/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 10C/10D - HTTP server: STM32 + ESP8266 TCP server on port 80
  *                    Parses +IPD frames, builds a raw HTTP response, sends it with AT+CIPSEND.
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
#include <stdio.h>
#include "esp_at.h"
#include "wifi_config.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint8_t pending;      /* 1 = a request is waiting for a response */
  char    path[48];     /* requested path, e.g. "/" or "/led/on"   */
} http_request_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PC_UART          (&huart2)       /* debug terminal (PuTTY)              */
#define MAX_LINKS        5U              /* ESP8266 allows link IDs 0..4        */
#define HTTP_PORT        80U

#define LED_PORT         GPIOC           /* on-board LED PC13, active-low       */
#define LED_PIN          GPIO_PIN_13
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static http_request_t requests[MAX_LINKS];
static volatile uint8_t wifi_lost = 0;
static uint32_t pages_served = 0;
static uint8_t  led_on = 0;
static char     ip_addr[20] = "0.0.0.0";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void pc_print(const char *s)
{
  HAL_UART_Transmit(PC_UART, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

/* UART interrupt callbacks -> ESP driver ring buffer */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  esp_at_rx_cplt_callback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  esp_at_error_callback(huart);
}

/* Unsolicited lines from the ESP8266.
 *   "+IPD,0,412:GET /led/on HTTP/1.1"  -> remember link 0 wants "/led/on"
 *   "0,CLOSED"                          -> forget link 0
 *   "WIFI DISCONNECT"                   -> Wi-Fi lost                     */
static void on_esp_event(const char *line)
{
  if (strncmp(line, "+IPD,", 5) == 0)
  {
    uint8_t link = (uint8_t)(line[5] - '0');
    const char *get = strstr(line, ":GET ");
    if (link < MAX_LINKS && get != NULL)
    {
      const char *src = get + 5;
      uint8_t n = 0;
      while (*src && *src != ' ' && n < sizeof(requests[link].path) - 1U)
        requests[link].path[n++] = *src++;
      requests[link].path[n] = '\0';
      requests[link].pending = 1;
    }
  }
  else if (strstr(line, ",CLOSED") != NULL)
  {
    uint8_t link = (uint8_t)(line[0] - '0');
    if (link < MAX_LINKS)
      requests[link].pending = 0;
  }
  else if (strcmp(line, "WIFI DISCONNECT") == 0)
  {
    wifi_lost = 1;
  }
}

/* Build "status line + headers + blank line + body" and send it */
static void http_send(uint8_t link, const char *status, const char *body)
{
  static char response[768];
  int len = snprintf(response, sizeof(response),
                     "HTTP/1.1 %s\r\n"
                     "Content-Type: text/html\r\n"
                     "Content-Length: %u\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     status, (unsigned)strlen(body), body);
  if (len >= (int)sizeof(response))
    len = sizeof(response) - 1;
  esp_at_send(link, response, (uint16_t)len);
  esp_at_close(link);
}

static void handle_request(uint8_t link, const char *path)
{
  static char body[512];
  char msg[80];

  snprintf(msg, sizeof(msg), "[HTTP] link %u  GET %s\r\n", link, path);
  pc_print(msg);

  if (strcmp(path, "/led/on") == 0)       led_on = 1;
  else if (strcmp(path, "/led/off") == 0) led_on = 0;
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, led_on ? GPIO_PIN_RESET : GPIO_PIN_SET);

  if (strcmp(path, "/") == 0 || strncmp(path, "/led/", 5) == 0)
  {
    pages_served++;
    snprintf(body, sizeof(body),
             "<html><body>"
             "<h1>STM32 IoT Node</h1>"
             "<p>Hello from STM32 Blue Pill</p>"
             "<p>LED: <b>%s</b> &nbsp; <a href=\"/led/on\">ON</a> | <a href=\"/led/off\">OFF</a></p>"
             "<p>Pages served: %lu, uptime: %lu s</p>"
             "</body></html>",
             led_on ? "ON" : "OFF", (unsigned long)pages_served, (unsigned long)(HAL_GetTick() / 1000U));
    http_send(link, "200 OK", body);
  }
  else
  {
    http_send(link, "404 Not Found", "<html><body><h1>404</h1><p>Page not found</p></body></html>");
  }
}

static void server_start(void)
{
  char msg[64];
  esp_at_get_ip(ip_addr, sizeof(ip_addr));
  if (esp_at_start_server(HTTP_PORT) == ESP_OK)
  {
    snprintf(msg, sizeof(msg), "Server running: http://%s/\r\n", ip_addr);
    pc_print(msg);
  }
  else
  {
    pc_print("AT+CIPSERVER failed\r\n");
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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  GPIO_InitTypeDef led = { 0 };
  char line[ESP_LINE_MAX];
  
  /* PC13 LED (not configured in CubeMX for this project, so set up here) */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  HAL_GPIO_WritePin(LED_PORT, LED_PIN, GPIO_PIN_SET);
  led.Pin   = LED_PIN;
  led.Mode  = GPIO_MODE_OUTPUT_PP;
  led.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_PORT, &led);
  
  pc_print("\r\nCAT10D - STM32 HTTP server through the ESP8266\r\n");
  esp_at_init(&huart1, on_esp_event);
  
  if (esp_at_startup() != ESP_OK)
  {
    pc_print("ESP8266 not responding\r\n");
    Error_Handler();
  }
  pc_print("Connecting to " WIFI_SSID " ...\r\n");
  if (esp_at_join_ap(WIFI_SSID, WIFI_PASSWORD) != ESP_OK)
  {
    pc_print("Wi-Fi join failed - check wifi_config.h\r\n");
    Error_Handler();
  }
  pc_print("Wi-Fi connected\r\n");
  server_start();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* read and discard ordinary lines; URCs go to on_esp_event() */
    while (esp_at_read_line(line, sizeof(line), 0) >= 0) { }

    for (uint8_t link = 0; link < MAX_LINKS; link++)
    {
      if (requests[link].pending)
      {
        requests[link].pending = 0;
        handle_request(link, requests[link].path);
      }
    }

    if (wifi_lost)
    {
      pc_print("Wi-Fi lost, waiting for reconnection...\r\n");
      /* the ESP8266 re-joins the AP by itself; STATUS:5 = not connected yet */
      do
      {
        HAL_Delay(2000);
      } while (esp_at_cmd("AT+CIPSTATUS", line, sizeof(line), 2000) != ESP_OK || strstr(line, "STATUS:5") != NULL);
      wifi_lost = 0;
      server_start();
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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

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
