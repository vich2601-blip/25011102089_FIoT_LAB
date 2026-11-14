/*
 * esp8266.c - ESP8266 (AT firmware) Wi-Fi + simple MQTT publish over USART1
 *
 * Works with the normal ESP8266 AT firmware (no MQTT AT commands needed):
 * the MQTT packets are built here and sent through a TCP connection.
 */
#include "esp8266.h"
#include <stdio.h>
#include <string.h>

#define RX_BUF_SIZE   512

static UART_HandleTypeDef *esp_uart;
static uint8_t  rx_byte;
uint8_t  rx_buf[RX_BUF_SIZE];          /* global for debugger */
volatile uint16_t rx_len;              /* global for debugger */
uint8_t  mqtt_connected;               /* global for debugger */

/* ---------- UART receive (interrupt, 1 byte at a time) ---------- */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == esp_uart)
    {
        if (rx_len < RX_BUF_SIZE)
            rx_buf[rx_len++] = rx_byte;
        HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == esp_uart)
        HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);   /* restart after error */
}

static void rx_clear(void)
{
    rx_len = 0;
}

/* Search for bytes in the receive buffer (works with binary data) */
static int rx_find(const char *str, uint16_t len)
{
    uint16_t n = rx_len;
    for (uint16_t i = 0; i + len <= n; i++)
        if (memcmp(&rx_buf[i], str, len) == 0)
            return i;
    return -1;
}

/* Wait until "expect" is received. Returns 1 if found, 0 on timeout/ERROR */
static uint8_t wait_for(const char *expect, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout_ms)
    {
        if (rx_find(expect, strlen(expect)) >= 0) return 1;
        if (rx_find("ERROR", 5) >= 0 || rx_find("FAIL", 4) >= 0) return 0;
    }
    return 0;
}

static void send_raw(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(esp_uart, (uint8_t *)data, len, 1000);
}

/* Send an AT command and wait for the reply */
static uint8_t send_cmd(const char *cmd, const char *expect, uint32_t timeout_ms)
{
    rx_clear();
    send_raw((const uint8_t *)cmd, strlen(cmd));
    send_raw((const uint8_t *)"\r\n", 2);
    return wait_for(expect, timeout_ms);
}

/* Send binary data over the open TCP connection */
static uint8_t tcp_send(const uint8_t *data, uint16_t len)
{
    char cmd[24];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", len);
    if (!send_cmd(cmd, ">", 2000)) return 0;

    rx_clear();
    send_raw(data, len);
    return wait_for("SEND OK", 3000);
}

/* ---------- ESP8266 ---------- */

void ESP_Init(UART_HandleTypeDef *huart)
{
    esp_uart = huart;
    mqtt_connected = 0;
    rx_clear();
    HAL_UART_Receive_IT(esp_uart, &rx_byte, 1);

    send_cmd("AT+RST", "ready", 5000);   /* restart ESP */
    HAL_Delay(1000);
    send_cmd("ATE0", "OK", 1000);        /* echo off */
}

uint8_t ESP_ConnectWiFi(void)
{
    char cmd[128];

    if (!send_cmd("AT", "OK", 1000)) return 0;
    send_cmd("AT+CWMODE=1", "OK", 1000);     /* station mode */
    send_cmd("AT+CIPMUX=0", "OK", 1000);     /* single connection */

    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWORD);
    return send_cmd(cmd, "OK", 20000);       /* joining can take a while */
}

/* ---------- MQTT ---------- */

/* MQTT "remaining length" field (1-4 bytes) */
static uint16_t put_length(uint8_t *buf, uint32_t len)
{
    uint16_t i = 0;
    do {
        uint8_t b = len % 128;
        len /= 128;
        if (len > 0) b |= 0x80;
        buf[i++] = b;
    } while (len > 0);
    return i;
}

/* MQTT string: 2-byte length + text */
static uint16_t put_string(uint8_t *buf, const char *s)
{
    uint16_t len = strlen(s);
    buf[0] = len >> 8;
    buf[1] = len & 0xFF;
    memcpy(&buf[2], s, len);
    return len + 2;
}

uint8_t MQTT_Connect(void)
{
    char cmd[96];
    uint8_t body[128];
    uint8_t pkt[136];
    uint16_t n = 0, p = 0;
    uint8_t flags = 0x02;   /* clean session */

    mqtt_connected = 0;

    /* 1. Open TCP connection to the broker */
    send_cmd("AT+CIPCLOSE", "OK", 1000);
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d", MQTT_BROKER, MQTT_PORT);
    if (!send_cmd(cmd, "CONNECT", 10000)) return 0;
    HAL_Delay(200);

    /* 2. Build MQTT CONNECT packet */
    if (strlen(MQTT_USER) > 0) flags |= 0x80;
    if (strlen(MQTT_PASS) > 0) flags |= 0x40;

    n += put_string(&body[n], "MQTT");          /* protocol name */
    body[n++] = 0x04;                          /* protocol level 3.1.1 */
    body[n++] = flags;
    body[n++] = MQTT_KEEPALIVE >> 8;
    body[n++] = MQTT_KEEPALIVE & 0xFF;
    n += put_string(&body[n], MQTT_CLIENT_ID);
    if (flags & 0x80) n += put_string(&body[n], MQTT_USER);
    if (flags & 0x40) n += put_string(&body[n], MQTT_PASS);

    pkt[p++] = 0x10;                           /* CONNECT */
    p += put_length(&pkt[p], n);
    memcpy(&pkt[p], body, n);
    p += n;

    /* 3. Send it and wait for CONNACK (0x20 0x02 0x00 0x00) */
    if (!tcp_send(pkt, p)) return 0;

    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 5000)
    {
        int i = rx_find("+IPD,4:", 7);
        if (i >= 0 && rx_len >= i + 7 + 4)
        {
            uint8_t *ack = &rx_buf[i + 7];
            if (ack[0] == 0x20 && ack[3] == 0x00)
            {
                mqtt_connected = 1;
                return 1;
            }
            return 0;   /* broker refused */
        }
    }
    return 0;
}

uint8_t MQTT_Publish(const char *topic, const char *message)
{
    static uint8_t pkt[300];
    uint16_t topic_len = strlen(topic);
    uint16_t msg_len = strlen(message);
    uint32_t rem = 2 + topic_len + msg_len;
    uint16_t p = 0;

    if (!mqtt_connected) return 0;
    if (rem + 5 > sizeof(pkt)) return 0;

    pkt[p++] = 0x30;                           /* PUBLISH, QoS 0 */
    p += put_length(&pkt[p], rem);
    p += put_string(&pkt[p], topic);
    memcpy(&pkt[p], message, msg_len);
    p += msg_len;

    if (!tcp_send(pkt, p))
    {
        mqtt_connected = 0;                    /* connection lost */
        return 0;
    }
    return 1;
}

uint8_t MQTT_IsConnected(void)
{
    return mqtt_connected;
}
