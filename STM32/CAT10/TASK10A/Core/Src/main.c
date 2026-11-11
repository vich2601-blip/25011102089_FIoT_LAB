/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : CAT 10 - Task A : STM32 <-> Wemos (ESP8266) UART link test with the AT command
  *                    USART1 = Wemos, USART2 = PC terminal. Ends in a PC<->ESP bridge mode.
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
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef enum { AT_OK = 0, AT_ERROR, AT_TIMEOUT } at_status_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ESP_UART        (&huart1)    /* USART1 PA9/PA10  <-> Wemos D1 mini (AT firmware) */
#define PC_UART         (&huart2)    /* USART2 PA2/PA3   <-> USB-TTL -> PuTTY            */
#define AT_LINE_MAX     128U
#define AT_RESP_MAX     1024U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

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
/* ---------------- PC terminal (USART2) ---------------- */
static void pc_print(const char *s)
{
  HAL_UART_Transmit(PC_UART, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

static void pc_println(const char *s)
{
  pc_print(s);
  pc_print("\r\n");
}

/* ---------------- ESP8266 (USART1), polling with timeouts ---------------- */
/* Read one text line (without CR/LF). Returns its length, or -1 on timeout. */
static int esp_read_line(char *buf, uint16_t size, uint32_t timeout_ms)
{
  uint16_t len = 0;
  uint8_t  c;
  uint32_t start = HAL_GetTick();

  while ((HAL_GetTick() - start) < timeout_ms)
  {
    if (HAL_UART_Receive(ESP_UART, &c, 1, 1) != HAL_OK)
      continue;
    if (c == '\r' || c == '\n')
    {
      if (len > 0)
      {
        buf[len] = '\0';
        return len;
      }
    }
    else if (len < size - 1U)
    {
      buf[len++] = (char)c;
    }
  }
  buf[len] = '\0';
  return -1;
}

/* Send one AT command and collect every reply line until OK / ERROR / FAIL.
 * The lines are stored first and printed later, so no byte from the ESP is
 * lost while the (slower) PC terminal is being written.                 */
static at_status_t at_command(const char *cmd, char *resp, uint16_t resp_size, uint32_t timeout_ms)
{
  char     line[AT_LINE_MAX];
  uint32_t start = HAL_GetTick();

  resp[0] = '\0';
  __HAL_UART_CLEAR_OREFLAG(ESP_UART);
  HAL_UART_Transmit(ESP_UART, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
  HAL_UART_Transmit(ESP_UART, (uint8_t *)"\r\n", 2, 1000);

  while ((HAL_GetTick() - start) < timeout_ms)
  {
    if (esp_read_line(line, sizeof(line), timeout_ms - (HAL_GetTick() - start)) < 0)
      break;
    if (strcmp(line, cmd) == 0)
      continue;                                   /* echo of our own command */
    if ((strlen(resp) + strlen(line) + 2U) < resp_size)
    {
      strcat(resp, line);
      strcat(resp, "\n");
    }
    if (strcmp(line, "OK") == 0)                       return AT_OK;
    if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0) return AT_ERROR;
  }
  return AT_TIMEOUT;
}

/* Print ">> command", run it, then print every reply line as "<< line" */
static at_status_t at_show(const char *cmd, uint32_t timeout_ms)
{
  static char resp[AT_RESP_MAX];
  at_status_t st;
  char *line;

  pc_print(">> ");
  pc_println(cmd);
  st = at_command(cmd, resp, sizeof(resp), timeout_ms);
  for (line = strtok(resp, "\n"); line != NULL; line = strtok(NULL, "\n"))
  {
    pc_print("<< ");
    pc_println(line);
  }
  if (st == AT_TIMEOUT)
    pc_println("<< (no reply - check wiring TX/RX crossed, 5 V, common GND)");
  return st;
}

/* Power-up: wait for the ESP to boot, check it answers, switch echo off */
static at_status_t esp_startup(void)
{
  static char resp[64];
  at_status_t st = AT_TIMEOUT;

  HAL_Delay(2000);
  __HAL_UART_CLEAR_OREFLAG(ESP_UART);
  for (int i = 0; i < 3 && st != AT_OK; i++)
    st = at_command("AT", resp, sizeof(resp), 1000);
  at_command("ATE0", resp, sizeof(resp), 1000);
  return st;
}

/* ---------------------------------------------------------------------------
 * Transparent bridge PuTTY <-> ESP8266, interrupt driven.
 * Each UART receives into its own ring buffer from its RX interrupt, so a
 * byte arriving on one side is never lost while the other side is being
 * written. The main loop forwards whatever has arrived in batches.
 * Enter (CR) from PuTTY is sent to the ESP as CR LF. Overflows are counted.
 * ------------------------------------------------------------------------- */
#define BRIDGE_RING   256U                       /* power of two; one slot stays free -> 255 usable bytes */
typedef struct
{
  volatile uint8_t  buf[BRIDGE_RING];
  volatile uint16_t head, tail;
  volatile uint32_t lost;                        /* bytes dropped: ring full            */
  volatile uint8_t  armed;                       /* RX interrupt currently armed        */
  uint8_t           rx;                          /* byte the interrupt receives into */
} bridge_ring_t;

static bridge_ring_t from_pc, from_esp;
static volatile uint8_t bridge_on = 0;         /* read by the UART callbacks */
static uint32_t tx_failures = 0;               /* HAL_UART_Transmit errors/timeouts */

static void ring_push(bridge_ring_t *r, uint8_t c)
{
  uint16_t next = (uint16_t)((r->head + 1U) & (BRIDGE_RING - 1U));
  if (next != r->tail) { r->buf[r->head] = c; r->head = next; }
  else r->lost++;
}

static uint16_t ring_take(bridge_ring_t *r, uint8_t *out, uint16_t max)
{
  uint16_t n = 0;
  while (n < max && r->tail != r->head)
  {
    out[n++] = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1U) & (BRIDGE_RING - 1U));
  }
  return n;
}

/* (Re)arm one-byte interrupt reception; remembers whether it succeeded so the
   main loop can retry if HAL refused (e.g. busy after an error). */
static void bridge_arm(UART_HandleTypeDef *huart, bridge_ring_t *r)
{
  r->armed = (HAL_UART_Receive_IT(huart, &r->rx, 1) == HAL_OK);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (!bridge_on) return;
  if (huart == ESP_UART)     { ring_push(&from_esp, from_esp.rx); bridge_arm(ESP_UART, &from_esp); }
  else if (huart == PC_UART) { ring_push(&from_pc,  from_pc.rx);  bridge_arm(PC_UART,  &from_pc);  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (!bridge_on) return;
  __HAL_UART_CLEAR_OREFLAG(huart);
  if (huart == ESP_UART)     bridge_arm(ESP_UART, &from_esp);
  else if (huart == PC_UART) bridge_arm(PC_UART,  &from_pc);
}

/* ---- transmit side: one ring per UART, sent by interrupt (never blocks) ---- */
#define TX_RING       512U                       /* power of two; 511 usable bytes */
typedef struct
{
  uint8_t           buf[TX_RING];
  volatile uint16_t head, tail;                  /* head: main, tail: TX-complete ISR */
  volatile uint16_t inflight;                    /* bytes handed to HAL_UART_Transmit_IT */
  volatile uint8_t  busy;
  volatile uint32_t dropped;                     /* bytes not queued: TX ring full */
} tx_ring_t;

static tx_ring_t to_pc, to_esp;

static tx_ring_t *tx_ring_for(UART_HandleTypeDef *huart)
{
  return (huart == ESP_UART) ? &to_esp : &to_pc;
}

/* Start sending the next contiguous block, if idle. Called with interrupts
   masked from the main loop, or from the TX-complete interrupt. */
static void tx_kick(UART_HandleTypeDef *huart, tx_ring_t *r)
{
  if (r->busy || r->head == r->tail)
    return;
  uint16_t end = (r->head > r->tail) ? r->head : TX_RING;    /* up to head, or to the wrap point */
  uint16_t len = (uint16_t)(end - r->tail);
  r->inflight = len;
  r->busy = 1;
  if (HAL_UART_Transmit_IT(huart, &r->buf[r->tail], len) != HAL_OK)
  {
    r->busy = 0;                                 /* retried on the next bridge_send / poll */
    tx_failures++;
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if (!bridge_on) return;
  tx_ring_t *r = tx_ring_for(huart);
  r->tail = (uint16_t)((r->tail + r->inflight) & (TX_RING - 1U));
  r->busy = 0;
  tx_kick(huart, r);
}

/* Queue bytes for sending; never waits. Bytes that do not fit are counted. */
static void bridge_send(UART_HandleTypeDef *huart, const uint8_t *d, uint16_t n)
{
  tx_ring_t *r = tx_ring_for(huart);
  for (uint16_t i = 0; i < n; i++)
  {
    uint16_t next = (uint16_t)((r->head + 1U) & (TX_RING - 1U));
    if (next == r->tail) { r->dropped += (uint32_t)(n - i); break; }
    r->buf[r->head] = d[i];
    r->head = next;
  }
  __disable_irq();
  tx_kick(huart, r);
  __enable_irq();
}

static void at_bridge_start(void)
{
  memset((void *)&from_pc, 0, sizeof(from_pc));
  memset((void *)&from_esp, 0, sizeof(from_esp));
  memset((void *)&to_pc, 0, sizeof(to_pc));
  memset((void *)&to_esp, 0, sizeof(to_esp));
  bridge_on = 1;
  __HAL_UART_CLEAR_OREFLAG(ESP_UART);
  bridge_arm(ESP_UART, &from_esp);
  bridge_arm(PC_UART,  &from_pc);
}

static void at_bridge_poll(void)
{
  static uint32_t reported = 0;

  /* re-arm reception if an earlier attempt was refused */
  if (!from_esp.armed) bridge_arm(ESP_UART, &from_esp);
  if (!from_pc.armed)  bridge_arm(PC_UART,  &from_pc);
  /* restart transmission if an earlier start was refused */
  __disable_irq();
  tx_kick(PC_UART, &to_pc);
  tx_kick(ESP_UART, &to_esp);
  __enable_irq();
  uint8_t chunk[64];
  uint8_t out[2 * sizeof(chunk)];
  uint16_t n, i, m;

  /* ESP8266 -> PC, as much as has arrived (up to 64 bytes per call) */
  n = ring_take(&from_esp, chunk, sizeof(chunk));
  if (n) bridge_send(PC_UART, chunk, n);

  /* PC -> ESP8266 (CR becomes CR LF) and local echo */
  n = ring_take(&from_pc, chunk, sizeof(chunk));
  for (i = 0, m = 0; i < n; i++)
  {
    out[m++] = chunk[i];
    if (chunk[i] == '\r') out[m++] = '\n';
  }
  if (m)
  {
    bridge_send(PC_UART, out, m);
    bridge_send(ESP_UART, out, m);
  }

  uint32_t total = from_pc.lost + from_esp.lost + to_pc.dropped + to_esp.dropped + tx_failures;
  if (total != reported)
  {
    char msg[128];
    reported = total;
    int len = snprintf(msg, sizeof(msg),
                       "\r\n[bridge] RX lost PC %lu / ESP %lu, TX queue full PC %lu / ESP %lu, TX start failures %lu\r\n",
                       (unsigned long)from_pc.lost, (unsigned long)from_esp.lost,
                       (unsigned long)to_pc.dropped, (unsigned long)to_esp.dropped, (unsigned long)tx_failures);
    if (len > 0)
      bridge_send(PC_UART, (const uint8_t *)msg, (uint16_t)(len < (int)sizeof(msg) ? len : (int)sizeof(msg) - 1));
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
  pc_println("");
  pc_println("STM32-WiFi MODEM TEST  (CAT10A)");
  pc_println("USART1 <-> Wemos, USART2 <-> PC, both 115200-8-N-1");
  
  if (esp_startup() == AT_OK)
    pc_println("ESP8266 answered, echo off");
  at_show("AT", 1000);
  
  pc_println("");
  pc_println("Bridge mode: type AT commands in this terminal.");
  at_bridge_start();                /* interrupt-driven from here on */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    at_bridge_poll();
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
