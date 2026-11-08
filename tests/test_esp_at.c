/*
 * Host-side test of Common_Libraries/ESP8266_AT_Driver/esp_at.c against a
 * simulated ESP8266 (used by CAT 10D and CAT 12B).
 */
#include <stdio.h>
#include <string.h>
#include "esp_at.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t now_ms;
static void deliver_all(void);
uint32_t HAL_GetTick(void) { deliver_all(); return now_ms += 1; }   /* time passing lets queued bytes arrive */
void HAL_Delay(uint32_t ms) { now_ms += ms; }

static UART_HandleTypeDef huart1;
static uint8_t *rx_target; static int rx_armed;
static int fail_arm;                    /* fault injection: refuse this many re-arms */
static uint8_t pending[4096]; static size_t pend_head, pend_tail;

HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) { (void)h; rx_armed = 0; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{ (void)h; (void)n; if (fail_arm > 0) { fail_arm--; return HAL_BUSY; } if (rx_armed) return HAL_BUSY; rx_target = d; rx_armed = 1; return HAL_OK; }
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h) { esp_at_rx_cplt_callback(h); }

static void deliver_all(void)
{
  while (pend_tail != pend_head && rx_armed) { rx_armed = 0; *rx_target = pending[pend_tail++]; HAL_UART_RxCpltCallback(&huart1); }
  if (pend_tail == pend_head) pend_tail = pend_head = 0;
}
static void esp_says(const char *s) { memcpy(&pending[pend_head], s, strlen(s)); pend_head += strlen(s); }

/* fake ESP8266 */
static char txline[256]; static size_t txline_len; static size_t expect_raw; static int skip_lf;
static int cipsend_ok_first = 1, urc_during_prompt = 0, closed_during_prompt = 0, overflow_during_prompt = 0;
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t)
{
  (void)h; (void)t;
  for (uint16_t i = 0; i < n; i++)
  {
    if (skip_lf && d[i] == '\n') { skip_lf = 0; continue; }
    skip_lf = 0;
    if (expect_raw > 0) { if (--expect_raw == 0) esp_says("\r\nRecv bytes\r\n\r\nSEND OK\r\n"); continue; }
    if (d[i] != '\r') { if (txline_len < sizeof txline - 1) txline[txline_len++] = (char)d[i]; continue; }
    txline[txline_len] = '\0'; txline_len = 0; skip_lf = 1;
    unsigned link, len;
    if (sscanf(txline, "AT+CIPSEND=%u,%u", &link, &len) == 2)
    {
      if (closed_during_prompt) { esp_says("0,CLOSED\r\n"); continue; }       /* link dropped: no prompt */
      expect_raw = len;
      if (urc_during_prompt) esp_says("1,CONNECT\r\n");
      if (overflow_during_prompt) { static char junk[700]; memset(junk, 'j', sizeof junk - 1); esp_says(junk); expect_raw = 0; }
      esp_says(cipsend_ok_first ? "\r\nOK\r\n> " : "> ");
    }
    else esp_says("\r\nOK\r\n");
  }
  deliver_all();
  return HAL_OK;
}

static char urc_seen[128];
static void on_urc(const char *line) { strncpy(urc_seen, line, sizeof urc_seen - 1); }

int main(void)
{
  char line[ESP_LINE_MAX];
  esp_at_init(&huart1, on_urc);
  CHECK(esp_at_startup() == ESP_OK);

  /* both CIPSEND reply styles, no timeout wait */
  for (cipsend_ok_first = 1; cipsend_ok_first >= 0; cipsend_ok_first--)
  {
    uint32_t t0 = now_ms;
    CHECK(esp_at_send(0, "hello", 5) == 1);
    CHECK(now_ms - t0 < 200);
  }
  cipsend_ok_first = 1;

  /* an unsolicited line arriving during the prompt wait reaches the callback */
  urc_during_prompt = 1; urc_seen[0] = '\0';
  CHECK(esp_at_send(0, "x", 1) == 1);
  CHECK(strcmp(urc_seen, "1,CONNECT") == 0);
  urc_during_prompt = 0;

  /* a line longer than the buffer is discarded (not returned cut short) and counted */
  {
    char longl[300]; memset(longl, 'a', 200); strcpy(&longl[200], "\r\nOK\r\n");
    uint32_t t0 = esp_at_truncated_lines();
    esp_says(longl); deliver_all();
    CHECK(esp_at_read_line(line, sizeof line, 10) == 2 && strcmp(line, "OK") == 0);   /* next real line */
    CHECK(esp_at_truncated_lines() == t0 + 1);
  }
  /* an over-long line starting with "ERROR" is not mistaken for ERROR */
  {
    char longe[300]; strcpy(longe, "ERROR"); memset(&longe[5], 'x', 195); strcpy(&longe[200], "\r\n");
    esp_says(longe); deliver_all();
    CHECK(esp_at_read_line(line, sizeof line, 10) == -1);      /* nothing valid to read */
  }

  /* the link closes while we wait for the prompt: fail fast, callback still sees it */
  closed_during_prompt = 1; urc_seen[0] = '\0';
  {
    uint32_t t0 = now_ms;
    CHECK(esp_at_send(0, "x", 1) == 0);
    CHECK(now_ms - t0 < 200 && strcmp(urc_seen, "0,CLOSED") == 0);
  }
  closed_during_prompt = 0;

  /* RX overflow during CIPSEND: the send fails instead of trusting a broken stream */
  overflow_during_prompt = 1;
  CHECK(esp_at_send(0, "x", 1) == 0);
  CHECK(esp_at_overflow_count() > 0);
  overflow_during_prompt = 0;
  /* ... and after resynchronising, a normal command works again */
  CHECK(esp_at_cmd("AT", NULL, 0, 1000) == ESP_OK);
  CHECK(esp_at_send(0, "hello", 5) == 1);

  /* argument checks */
  char ssid[120]; memset(ssid, 's', 119); ssid[119] = '\0';
  CHECK(esp_at_join_ap(ssid, "pw") == ESP_ERROR);              /* would not fit the command buffer */
  CHECK(esp_at_read_line(line, 0, 10) == -1);
  CHECK(esp_at_send(0, NULL, 5) == 0 && esp_at_send(0, "x", 0) == 0);
  CHECK(esp_at_tx_errors() == 0);

  /* fault injection: HAL refuses one RX re-arm; the driver notices and retries */
  fail_arm = 1;
  esp_says("noise\r\n"); deliver_all();                       /* callback re-arm fails here */
  CHECK(esp_at_cmd("AT", NULL, 0, 1000) == ESP_OK);            /* reception restarted, reply seen */
  CHECK(esp_at_rearm_failures() == 1);

  /* caller's buffer too small: full length returned so the cut is visible */
  {
    char small[4];
    esp_says("OK123456\r\n"); deliver_all();
    CHECK(esp_at_read_line(small, sizeof small, 10) == 8 && strcmp(small, "OK1") == 0);
  }
  CHECK(esp_at_cmd(NULL, NULL, 0, 10) == ESP_ERROR);

  /* re-initialising resets state and counters */
  esp_at_init(&huart1, on_urc);
  CHECK(esp_at_overflow_count() == 0 && esp_at_truncated_lines() == 0 && esp_at_rearm_failures() == 0);
  CHECK(esp_at_cmd("AT", NULL, 0, 1000) == ESP_OK);

  printf("esp_at: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
