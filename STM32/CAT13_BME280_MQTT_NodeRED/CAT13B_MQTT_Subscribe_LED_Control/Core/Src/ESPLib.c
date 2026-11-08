/**
  ******************************************************************************
  * @file    ESPLib.c
  * @brief   ESP8266 (AT firmware) + MQTT 3.1.1 client - see ESPLib.h
  *
  *  Data path
  *    USART RX interrupt -> ring[] (1 KB)  -> esp_pump() in the main context
  *      text lines  : OK / ERROR / FAIL / SEND OK / SEND FAIL / CONNECT / CLOSED / WIFI ...
  *      '>'         : CIPSEND prompt
  *      +IPD,<n>:   : the next n bytes are TCP data -> mqtt_parser_feed()
  ******************************************************************************
  */
#include "ESPLib.h"
#include <stdio.h>

#define RING_SIZE      1024U                     /* power of two */
#define RING_MASK      (RING_SIZE - 1U)
#define LINE_MAX       128U
#define TX_PACKET_MAX  512U

#define T_CMD_MS       2000U
#define T_JOIN_MS      20000U
#define T_TCP_MS       15000U
#define T_SEND_MS      5000U
#define T_ACK_MS       5000U

uint8_t uartRxData;

static UART_HandleTypeDef *esp_uart = NULL;

/* ---- ring buffer (head: ISR, tail: main) ---- */
static volatile uint8_t  ring[RING_SIZE];
static volatile uint16_t ring_head, ring_tail;
static volatile uint32_t ring_overflows;

/* ---- text / +IPD stream state ---- */
static char     line[LINE_MAX];
static uint16_t line_len;
static uint32_t ipd_left;                      /* TCP bytes still expected from the current +IPD */

/* ---- response flags (set by esp_pump) ---- */
static volatile uint8_t got_ok, got_error, got_prompt, got_send_ok, got_send_fail, got_connect;
static volatile bool    tcp_connected, mqtt_connected;

/* ---- MQTT ---- */
static mqtt_parser_t parser;
static int16_t  connack_code = -1;             /* -1 = none received yet */
static int16_t  suback_code  = -1;
static uint16_t suback_id;
static uint16_t next_packet_id = 1;
static uint8_t  txbuf[TX_PACKET_MAX];

static bool     msg_pending;
static char     msg_topic[MQTT_TOPIC_MAX];
static char     msg_payload[MQTT_MSG_MAX];
static uint32_t msg_dropped;

/* ========================================================================== */
/*  Interrupt side                                                            */
/* ========================================================================== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart != esp_uart)
    return;
  uint16_t next = (uint16_t)((ring_head + 1U) & RING_MASK);
  if (next != ring_tail) { ring[ring_head] = uartRxData; ring_head = next; }
  else ring_overflows++;
  HAL_UART_Receive_IT(esp_uart, &uartRxData, 1);          /* stay armed */
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart != esp_uart)
    return;
  __HAL_UART_CLEAR_OREFLAG(huart);
  HAL_UART_Receive_IT(esp_uart, &uartRxData, 1);
}

/* ========================================================================== */
/*  Stream processing (main context)                                          */
/* ========================================================================== */
static void on_mqtt_event(mqtt_event_t ev)
{
  switch (ev)
  {
    case MQTT_EVT_CONNACK:
      connack_code = parser.code;
      mqtt_connected = (parser.code == 0U);
      break;
    case MQTT_EVT_SUBACK:
      suback_id = parser.id;
      suback_code = parser.code;
      break;
    case MQTT_EVT_PUBLISH:
      if (msg_pending || parser.code != 0U) { msg_dropped++; break; }  /* busy or truncated */
      strncpy(msg_topic, parser.topic, sizeof(msg_topic) - 1U);
      msg_topic[sizeof(msg_topic) - 1U] = '\0';
      {
        size_t n = parser.payload_len < sizeof(msg_payload) - 1U ? parser.payload_len : sizeof(msg_payload) - 1U;
        memcpy(msg_payload, parser.payload, n);
        msg_payload[n] = '\0';
      }
      msg_pending = true;
      break;
    case MQTT_EVT_ERROR:
      msg_dropped++;
      break;
    default:
      break;
  }
}

static void on_line(void)
{
  line[line_len] = '\0';
  if      (strcmp(line, "OK") == 0)                                   got_ok = 1;
  else if (strcmp(line, "ERROR") == 0 || strcmp(line, "FAIL") == 0)   got_error = 1;
  else if (strcmp(line, "SEND OK") == 0)                              got_send_ok = 1;
  else if (strcmp(line, "SEND FAIL") == 0)                            got_send_fail = 1;
  else if (strcmp(line, "ALREADY CONNECTED") == 0)                    { got_connect = 1; tcp_connected = true; }
  else if (strstr(line, "CLOSED") != NULL)                            { tcp_connected = false; mqtt_connected = false; }
  else if (strcmp(line, "WIFI DISCONNECT") == 0)                      { tcp_connected = false; mqtt_connected = false; }
  else if (strstr(line, "CONNECT") != NULL && strncmp(line, "WIFI", 4) != 0) { got_connect = 1; tcp_connected = true; }
}

/* "+IPD,<len>:" or "+IPD,<link>,<len>:" -> len (0 if malformed) */
static uint32_t ipd_length(void)
{
  const char *comma = strrchr(line, ',');
  uint32_t v = 0;
  if (comma == NULL) return 0;
  for (const char *c = comma + 1; *c; c++)
  {
    if (*c < '0' || *c > '9' || v > 100000U) return 0;
    v = v * 10U + (uint32_t)(*c - '0');
  }
  return v;
}

static void esp_pump(void)
{
  while (ring_tail != ring_head)
  {
    uint8_t c = ring[ring_tail];
    ring_tail = (uint16_t)((ring_tail + 1U) & RING_MASK);

    if (ipd_left > 0U)                                  /* binary TCP payload */
    {
      ipd_left--;
      on_mqtt_event(mqtt_parser_feed(&parser, c));
      continue;
    }
    if (c == '>' && line_len == 0U) { got_prompt = 1; continue; }
    if (c == '\r' || c == '\n')
    {
      if (line_len > 0U) on_line();
      line_len = 0;
      continue;
    }
    if (c == ':' && line_len >= 5U && strncmp(line, "+IPD,", 5) == 0)
    {
      line[line_len] = '\0';
      ipd_left = ipd_length();
      line_len = 0;
      continue;
    }
    if (line_len < LINE_MAX - 1U) line[line_len++] = (char)c;
  }
}

static bool wait_until(volatile uint8_t *flag, volatile uint8_t *fail, uint32_t timeout_ms)
{
  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < timeout_ms)
  {
    esp_pump();
    if (*flag) return true;
    if (fail != NULL && *fail) return false;
  }
  return false;
}

static void esp_write(const uint8_t *d, uint16_t n)
{
  HAL_UART_Transmit(esp_uart, (uint8_t *)d, n, 1000);   /* RX interrupt keeps running meanwhile */
}

static funcState_t at_cmd(const char *cmd, uint32_t timeout_ms)
{
  esp_pump();
  got_ok = 0; got_error = 0;
  esp_write((const uint8_t *)cmd, (uint16_t)strlen(cmd));
  esp_write((const uint8_t *)"\r\n", 2);
  return wait_until(&got_ok, &got_error, timeout_ms) ? funcOk : funcErr;
}

/* ========================================================================== */
/*  ESP8266                                                                   */
/* ========================================================================== */
funcState_t ESP8266_InitPtr(espMode_t mode, UART_HandleTypeDef *huart)
{
  char cmd[24];
  esp_uart = huart;
  ring_head = ring_tail = 0; line_len = 0; ipd_left = 0;
  mqtt_parser_init(&parser);
  HAL_UART_Receive_IT(esp_uart, &uartRxData, 1);

  HAL_Delay(500);                                     /* let boot messages arrive */
  funcState_t ok = funcErr;
  for (int i = 0; i < 10 && ok != funcOk; i++)
    ok = at_cmd("AT", 1000);
  if (ok != funcOk) return funcErr;
  at_cmd("ATE0", T_CMD_MS);                          /* echo off (result not critical) */
  snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d", (int)mode);
  return at_cmd(cmd, T_CMD_MS);
}

funcState_t ESP8266_Reset(void)
{
  funcState_t r = at_cmd("AT+RST", T_CMD_MS);
  HAL_Delay(2000);
  esp_pump();
  return r;
}

funcState_t ESP8266_wifiConnect(char *SSID, char *Password)
{
  char cmd[100];
  at_cmd("AT+CWQAP", T_CMD_MS);                       /* leave any previous AP */
  if (snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", SSID, Password) >= (int)sizeof(cmd))
    return funcErr;
  for (int attempt = 0; attempt < 3; attempt++)
    if (at_cmd(cmd, T_JOIN_MS) == funcOk) return funcOk;
  return funcErr;
}

funcState_t ESP8266_portConnect(char *type, char *remoteIP, char *remotePort)
{
  char cmd[100];
  at_cmd("AT+CIPCLOSE", T_CMD_MS);                    /* "ERROR" here just means nothing was open */
  at_cmd("AT+CIPMUX=0", T_CMD_MS);                    /* single connection: "+IPD,<len>:" framing */
  if (snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"%s\",\"%s\",%s", type, remoteIP, remotePort) >= (int)sizeof(cmd))
    return funcErr;
  got_connect = 0;
  mqtt_parser_init(&parser);
  funcState_t r = at_cmd(cmd, T_TCP_MS);
  return (r == funcOk || got_connect) ? funcOk : funcErr;
}

funcState_t ESP8266_sendMessage(const uint8_t *msg, uint16_t msgSize)
{
  char cmd[24];
  if (msgSize == 0U || msgSize > 2048U) return funcErr;
  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", msgSize);
  got_prompt = 0; got_send_ok = 0; got_send_fail = 0;
  if (at_cmd(cmd, T_CMD_MS) != funcOk && !got_prompt) return funcErr;
  if (!wait_until(&got_prompt, &got_error, T_CMD_MS)) return funcErr;
  esp_write(msg, msgSize);
  return wait_until(&got_send_ok, &got_send_fail, T_SEND_MS) ? funcOk : funcErr;
}

uint32_t ESP8266_rxOverflows(void) { return ring_overflows; }

/* ========================================================================== */
/*  MQTT                                                                      */
/* ========================================================================== */
funcState_t MQTT_connectBroker(MQTT_InitTypeDef_t *c)
{
  const char *user = (c->connectFlag & UserNameFlag) ? c->username : NULL;
  const char *pass = (c->connectFlag & PasswordFlag) ? c->password : NULL;
  size_t n = mqtt_build_connect(txbuf, sizeof(txbuf), c->clientID, user, pass,
                                c->keepAlive, (c->connectFlag & CleanSession) != 0U);
  if (n == 0U) return funcErr;
  connack_code = -1;
  mqtt_connected = false;
  if (ESP8266_sendMessage(txbuf, (uint16_t)n) != funcOk) return funcErr;

  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < T_ACK_MS && connack_code < 0) esp_pump();
  return (connack_code == 0) ? funcOk : funcErr;      /* 0 = accepted, 1..5 = refused */
}

funcState_t MQTT_publishTopic(char *topic, char *msg, uint8_t QoS)
{
  (void)QoS;                                          /* QoS 0 only */
  size_t n = mqtt_build_publish(txbuf, sizeof(txbuf), topic, (const uint8_t *)msg, strlen(msg), 0);
  if (n == 0U || !mqtt_connected) return funcErr;
  return ESP8266_sendMessage(txbuf, (uint16_t)n);
}

funcState_t MQTT_subscribeTopic(char *topic, uint8_t QoS)
{
  (void)QoS;                                          /* subscribe with QoS 0 */
  uint16_t id = next_packet_id++;
  if (next_packet_id == 0U) next_packet_id = 1;
  size_t n = mqtt_build_subscribe(txbuf, sizeof(txbuf), id, topic, 0);
  if (n == 0U || !mqtt_connected) return funcErr;
  suback_code = -1;
  if (ESP8266_sendMessage(txbuf, (uint16_t)n) != funcOk) return funcErr;

  uint32_t t0 = HAL_GetTick();
  while ((HAL_GetTick() - t0) < T_ACK_MS && !(suback_code >= 0 && suback_id == id)) esp_pump();
  return (suback_code >= 0 && suback_id == id && suback_code != 0x80) ? funcOk : funcErr;
}

funcState_t MQTT_unsubscribeTopic(char *topic)
{
  size_t tlen = strlen(topic);
  uint16_t id = next_packet_id++;
  if (next_packet_id == 0U) next_packet_id = 1;
  if (tlen == 0U || tlen + 8U > sizeof(txbuf)) return funcErr;
  size_t body = 2U + 2U + tlen;
  uint8_t rl[4];
  size_t rn = mqtt_encode_remaining_length(rl, (uint32_t)body);
  size_t pos = 0;
  txbuf[pos++] = 0xA2;                                 /* UNSUBSCRIBE, reserved flags 0010 */
  memcpy(&txbuf[pos], rl, rn); pos += rn;
  txbuf[pos++] = (uint8_t)(id >> 8); txbuf[pos++] = (uint8_t)id;
  txbuf[pos++] = (uint8_t)(tlen >> 8); txbuf[pos++] = (uint8_t)tlen;
  memcpy(&txbuf[pos], topic, tlen); pos += tlen;
  return ESP8266_sendMessage(txbuf, (uint16_t)pos);
}

funcState_t MQTT_disconnectBroker(void)
{
  size_t n = mqtt_build_disconnect(txbuf, sizeof(txbuf));
  mqtt_connected = false;
  return ESP8266_sendMessage(txbuf, (uint16_t)n);
}

funcState_t MQTT_pingReq(void)
{
  size_t n = mqtt_build_pingreq(txbuf, sizeof(txbuf));
  return ESP8266_sendMessage(txbuf, (uint16_t)n);
}

bool MQTT_isConnected(void)
{
  esp_pump();
  return mqtt_connected && tcp_connected;
}

int MQTT_getMessage(char *topic, size_t topic_size, char *msg, size_t msg_size)
{
  esp_pump();
  if (!msg_pending || topic_size == 0U || msg_size == 0U) return 0;
  strncpy(topic, msg_topic, topic_size - 1U); topic[topic_size - 1U] = '\0';
  strncpy(msg, msg_payload, msg_size - 1U);   msg[msg_size - 1U] = '\0';
  msg_pending = false;
  return 1;
}

int MQTTDataHandler(char *mqttTopic, char *mqttMsg)
{
  return MQTT_getMessage(mqttTopic, MQTT_TOPIC_MAX, mqttMsg, MQTT_MSG_MAX);
}

void MQTTTimer(void)
{
  /* The original library used SysTick to detect the end of a packet by silence.
     Packets are now delimited by the +IPD length and the MQTT Remaining Length,
     so nothing is needed here. Kept so existing SysTick hooks still link. */
}

uint32_t MQTT_droppedMessages(void) { return msg_dropped; }
