/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 12 - Steps 1-3 : BME280 over I2C with the Bosch Sensor API, output on UART
  *                    Chip ID -> raw registers -> compensated T/P/H -> compare two oversampling profiles.
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
#include "fmt_fixed.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DBG_UART          (&huart6)   /* USART6: PC6 TX, PC7 RX -> USB-TTL -> PuTTY 115200 */
/* BME280 on I2C1: PB6 = SCL, PB7 = SDA, VCC = 3.3 V, SDO = GND -> address 0x76 */

#define READ_PERIOD_MS     2000U
#define READINGS_PER_PROFILE 10U    /* switch oversampling/filter profile every 10 readings */
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
static uint32_t reading_no = 0;
static uint8_t  profile    = 0;     /* 0 = A (2x, no filter), 1 = B (16x, filter 16) */
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

/* Step 1: read the chip ID register (0xD0) directly - should be 0x60 */
static uint8_t read_chip_id(void)
{
  uint8_t id = 0;
  if (user_i2c_read(BME280_REG_CHIP_ID, &id, 1, &bme_addr) != BME280_OK)
    return 0;
  return id;
}

/* Step 2: read the 8 raw measurement registers 0xF7..0xFE and unpack the ADC values */
static void print_raw(void)
{
  uint8_t r[BME280_LEN_P_T_H_DATA];
  char    line[64];
  if (bme280_get_regs(BME280_REG_DATA, r, sizeof(r), &bme) != BME280_OK)
    return;
  uint32_t adc_p = ((uint32_t)r[0] << 12) | ((uint32_t)r[1] << 4) | (r[2] >> 4);
  uint32_t adc_t = ((uint32_t)r[3] << 12) | ((uint32_t)r[4] << 4) | (r[5] >> 4);
  uint32_t adc_h = ((uint32_t)r[6] << 8)  |  r[7];
  snprintf(line, sizeof(line), "Raw ADC     : T=%lu P=%lu H=%lu\r\n",
           (unsigned long)adc_t, (unsigned long)adc_p, (unsigned long)adc_h);
  dbg(line);
}

/* Step 5: two settings profiles to compare the noise of the readings */
static void apply_profile(uint8_t which)
{
  if (which == 0)
  {
    bme_setup(BME280_OVERSAMPLING_2X, BME280_FILTER_COEFF_OFF, BME280_STANDBY_TIME_62_5_MS);
    dbg("\r\n>>> Profile A: oversampling 2x, IIR filter OFF\r\n");
  }
  else
  {
    bme_setup(BME280_OVERSAMPLING_16X, BME280_FILTER_COEFF_16, BME280_STANDBY_TIME_125_MS);
    dbg("\r\n>>> Profile B: oversampling 16x, IIR filter 16 (smoother, slower)\r\n");
  }
  HAL_Delay(200);                    /* let the first conversion finish */
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
  char msg[48];
  uint8_t id;
  
  dbg("\r\nCAT12A - BME280 over I2C with the Bosch Sensor API\r\n");
  
  id = read_chip_id();
  snprintf(msg, sizeof(msg), "Chip ID     : 0x%02X %s\r\n", id, (id == BME280_CHIP_ID) ? "(BME280 OK)" : "(WRONG / no sensor)");
  dbg(msg);
  if (id != BME280_CHIP_ID)
  {
    dbg("Check wiring: SCL=PB6, SDA=PB7, 3.3V, GND, address 0x76\r\n");
    Error_Handler();
  }
  
  apply_profile(profile);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    char hdr[48];

    reading_no++;
    snprintf(hdr, sizeof(hdr), "\r\n--- Reading %lu (profile %c) ---\r\n", (unsigned long)reading_no, profile ? 'B' : 'A');
    dbg(hdr);
    print_raw();
    if (bme_read())
      print_readings();
    else
      dbg("bme280_get_sensor_data() failed\r\n");

    if ((reading_no % READINGS_PER_PROFILE) == 0)
    {
      profile = !profile;
      apply_profile(profile);
    }
    HAL_Delay(READ_PERIOD_MS);
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

/* delay_function(): the API asks for microseconds.
 * >= 1 ms : HAL_Delay (SysTick, waits at least the requested time)
 *  < 1 ms : busy-wait on the Cortex-M4 DWT cycle counter (84 cycles = 1 us) */
void user_delay_us(uint32_t period, void *intf_ptr)
{
  (void)intf_ptr;
  if (period >= 1000U)
  {
    HAL_Delay((period + 999U) / 1000U);
    return;
  }
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)        /* enable the cycle counter once */
  {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = period * (SystemCoreClock / 1000000U);
  while ((DWT->CYCCNT - start) < ticks)                  /* unsigned maths: wrap-safe */
  {
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
