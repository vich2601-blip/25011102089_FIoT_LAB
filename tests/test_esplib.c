/*
 * Host-side test of ESPLib.c against a simulated ESP8266 + MQTT broker.
 * The fake ESP answers AT commands, wraps broker packets in "+IPD,<len>:",
 * and can inject a PUBLISH while the client is in the middle of a send -
 * the case that lost data in the original library (RX aborted during send).
 */
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "ESPLib.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ------------------------------------------------------------- fake time */
static uint32_t now_ms;
uint32_t HAL_GetTick(void) { return now_ms += 1; }      /* time advances while polling */
void HAL_Delay(uint32_t ms) { now_ms += ms; }

/* ------------------------------------------------------------- fake UART */
static UART_HandleTypeDef huart1;
static uint8_t *rx_target;
static int rx_armed;

/* bytes waiting to be "received" (delivered one by one through the RX interrupt) */
static uint8_t pending[4096];
static size_t pend_head, pend_tail;

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{
  (void)h; (void)n;
  if (rx_armed) return HAL_BUSY;
  rx_target = d; rx_armed = 1;
  return HAL_OK;
}

static void deliver_all(void)       /* simulate the RX interrupt firing for every queued byte */
{
  while (pend_tail != pend_head && rx_armed)
  {
    rx_armed = 0;
    *rx_target = pending[pend_tail++];
    HAL_UART_RxCpltCallback(&huart1);   /* library re-arms inside the callback */
  }
}

static void esp_says(const void *d, size_t n) { memcpy(&pending[pend_head], d, n); pend_head += n; }
static void esp_says_str(const char *s) { esp_says(s, strlen(s)); }
static void esp_ipd(const uint8_t *pkt, size_t n)
{
  char hdr[24]; snprintf(hdr, sizeof hdr, "\r\n+IPD,%u:", (unsigned)n);
  esp_says_str(hdr); esp_says(pkt, n);
}

/* ------------------------------------------------------------- fake ESP8266 */
static char     txline[256];
static size_t   txline_len;
static size_t   expect_raw;            /* bytes of raw data expected after CIPSEND */
static uint8_t  raw[512];
static size_t   raw_len;
static int      inject_publish_during_send;
static int      broker_refuses;
static int      broker_silent;         /* do not answer PINGREQ / UNSUBSCRIBE */
static uint8_t  suback_grant;          /* QoS the broker grants in SUBACK */
static int      cipsend_ok_first = 1;  /* 1: "OK" then ">" (Espressif), 0: only ">" (older firmware) */
static int      skip_lf;               /* the \n that ends an AT command line */

static void broker_handle(const uint8_t *p, size_t n)
{
  (void)n;
  uint8_t type = p[0] >> 4;
  if (type == 1) { uint8_t ack[] = { 0x20, 0x02, 0x00, (uint8_t)(broker_refuses ? 5 : 0) }; esp_ipd(ack, sizeof ack); }
  if (type == 8) { uint8_t ack[] = { 0x90, 0x03, p[2], p[3], suback_grant }; esp_ipd(ack, sizeof ack); }
  if (type == 10 && !broker_silent) { uint8_t ack[] = { 0xB0, 0x02, p[2], p[3] }; esp_ipd(ack, sizeof ack); }
  if (type == 12 && !broker_silent) { uint8_t ack[] = { 0xD0, 0x00 }; esp_ipd(ack, sizeof ack); }
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t)
{
  (void)h; (void)t;
  for (uint16_t i = 0; i < n; i++)
  {
    if (skip_lf && d[i] == '\n') { skip_lf = 0; continue; }
    skip_lf = 0;
    if (expect_raw > 0)
    {
      raw[raw_len++] = d[i];
      if (--expect_raw == 0)
      {
        if (inject_publish_during_send)            /* broker pushes a message just as we finish sending */
        {
          const uint8_t pub[] = { 0x30, 0x0B, 0x00, 0x05, 'a', '/', 'l', 'e', 'd', 'T', 'O', 'G', 'L' };
          esp_ipd(pub, sizeof pub);
        }
        esp_says_str("\r\nRecv bytes\r\n\r\nSEND OK\r\n");
        broker_handle(raw, raw_len);
      }
      continue;
    }
    if (d[i] == '\n') continue;
    if (d[i] != '\r') { if (txline_len < sizeof txline - 1) txline[txline_len++] = (char)d[i]; continue; }
    txline[txline_len] = '\0'; txline_len = 0; skip_lf = 1;
    unsigned len;
    if (sscanf(txline, "AT+CIPSEND=%u", &len) == 1) { expect_raw = len; raw_len = 0; esp_says_str(cipsend_ok_first ? "\r\nOK\r\n> " : "> "); }
    else if (strncmp(txline, "AT+CIPSTART", 11) == 0) esp_says_str("CONNECT\r\n\r\nOK\r\n");
    else if (strncmp(txline, "AT+CWJAP", 8) == 0) esp_says_str("WIFI CONNECTED\r\nWIFI GOT IP\r\n\r\nOK\r\n");
    else if (strcmp(txline, "AT+CIPCLOSE") == 0) esp_says_str("\r\nERROR\r\n");
    else esp_says_str("\r\nOK\r\n");
  }
  deliver_all();
  return HAL_OK;
}

/* ------------------------------------------------------------- tests */
int main(void)
{
  char topic[MQTT_TOPIC_MAX], msg[MQTT_MSG_MAX];
  MQTT_InitTypeDef_t cfg = { .clientID = "stm32-test", .keepAlive = 60, .connectFlag = CleanSession };

  CHECK(ESP8266_Init(StationMode, huart1) == funcOk);
  CHECK(ESP8266_wifiConnect("ssid", "pw") == funcOk);
  CHECK(ESP8266_portConnect("TCP", "broker", "1883") == funcOk);
  CHECK(MQTT_connectBroker(&cfg) == funcOk);                   /* CONNACK 0 received and checked */
  CHECK(MQTT_isConnected());

  CHECK(MQTT_subscribeTopic("a/led", QoS0) == funcOk);        /* SUBACK received and matched */
  CHECK(MQTTDataHandler(topic, msg) == 0);                     /* nothing yet */

  /* a PUBLISH arrives while we are sending: it must not be lost */
  inject_publish_during_send = 1;
  CHECK(MQTT_publishTopic("a/data", "{\"t\":28.4}", QoS0) == funcOk);
  inject_publish_during_send = 0;
  CHECK(MQTTDataHandler(topic, msg) == 1);
  CHECK(strcmp(topic, "a/led") == 0 && strcmp(msg, "TOGL") == 0);
  CHECK(MQTTDataHandler(topic, msg) == 0);                     /* delivered exactly once */

  /* the published bytes really were a valid MQTT PUBLISH */
  CHECK(raw[0] == 0x30 && raw[1] == 2 + 6 + 10 && memcmp(&raw[4], "a/data", 6) == 0);

  /* refused connection is reported as an error */
  broker_refuses = 1;
  CHECK(MQTT_connectBroker(&cfg) == funcErr);
  CHECK(!MQTT_isConnected());
  CHECK(MQTT_publishTopic("a/data", "x", QoS0) == funcErr);   /* not connected -> refuses to send */

  /* both CIPSEND reply styles work, and neither waits for a timeout */
  broker_refuses = 0;
  CHECK(MQTT_connectBroker(&cfg) == funcOk);
  for (cipsend_ok_first = 1; cipsend_ok_first >= 0; cipsend_ok_first--)
  {
    uint32_t t0 = now_ms;
    CHECK(MQTT_publishTopic("a/data", "x", QoS0) == funcOk);
    CHECK(now_ms - t0 < 200);                                   /* no hidden 2 s wait */
  }
  cipsend_ok_first = 1;

  /* PINGREQ: answered -> ok; broker silent -> error and marked disconnected */
  CHECK(MQTT_pingReq() == funcOk && MQTT_isConnected());
  broker_silent = 1;
  CHECK(MQTT_pingReq() == funcErr);
  CHECK(!MQTT_isConnected());
  broker_silent = 0;

  /* a topic longer than MQTT_TOPIC_MAX is dropped, not silently truncated */
  CHECK(MQTT_connectBroker(&cfg) == funcOk);
  {
    uint8_t pub[2 + 2 + 70 + 2];
    pub[0] = 0x30; pub[1] = 2 + 70 + 2; pub[2] = 0; pub[3] = 70;
    memset(&pub[4], 't', 70); pub[74] = 'h'; pub[75] = 'i';
    uint32_t dropped0 = MQTT_droppedMessages();
    esp_ipd(pub, sizeof pub); deliver_all();
    CHECK(MQTTDataHandler(topic, msg) == 0);
    CHECK(MQTT_droppedMessages() == dropped0 + 1);
  }

  /* QoS 0 only: QoS 1/2 requests are refused, not silently downgraded */
  CHECK(MQTT_publishTopic("a/data", "x", QoS1) == funcErr);
  CHECK(MQTT_subscribeTopic("a/led", QoS1) == funcErr);

  /* broker grants QoS 1 for a QoS 0 request -> treated as failure */
  suback_grant = 1;
  CHECK(MQTT_subscribeTopic("a/led", QoS0) == funcErr);
  suback_grant = 0;

  /* incoming QoS 1 PUBLISH (would need a PUBACK) is dropped and counted */
  {
    const uint8_t q1[] = { 0x32, 0x0A, 0x00, 0x05, 'a', '/', 'l', 'e', 'd', 0x00, 0x07, 'O', 'N' };
    uint8_t fixed[sizeof q1]; memcpy(fixed, q1, sizeof q1); fixed[1] = sizeof q1 - 2;
    uint32_t d0 = MQTT_droppedMessages();
    esp_ipd(fixed, sizeof fixed); deliver_all();
    CHECK(MQTTDataHandler(topic, msg) == 0 && MQTT_droppedMessages() == d0 + 1);
  }

  /* UNSUBSCRIBE waits for the matching UNSUBACK */
  CHECK(MQTT_unsubscribeTopic("a/led") == funcOk);
  broker_silent = 1;
  CHECK(MQTT_unsubscribeTopic("a/led") == funcErr);
  broker_silent = 0;

  CHECK(ESP8266_rxOverflows() == 0);
  printf("ESPLib: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
