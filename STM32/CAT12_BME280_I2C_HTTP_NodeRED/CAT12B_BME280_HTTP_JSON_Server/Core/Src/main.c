/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 12 - Raw HTTP response with BME280 data (JSON)
  *                    STM32 builds {"status","temperature","humidity","pressure"} and serves it through the ESP8266.
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
#include "bme280.h"
#include "esp_at.h"
#include "wifi_config.h"
#include "fmt_fixed.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct
{
  uint8_t pending;
  char    path[48];
} http_request_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DBG_UART          (&huart6)   /* USART6: PC6 TX, PC7 RX -> USB-TTL -> PuTTY 115200 */
/* BME280 on I2C1: PB6 = SCL, PB7 = SDA, VCC = 3.3 V, SDO = GND -> address 0x76 */

/* ESP8266 (Wemos D1 mini, AT firmware) on USART1: PA9 TX -> Wemos RX, PA10 RX <- Wemos TX */
#define MAX_LINKS          5U
#define HTTP_PORT          80U
#define SENSOR_PERIOD_MS   2000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
static struct bme280_dev      bme;
static struct bme280_settings bme_cfg;
static struct bme280_data     bme_data;
static uint8_t                bme_addr = BME280_I2C_ADDR_PRIM;   /* 0x76 */

/* compensated results - add these to Live Expressions while debugging */
float temperature = 0.0f;    /* deg C */
float pressure    = 0.0f;    /* hPa   */
float humidity    = 0.0f;    /* %RH   */
int8_t bme_status = BME280_OK;
static http_request_t    requests[MAX_LINKS];
static volatile uint8_t  wifi_lost = 0;
static uint32_t          last_read_ms = 0;
static uint32_t          requests_served = 0;
static uint8_t           sensor_ok = 0;
static char              ip_addr[20] = "0.0.0.0";
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);
void   user_delay_us(uint32_t period, void *intf_ptr);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void dbg(const char *s)
{
  HAL_UART_Transmit(DBG_UART, (uint8_t *)s, (uint16_t)strlen(s), 200);
}

/* Connect the Bosch API to our I2C functions, check the chip and configure it */
static int8_t bme_setup(uint8_t osr, uint8_t filter, uint8_t standby)
{
  int8_t r;

  bme.intf_ptr = &bme_addr;
  bme.intf     = BME280_I2C_INTF;
  bme.read     = user_i2c_read;
  bme.write    = user_i2c_write;
  bme.delay_us = user_delay_us;

  r = bme280_init(&bme);                 /* reads chip ID 0x60, soft reset, trim data */
  if (r != BME280_OK)
    return r;

  r = bme280_get_sensor_settings(&bme_cfg, &bme);   /* start from the current settings */
  if (r != BME280_OK)
    return r;
  bme_cfg.osr_t        = osr;            /* temperature oversampling */
  bme_cfg.osr_p        = osr;            /* pressure oversampling    */
  bme_cfg.osr_h        = osr;            /* humidity oversampling    */
  bme_cfg.filter       = filter;         /* IIR filter coefficient   */
  bme_cfg.standby_time = standby;        /* time between conversions in normal mode */
  r = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &bme_cfg, &bme);
  if (r != BME280_OK)
    return r;

  return bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme);   /* continuous */
}

/* Read compensated values; returns 1 on success */
static int bme_read(void)
{
  bme_status = bme280_get_sensor_data(BME280_ALL, &bme_data, &bme);
  if (bme_status != BME280_OK)
    return 0;
  temperature = (float)bme_data.temperature;
  pressure    = (float)(bme_data.pressure / 100.0);   /* Pa -> hPa */
  humidity    = (float)bme_data.humidity;
  return 1;
}

static void print_readings(void)
{
  char t[12], p[12], h[12], line[48];
  fmt_2dp(t, sizeof(t), temperature);
  fmt_2dp(p, sizeof(p), pressure);
  fmt_2dp(h, sizeof(h), humidity);
  snprintf(line, sizeof(line), "Temperature : %s \xC2\xB0" "C\r\n", t);  dbg(line);
  snprintf(line, sizeof(line), "Pressure    : %s hPa\r\n", p);           dbg(line);
  snprintf(line, sizeof(line), "Humidity    : %s %%RH\r\n", h);          dbg(line);
}

/* -------------------- ESP8266 glue -------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  esp_at_rx_cplt_callback(huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  esp_at_error_callback(huart);
}

/* "+IPD,<link>,<len>:GET /path HTTP/1.1" -> store the request for the main loop */
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

/* -------------------- HTTP -------------------- */
static void build_json(char *json, size_t size)
{
  char t[12], h[12], p[12];
  if (!sensor_ok)
  {
    snprintf(json, size, "{\"status\":\"error\",\"message\":\"sensor read failed\"}");
    return;
  }
  fmt_2dp(t, sizeof(t), temperature);
  fmt_2dp(h, sizeof(h), humidity);
  fmt_2dp(p, sizeof(p), pressure);
  snprintf(json, size,
           "{\"status\":\"success\",\"temperature\":%s,\"humidity\":%s,\"pressure\":%s}",
           t, h, p);
}

static void http_reply(uint8_t link, const char *status, const char *json)
{
  static char resp[384];
  int content_length = (int)strlen(json);            /* from the ACTUAL JSON string */
  int len = snprintf(resp, sizeof(resp),
                     "HTTP/1.1 %s\r\n"
                     "Content-Type: application/json\r\n"
                     "Content-Length: %d\r\n"
                     "Access-Control-Allow-Origin: *\r\n"
                     "Connection: close\r\n"
                     "\r\n"
                     "%s",
                     status, content_length, json);
  if (len >= (int)sizeof(resp))
    len = sizeof(resp) - 1;
  esp_at_send(link, resp, (uint16_t)len);
  esp_at_close(link);
}

static void handle_request(uint8_t link, const char *path)
{
  char json[160];
  char msg[80];

  snprintf(msg, sizeof(msg), "[HTTP] link %u GET %s\r\n", link, path);
  dbg(msg);

  if (strcmp(path, "/") == 0 || strcmp(path, "/api/bme280") == 0)
  {
    build_json(json, sizeof(json));
    http_reply(link, "200 OK", json);
    requests_served++;
  }
  else
  {
    http_reply(link, "404 Not Found", "{\"status\":\"error\",\"message\":\"not found\"}");
  }
}

static void server_start(void)
{
  char msg[64];
  esp_at_get_ip(ip_addr, sizeof(ip_addr));
  if (esp_at_start_server(HTTP_PORT) == ESP_OK)
  {
    snprintf(msg, sizeof(msg), "Open http://%s/ in a browser / Node-RED\r\n", ip_addr);
    dbg(msg);
  }
  else
  {
    dbg("AT+CIPSERVER failed\r\n");
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
  MX_USART1_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  char line[ESP_LINE_MAX];
  
  dbg("\r\nCAT12B - BME280 + ESP8266 HTTP JSON server\r\n");
  
  /* 1. Sensor */
  bme_status = bme_setup(BME280_OVERSAMPLING_2X, BME280_FILTER_COEFF_OFF, BME280_STANDBY_TIME_62_5_MS);
  if (bme_status != BME280_OK)
  {
    dbg("BME280 init failed (chip ID / wiring)\r\n");
    Error_Handler();
  }
  HAL_Delay(100);
  sensor_ok = (uint8_t)bme_read();
  print_readings();
  
  /* 2. Wi-Fi + server */
  esp_at_init(&huart1, on_esp_event);
  if (esp_at_startup() != ESP_OK)
  {
    dbg("ESP8266 not responding\r\n");
    Error_Handler();
  }
  dbg("Joining " WIFI_SSID " ...\r\n");
  if (esp_at_join_ap(WIFI_SSID, WIFI_PASSWORD) != ESP_OK)
  {
    dbg("Wi-Fi join failed - edit Core/Inc/wifi_config.h\r\n");
    Error_Handler();
  }
  dbg("Wi-Fi connected\r\n");
  server_start();
  last_read_ms = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ESP events (requests are collected by on_esp_event) */
    while (esp_at_read_line(line, sizeof(line), 0) >= 0) { }

    for (uint8_t link = 0; link < MAX_LINKS; link++)
    {
      if (requests[link].pending)
      {
        requests[link].pending = 0;
        handle_request(link, requests[link].path);
      }
    }

    /* fresh sensor data every 2 s, without blocking the server */
    if ((HAL_GetTick() - last_read_ms) >= SENSOR_PERIOD_MS)
    {
      last_read_ms = HAL_GetTick();
      sensor_ok = (uint8_t)bme_read();
      dbg("\r\n");
      print_readings();
    }

    if (wifi_lost)
    {
      dbg("Wi-Fi lost - waiting for the ESP8266 to reconnect...\r\n");
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* ---------------------------------------------------------------------------
 * Hardware abstraction for the Bosch BME280 Sensor API.
 * The Bosch driver never touches the hardware itself: it calls these three
 * functions through the bme280_dev structure.
 * ------------------------------------------------------------------------- */

/* read_register(): burst-read 'len' bytes starting at 'reg_addr' */
int8_t user_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  uint8_t addr7 = *(uint8_t *)intf_ptr;
  /* HAL wants the 8-bit (shifted) address */
  if (HAL_I2C_Mem_Read(&hi2c1, (uint16_t)(addr7 << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
                       reg_data, (uint16_t)len, 100) != HAL_OK)
    return BME280_E_COMM_FAIL;
  return BME280_OK;
}

/* write_register(): write 'len' bytes starting at 'reg_addr' */
int8_t user_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
  uint8_t addr7 = *(uint8_t *)intf_ptr;
  if (HAL_I2C_Mem_Write(&hi2c1, (uint16_t)(addr7 << 1), reg_addr, I2C_MEMADD_SIZE_8BIT,
                        (uint8_t *)reg_data, (uint16_t)len, 100) != HAL_OK)
    return BME280_E_COMM_FAIL;
  return BME280_OK;
}

/* delay_function(): the API asks for microseconds, HAL only has ms -> round up */
void user_delay_us(uint32_t period, void *intf_ptr)
{
  (void)intf_ptr;
  HAL_Delay((period + 999U) / 1000U);
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
