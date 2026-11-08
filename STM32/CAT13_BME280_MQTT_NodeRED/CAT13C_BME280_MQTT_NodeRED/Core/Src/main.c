/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 13 - BME280 over I2C published as JSON over MQTT (broker.emqx.io)
  *                    Node-RED mqtt-in subscribes to vishrudh/stm32/bme280 and visualises T/H/P.
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
#include "ESPLib.h"
#include "bme280.h"
#include "wifi_config.h"
#include "fmt_fixed.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DBG_UART          (&huart6)          /* USART6 PC6/PC7 -> PuTTY 115200 */
#define MQTT_BROKER       "broker.emqx.io"   /* public test broker            */
#define MQTT_PORT         "1883"             /* plain (unencrypted) MQTT      */
#define DATA_TOPIC        "vishrudh/stm32/bme280"
#define RETRY_MS          5000U              /* wait before reconnecting      */
#define CLIENT_ID         "stm32-vishrudh-bme280"
#define PUBLISH_PERIOD_MS 2000U
/* BME280 on I2C1: PB6 = SCL, PB7 = SDA, 3.3 V, address 0x76 */
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
static uint32_t published = 0;
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

/* Print the library's error counters whenever they change */
static void report_diagnostics(void)
{
  static uint32_t last_ovf = 0, last_drop = 0;
  uint32_t ovf = ESP8266_rxOverflows(), drop = MQTT_droppedMessages();
  if (ovf != last_ovf || drop != last_drop)
  {
    char line[80];
    snprintf(line, sizeof(line), "[diag] UART RX overflow bytes: %lu, dropped MQTT packets: %lu\r\n",
             (unsigned long)ovf, (unsigned long)drop);
    dbg(line);
    last_ovf = ovf;
    last_drop = drop;
  }
}

/* Wi-Fi + TCP + MQTT CONNECT. Every step is checked; returns 1 only when the
 * broker has answered CONNACK "accepted". Optional broker login: define
 * MQTT_USERNAME / MQTT_PASSWORD in wifi_secrets.h. */
static int mqtt_start(const char *client_id)
{
  MQTT_InitTypeDef_t cfg;

  dbg("ESP8266 init (station mode)...\r\n");
  if (ESP8266_Init(StationMode, huart1) != funcOk) { dbg("  ESP8266 not responding\r\n"); return 0; }
  dbg("Joining " WIFI_SSID "...\r\n");
  if (ESP8266_wifiConnect(WIFI_SSID, WIFI_PASSWORD) != funcOk) { dbg("  Wi-Fi join failed\r\n"); return 0; }
  dbg("TCP connect to " MQTT_BROKER ":" MQTT_PORT "...\r\n");
  if (ESP8266_portConnect("TCP", MQTT_BROKER, MQTT_PORT) != funcOk) { dbg("  TCP connect failed\r\n"); return 0; }

  memset(&cfg, 0, sizeof(cfg));
  cfg.keepAlive   = 120;                 /* seconds; PINGREQ / publishes keep it alive */
  cfg.connectFlag = CleanSession;        /* no stored session */
  cfg.clientID    = (char *)client_id;
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
  cfg.username     = MQTT_USERNAME;
  cfg.password     = MQTT_PASSWORD;
  cfg.connectFlag |= UserNameFlag | PasswordFlag;
#endif
  if (MQTT_connectBroker(&cfg) != funcOk) { dbg("  broker refused / no CONNACK\r\n"); return 0; }
  dbg("MQTT connected (CONNACK accepted) as ");
  dbg(client_id);
  dbg("\r\n");
  return 1;
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

/* {"status":"success","temperature":28.45,"humidity":62.30,"pressure":1008.72} */
static void build_json(char *json, size_t size, int ok)
{
  char t[12], h[12], p[12];
  if (!ok)
  {
    snprintf(json, size, "{\"status\":\"error\"}");
    return;
  }
  fmt_2dp(t, sizeof(t), temperature);
  fmt_2dp(h, sizeof(h), humidity);
  fmt_2dp(p, sizeof(p), pressure);
  snprintf(json, size, "{\"status\":\"success\",\"temperature\":%s,\"humidity\":%s,\"pressure\":%s}", t, h, p);
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
  dbg("\r\nCAT13C - BME280 -> JSON -> MQTT -> Node-RED\r\n");
  
  bme_status = bme_setup(BME280_OVERSAMPLING_2X, BME280_FILTER_COEFF_OFF, BME280_STANDBY_TIME_62_5_MS);
  if (bme_status != BME280_OK)
  {
    dbg("BME280 init failed\r\n");
    Error_Handler();
  }
  dbg("BME280 ready\r\n");
  
  while (!mqtt_start(CLIENT_ID)) HAL_Delay(RETRY_MS);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    char json[128];
    int  ok = bme_read();

    build_json(json, sizeof(json), ok);
    report_diagnostics();
    if (!MQTT_isConnected())
    {
      dbg("Connection lost - reconnecting\r\n");
      while (!mqtt_start(CLIENT_ID)) HAL_Delay(RETRY_MS);
    }
    if (MQTT_publishTopic(DATA_TOPIC, json, QoS0) == funcOk)
      published++;
    else
      dbg("PUB failed\r\n");

    dbg("\r\nPUB " DATA_TOPIC " ");
    dbg(json);
    dbg("\r\n");
    if (ok)
      print_readings();

    HAL_Delay(PUBLISH_PERIOD_MS);
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
