/*
 * esp8266.h - ESP8266 (AT firmware) Wi-Fi + simple MQTT publish over USART1
 *
 * Wiring (ESP-01):
 *   ESP TX  -> PA10 (USART1_RX)
 *   ESP RX  -> PA9  (USART1_TX)
 *   VCC, CH_EN -> 3.3V   GND -> GND
 */
#ifndef ESP8266_H
#define ESP8266_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* ======== CHANGE THESE ======== */
/* Wi-Fi name and password go in Core/Inc/wifi_secrets.h (git-ignored):
 *   #define WIFI_SSID     "your-network"
 *   #define WIFI_PASSWORD "your-password"                                  */
#if defined(__has_include)
#  if __has_include("wifi_secrets.h")
#    include "wifi_secrets.h"
#  endif
#endif
#ifndef WIFI_SSID
#define WIFI_SSID        "YOUR_WIFI_SSID"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD    "YOUR_WIFI_PASSWORD"
#endif

#define MQTT_BROKER      "broker.hivemq.com"   /* free public test broker */
#define MQTT_PORT        1883
#define MQTT_CLIENT_ID   "stm32_storage_guard"
#define MQTT_USER        ""                    /* leave "" if not needed */
#define MQTT_PASS        ""                    /* leave "" if not needed */
#define MQTT_TOPIC       "storage/guard/data"
#define MQTT_KEEPALIVE   60                    /* seconds */
/* ============================== */

/* Start the ESP8266 driver (call once after MX_USART1_UART_Init) */
void ESP_Init(UART_HandleTypeDef *huart);

/* Connect to Wi-Fi. Returns 1 if connected, 0 if failed */
uint8_t ESP_ConnectWiFi(void);

/* Open TCP to the broker and send MQTT CONNECT. Returns 1 if OK */
uint8_t MQTT_Connect(void);

/* Publish a message (QoS 0). Returns 1 if sent */
uint8_t MQTT_Publish(const char *topic, const char *message);

/* 1 if the MQTT connection is up */
uint8_t MQTT_IsConnected(void);

#endif /* ESP8266_H */
