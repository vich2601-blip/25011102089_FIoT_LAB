/**
  ******************************************************************************
  * @file    esp_at.c
  * @brief   Interrupt-driven ESP8266 AT-command driver (see esp_at.h)
  ******************************************************************************
  */
#include "esp_at.h"
#include <string.h>
#include <stdio.h>

#define RING_MASK   (ESP_RX_RING_SIZE - 1U)

static UART_HandleTypeDef *esp_uart = NULL;
static esp_urc_handler_t   urc_cb   = NULL;

/* RX ring buffer: head is written by the ISR, tail is read by the application */
static volatile uint8_t  ring[ESP_RX_RING_SIZE];
static volatile uint16_t ring_head = 0;
static volatile uint16_t ring_tail = 0;
static volatile uint32_t ring_overflows = 0;
static uint8_t           rx_byte;

/* partial line being assembled from the ring buffer */
static char     line_acc[ESP_LINE_MAX];
static uint16_t line_len = 0;

/* ------------------------------------------------------------------ */
/*  Interrupt side                                                     */
/* ------------------------------------------------------------------ */
void esp_at_rx_cplt_callback(UART_HandleTypeDef *huart)
{
  if (huart != esp_uart)
    return;

  uint16_t next = (uint16_t)((ring_head + 1U) & RING_MASK);
  if (next != ring_tail)
  {
    ring[ring_head] = rx_byte;
    ring_head = next;
  }
  else
  {
    ring_overflows++;                       /* buffer full: byte dropped */
  }
  HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);  /* re-arm for the next byte */
}

void esp_at_error_callback(UART_HandleTypeDef *huart)
{
  if (huart != esp_uart)
    return;
  __HAL_UART_CLEAR_OREFLAG(huart);          /* overrun: clear and keep going */
  HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);
}

/* ------------------------------------------------------------------ */
/*  Application side                                                   */
/* ------------------------------------------------------------------ */
static int ring_pop(uint8_t *c)
{
  if (ring_head == ring_tail)
    return 0;
  *c = ring[ring_tail];
  ring_tail = (uint16_t)((ring_tail + 1U) & RING_MASK);
  return 1;
}

static uint32_t truncated_lines = 0;
static uint32_t tx_errors = 0;
static uint8_t  line_cut = 0;          /* current line exceeded the buffer */
static uint8_t  skip_to_eol = 0;       /* after an overflow: ignore input up to the next line break */

static int uart_write(const char *s, uint16_t len)
{
  if (HAL_UART_Transmit(esp_uart, (uint8_t *)s, len, 1000) != HAL_OK)
  {
    tx_errors++;
    return 0;
  }
  return 1;
}

/* After a ring-buffer overflow, bytes are missing somewhere in what is buffered.
   Discard the buffered data and the partial line, then ignore input until the
   next line break so parsing restarts on a clean boundary.
   Returns 1 if an overflow was handled. */
static int resync_after_overflow(void)
{
  static uint32_t seen = 0;
  if (ring_overflows == seen)
    return 0;
  seen = ring_overflows;
  ring_tail = ring_head;
  line_len = 0;
  line_cut = 0;
  skip_to_eol = 1;
  return 1;
}

void esp_at_init(UART_HandleTypeDef *huart, esp_urc_handler_t urc_handler)
{
  esp_uart  = huart;
  urc_cb    = urc_handler;
  ring_head = ring_tail = 0;
  line_len  = 0;
  HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);
}

void esp_at_flush(void)
{
  uint8_t c;
  while (ring_pop(&c)) { }
  line_len = 0;
}

uint32_t esp_at_overflow_count(void)  { return ring_overflows; }
uint32_t esp_at_truncated_lines(void) { return truncated_lines; }
uint32_t esp_at_tx_errors(void)       { return tx_errors; }

/* Move bytes from the ring into line_acc; returns 1 when a full line is ready */
/* Add one received character to the line being assembled.
   Returns 1 when a complete line is in line_acc (NUL-terminated). */
static int line_feed(uint8_t c)
{
  if (skip_to_eol)                    /* resynchronising after an overflow */
  {
    if (c == '\r' || c == '\n')
      skip_to_eol = 0;
    return 0;
  }
  if (c == '\r' || c == '\n')
  {
    if (line_len == 0U)
      return 0;
    line_acc[line_len] = '\0';
    line_len = 0;
    if (line_cut)                     /* too long: never hand a cut line on as a valid one */
    {
      truncated_lines++;
      line_cut = 0;
      return 0;
    }
    return 1;
  }
  if (line_len < ESP_LINE_MAX - 1U)
    line_acc[line_len++] = (char)c;
  else
    line_cut = 1;                     /* too long: rest of the line is dropped and counted */
  return 0;
}

static int poll_line(char *out, uint16_t size)
{
  uint8_t c;
  if (size == 0U)
    return 0;
  resync_after_overflow();
  while (ring_pop(&c))
  {
    if (line_feed(c))
    {
      strncpy(out, line_acc, size - 1U);
      out[size - 1U] = '\0';
      return 1;
    }
  }
  return 0;
}

/* Lines that the ESP8266 sends on its own, not as a reply to a command */
static int is_unsolicited(const char *l)
{
  if (strncmp(l, "+IPD,", 5) == 0)                 return 1;  /* incoming TCP data   */
  if (l[0] >= '0' && l[0] <= '4' && l[1] == ',')   return 1;  /* n,CONNECT / n,CLOSED */
  if (strncmp(l, "WIFI ", 5) == 0)                 return 1;  /* WIFI CONNECTED/GOT IP/DISCONNECT */
  return 0;
}

int esp_at_read_line(char *out, uint16_t size, uint32_t timeout_ms)
{
  if (out == NULL || size == 0U)
    return -1;
  uint32_t start = HAL_GetTick();
  do
  {
    while (poll_line(out, size))
    {
      if (is_unsolicited(out) && urc_cb != NULL)
      {
        urc_cb(out);          /* hand it to the application, keep waiting */
        continue;
      }
      return (int)strlen(out);
    }
  } while ((HAL_GetTick() - start) < timeout_ms);
  return -1;
}

esp_result_t esp_at_cmd(const char *cmd, char *resp, uint16_t resp_size, uint32_t timeout_ms)
{
  char     line[ESP_LINE_MAX];
  uint32_t start = HAL_GetTick();

  if (resp != NULL && resp_size > 0)
    resp[0] = '\0';

  if (!uart_write(cmd, (uint16_t)strlen(cmd)) || !uart_write("\r\n", 2))
    return ESP_ERROR;

  while ((HAL_GetTick() - start) < timeout_ms)
  {
    uint32_t left = timeout_ms - (HAL_GetTick() - start);
    if (esp_at_read_line(line, sizeof(line), left) < 0)
      break;

    if (strcmp(line, "OK") == 0)
      return ESP_OK;
    if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0)
      return ESP_ERROR;

    if (resp != NULL && (strlen(resp) + strlen(line) + 2U) < resp_size)
    {
      strcat(resp, line);       /* keep the information lines */
      strcat(resp, "\n");
    }
  }
  return ESP_TIMEOUT;
}

esp_result_t esp_at_startup(void)
{
  esp_result_t r = ESP_TIMEOUT;

  HAL_Delay(1500);                          /* ESP8266 boot messages */
  esp_at_flush();
  for (int i = 0; i < 5 && r != ESP_OK; i++)
    r = esp_at_cmd("AT", NULL, 0, 1000);    /* is the modem alive?  */
  if (r == ESP_OK)
    r = esp_at_cmd("ATE0", NULL, 0, 1000);  /* no command echo      */
  return r;
}

esp_result_t esp_at_join_ap(const char *ssid, const char *password)
{
  char cmd[100];
  if (esp_at_cmd("AT+CWMODE=1", NULL, 0, 2000) != ESP_OK)     /* station mode */
    return ESP_ERROR;
  if (snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password) >= (int)sizeof(cmd))
    return ESP_ERROR;                                          /* SSID/password too long */
  return esp_at_cmd(cmd, NULL, 0, 20000);                      /* joining can take ~10 s */
}

/* Copy the text between key and the next '"' (used for AT+CIFSR replies) */
static int extract_after(const char *text, const char *key, char *out, uint16_t size)
{
  if (out == NULL || size == 0U)
    return 0;
  const char *p = strstr(text, key);
  uint16_t n = 0;
  if (p == NULL)
    return 0;
  p += strlen(key);
  while (*p && *p != '"' && n < size - 1U)
    out[n++] = *p++;
  out[n] = '\0';
  return n > 0;
}

int esp_at_get_ip(char *ip, uint16_t size)
{
  char resp[256];
  if (esp_at_cmd("AT+CIFSR", resp, sizeof(resp), 3000) != ESP_OK)
    return 0;
  return extract_after(resp, "STAIP,\"", ip, size);
}

int esp_at_get_mac(char *mac, uint16_t size)
{
  char resp[256];
  if (esp_at_cmd("AT+CIFSR", resp, sizeof(resp), 3000) != ESP_OK)
    return 0;
  return extract_after(resp, "STAMAC,\"", mac, size);
}

esp_result_t esp_at_start_server(uint16_t port)
{
  char cmd[32];
  /* multiple connections; returns ERROR if a server is already running,
     which is fine, so the result is not checked */
  (void)esp_at_cmd("AT+CIPMUX=1", NULL, 0, 2000);
  snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%u", port);
  return esp_at_cmd(cmd, NULL, 0, 2000);
}

/* Wait for the CIPSEND '>' prompt. Works whether or not the firmware sends
   "OK" first. Other lines that arrive meanwhile are not thrown away:
   unsolicited ones (+IPD, n,CONNECT, ...) still go to the callback, and
   ERROR, busy, "<link>,CLOSED", Wi-Fi loss or an RX overflow end the wait
   with a failure. */
static int wait_for_prompt(uint8_t link, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint8_t  c;
  char     closed[12];
  snprintf(closed, sizeof(closed), "%u,CLOSED", link);
  resync_after_overflow();
  while ((HAL_GetTick() - start) < timeout_ms)
  {
    if (resync_after_overflow())
      return 0;                       /* data lost: the prompt state is unknown */
    if (!ring_pop(&c))
      continue;
    if (c == '>' && line_len == 0U)
      return 1;
    if (!line_feed(c))
      continue;
    if (strcmp(line_acc, "ERROR") == 0 || strcmp(line_acc, "link is not valid") == 0 ||
        strncmp(line_acc, "busy", 4) == 0)
      return 0;
    if (is_unsolicited(line_acc) && urc_cb != NULL)
      urc_cb(line_acc);
    if (strcmp(line_acc, closed) == 0 || strcmp(line_acc, "WIFI DISCONNECT") == 0)
      return 0;                       /* our link (or Wi-Fi) went away */
  }
  return 0;
}

/* AT+CIPSEND in two steps: command -> wait for '>' -> raw bytes -> SEND OK */
int esp_at_send(uint8_t link, const char *data, uint16_t len)
{
  char     cmd[32];
  char     line[ESP_LINE_MAX];
  uint32_t start;

  if (data == NULL || len == 0U || len > 2048U)
    return 0;
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u,%u", link, len);
  if (!uart_write(cmd, (uint16_t)strlen(cmd)) || !uart_write("\r\n", 2))
    return 0;
  if (!wait_for_prompt(link, 2000))     /* do not wait for "OK": some firmware sends only '>' */
    return 0;

  if (!uart_write(data, len))
    return 0;

  start = HAL_GetTick();
  while ((HAL_GetTick() - start) < 5000U)
  {
    if (esp_at_read_line(line, sizeof(line), 5000U - (HAL_GetTick() - start)) < 0)
      break;
    if (strcmp(line, "SEND OK") == 0)
      return 1;
    if (strcmp(line, "SEND FAIL") == 0 || strcmp(line, "ERROR") == 0)
      return 0;
  }
  return 0;
}

esp_result_t esp_at_close(uint8_t link)
{
  char cmd[24];
  snprintf(cmd, sizeof(cmd), "AT+CIPCLOSE=%u", link);
  return esp_at_cmd(cmd, NULL, 0, 2000);
}
