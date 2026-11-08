/**
  ******************************************************************************
  * @file    esp_at.h
  * @brief   Small interrupt-driven driver for an ESP8266 (Wemos D1 Mini) running
  *          the Espressif AT firmware. Works on STM32F1 and STM32F4 (HAL).
  *
  *          - Received bytes are stored by the UART RX interrupt in a ring
  *            buffer, so nothing is lost while the CPU is busy.
  *          - Every wait has a timeout (no HAL_MAX_DELAY hangs).
  *          - Unsolicited lines ("+IPD,...", "0,CONNECT", "0,CLOSED",
  *            "WIFI ...") are passed to a callback instead of being mixed
  *            into command responses.
  ******************************************************************************
  */
#ifndef ESP_AT_H
#define ESP_AT_H

#include "main.h"
#include <stdint.h>

#define ESP_LINE_MAX      128U    /* longest line kept (longer lines are cut) */
#define ESP_RX_RING_SIZE  512U    /* must be a power of two                   */

typedef enum { ESP_OK = 0, ESP_ERROR, ESP_TIMEOUT } esp_result_t;

/* Called for every unsolicited line (URC) the ESP8266 sends */
typedef void (*esp_urc_handler_t)(const char *line);

void         esp_at_init(UART_HandleTypeDef *huart, esp_urc_handler_t urc_handler);
void         esp_at_rx_cplt_callback(UART_HandleTypeDef *huart);   /* call from HAL_UART_RxCpltCallback */
void         esp_at_error_callback(UART_HandleTypeDef *huart);     /* call from HAL_UART_ErrorCallback  */

esp_result_t esp_at_startup(void);                                  /* AT handshake + ATE0              */
esp_result_t esp_at_cmd(const char *cmd, char *resp, uint16_t resp_size, uint32_t timeout_ms);
int          esp_at_read_line(char *out, uint16_t size, uint32_t timeout_ms); /* -1 = timeout       */
void         esp_at_flush(void);

esp_result_t esp_at_join_ap(const char *ssid, const char *password);
int          esp_at_get_ip(char *ip, uint16_t size);
int          esp_at_get_mac(char *mac, uint16_t size);
esp_result_t esp_at_start_server(uint16_t port);
int          esp_at_send(uint8_t link, const char *data, uint16_t len);       /* 1 = SEND OK         */
esp_result_t esp_at_close(uint8_t link);
uint32_t     esp_at_overflow_count(void);

#endif /* ESP_AT_H */
