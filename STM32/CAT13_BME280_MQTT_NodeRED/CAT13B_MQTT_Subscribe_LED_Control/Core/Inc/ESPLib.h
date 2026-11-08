/**
  ******************************************************************************
  * @file    ESPLib.h
  * @brief   ESP8266 (AT firmware) + MQTT 3.1.1 client for STM32 HAL.
  *
  *          Keeps the function names of the course "ESPLib" (ESP8266_Init,
  *          ESP8266_wifiConnect, ESP8266_portConnect, MQTT_connectBroker,
  *          MQTT_publishTopic, MQTT_subscribeTopic, MQTTDataHandler, MQTTTimer)
  *          so lab-sheet code compiles unchanged, but the implementation is new:
  *
  *          - UART RX interrupt is armed once and never aborted; every byte goes
  *            into a 1 KB ring buffer (nothing is lost while sending).
  *          - The ESP8266 "+IPD,<len>:" framing is removed and the TCP bytes are
  *            fed to a validating MQTT parser (mqtt_packet.c): Remaining Length
  *            up to 4 bytes, every length checked before copying, strings always
  *            NUL-terminated, oversized packets skipped.
  *          - CONNECT waits for CONNACK, SUBSCRIBE waits for SUBACK, both checked.
  *          - Every wait has a timeout; functions return funcOk / funcErr.
  ******************************************************************************
  */
#ifndef INC_ESPLIB_H_
#define INC_ESPLIB_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "mqtt_packet.h"

/* Buffer sizes that MQTTDataHandler() assumes for its two output strings */
#define MQTT_TOPIC_MAX   64U
#define MQTT_MSG_MAX     128U

typedef enum { funcErr = 0, funcOk = 1 } funcState_t;
typedef enum { StationMode = 1, AccessPointMode, StationAP } espMode_t;

typedef struct
{
  char    *clientID;
  char    *username;      /* used only when connectFlag has UserNameFlag */
  char    *password;      /* used only when connectFlag has PasswordFlag */
  uint16_t keepAlive;     /* seconds */
  uint8_t  connectFlag;   /* CleanSession | UserNameFlag | PasswordFlag */
} MQTT_InitTypeDef_t;

/* Connect flags (same values as the course library) */
#define CleanSession   0x02
#define PasswordFlag   0x40
#define UserNameFlag   0x80

/* QoS levels: this client supports QoS 0 only. Publish/subscribe calls with
   QoS1/QoS2 return funcErr; incoming QoS 1/2 PUBLISH packets are dropped and counted. */
#define QoS0           0x00
#define QoS1           0x01
#define QoS2           0x02

/* Byte the RX interrupt receives into (kept for lab-sheet compatibility;
   calling HAL_UART_Receive_IT(&huart, &uartRxData, 1) again is harmless) */
extern uint8_t uartRxData;

/* ------------------------------------------------------------------ ESP8266 */
funcState_t ESP8266_InitPtr(espMode_t mode, UART_HandleTypeDef *huart);
/* Lab-sheet call style: ESP8266_Init(StationMode, huart1); */
#define ESP8266_Init(mode, handle)   ESP8266_InitPtr((mode), &(handle))
funcState_t ESP8266_Reset(void);
funcState_t ESP8266_wifiConnect(char *SSID, char *Password);
funcState_t ESP8266_portConnect(char *type, char *remoteIP, char *remotePort);
funcState_t ESP8266_sendMessage(const uint8_t *msg, uint16_t msgSize);

/* --------------------------------------------------------------------- MQTT */
funcState_t MQTT_connectBroker(MQTT_InitTypeDef_t *MQTTConnect);     /* waits for CONNACK = accepted */
funcState_t MQTT_publishTopic(char *topic, char *msg, uint8_t QoS);    /* QoS must be QoS0 */
funcState_t MQTT_subscribeTopic(char *topic, uint8_t QoS);            /* QoS0; waits for SUBACK granting QoS 0 */
funcState_t MQTT_unsubscribeTopic(char *topic);                       /* waits for UNSUBACK */
#define MQTT_unsubsribeTopic MQTT_unsubscribeTopic                     /* spelling used in the course header */
funcState_t MQTT_disconnectBroker(void);
funcState_t MQTT_pingReq(void);
bool        MQTT_isConnected(void);                                   /* false after CLOSED / Wi-Fi loss */

/* Process received data. Returns 1 and fills topic (MQTT_TOPIC_MAX bytes) and
   msg (MQTT_MSG_MAX bytes) when a PUBLISH arrived, otherwise 0 and leaves them unchanged. */
int  MQTTDataHandler(char *mqttTopic, char *mqttMsg);
/* Same, with explicit buffer sizes */
int  MQTT_getMessage(char *topic, size_t topic_size, char *msg, size_t msg_size);
/* Kept for compatibility with the course SysTick hook; no longer required */
void MQTTTimer(void);

/* Diagnostics */
uint32_t ESP8266_rxOverflows(void);     /* bytes lost because the ring buffer was full */
uint32_t ESP8266_txErrors(void);        /* HAL_UART_Transmit failures */
uint32_t ESP8266_rearmFailures(void);   /* RX interrupt re-arm refused (then retried) */
uint32_t MQTT_droppedMessages(void);    /* PUBLISH packets dropped (not collected in time / malformed) */

#endif /* INC_ESPLIB_H_ */
