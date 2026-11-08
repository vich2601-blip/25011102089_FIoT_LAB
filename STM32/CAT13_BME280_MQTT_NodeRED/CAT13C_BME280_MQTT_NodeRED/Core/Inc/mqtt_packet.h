/**
  ******************************************************************************
  * @file    mqtt_packet.h
  * @brief   MQTT 3.1.1 packet encoder / stream decoder (no hardware dependency).
  *
  *          Encoding : CONNECT, PUBLISH (QoS 0), SUBSCRIBE, PINGREQ, DISCONNECT,
  *                     with the full variable-length "Remaining Length" field.
  *          Decoding : a byte-at-a-time state machine that reassembles packets
  *                     from a TCP stream, validates every length before copying
  *                     and reports CONNACK, SUBACK, PUBLISH and PINGRESP.
  *
  *          Pure C (no HAL), so it is unit-tested on a PC: tests/test_mqtt_packet.c
  ******************************************************************************
  */
#ifndef MQTT_PACKET_H
#define MQTT_PACKET_H

#include <stdint.h>
#include <stddef.h>

#define MQTT_PKT_CONNECT     1U
#define MQTT_PKT_CONNACK     2U
#define MQTT_PKT_PUBLISH     3U
#define MQTT_PKT_SUBSCRIBE   8U
#define MQTT_PKT_SUBACK      9U
#define MQTT_PKT_UNSUBSCRIBE 10U
#define MQTT_PKT_UNSUBACK    11U
#define MQTT_PKT_PINGREQ     12U
#define MQTT_PKT_PINGRESP    13U
#define MQTT_PKT_DISCONNECT  14U

#define MQTT_MAX_REMAINING   268435455UL   /* largest value 4 length bytes can encode */
#define MQTT_RX_PACKET_MAX   256U          /* bigger incoming packets are skipped safely */

/* ------------------------------------------------------------------ encoding */
/* All encoders return the packet length written to buf, or 0 if it does not fit. */
size_t mqtt_encode_remaining_length(uint8_t *out, uint32_t value);   /* 1-4 bytes, 0 on error */

size_t mqtt_build_connect(uint8_t *buf, size_t size, const char *client_id,
                          const char *username, const char *password,
                          uint16_t keep_alive_s, int clean_session);
size_t mqtt_build_publish(uint8_t *buf, size_t size, const char *topic,
                          const uint8_t *payload, size_t payload_len, int retain);
size_t mqtt_build_subscribe(uint8_t *buf, size_t size, uint16_t packet_id,
                            const char *topic, uint8_t qos);
size_t mqtt_build_unsubscribe(uint8_t *buf, size_t size, uint16_t packet_id, const char *topic);
size_t mqtt_build_pingreq(uint8_t *buf, size_t size);
size_t mqtt_build_disconnect(uint8_t *buf, size_t size);

/* ------------------------------------------------------------------ decoding */
typedef enum
{
  MQTT_EVT_NONE = 0,
  MQTT_EVT_CONNACK,     /* code = CONNACK return code (0 = accepted)          */
  MQTT_EVT_SUBACK,      /* code = granted QoS, 0x80 = refused; id = packet id */
  MQTT_EVT_PUBLISH,     /* topic / payload valid; qos = delivery QoS           */
  MQTT_EVT_UNSUBACK,    /* id = packet id                                      */
  MQTT_EVT_PINGRESP,
  MQTT_EVT_OTHER,       /* well-formed packet of another type                  */
  MQTT_EVT_ERROR        /* malformed packet (bad length / field), skipped      */
} mqtt_event_t;

typedef struct
{
  /* parser state (private) */
  uint8_t  state;
  uint8_t  header;
  uint32_t remaining;
  uint32_t multiplier;
  uint8_t  len_bytes;
  uint32_t received;
  uint8_t  body[MQTT_RX_PACKET_MAX];
  uint8_t  oversized;

  /* result of the last completed packet */
  uint8_t  code;
  uint8_t  qos;              /* PUBLISH: QoS from the fixed header */
  uint16_t id;
  const char    *topic;        /* NUL-terminated copy inside topic_buf */
  const uint8_t *payload;      /* NUL-terminated copy inside payload_buf */
  size_t         payload_len;
  char     topic_buf[96];
  uint8_t  payload_buf[160];
} mqtt_parser_t;

void         mqtt_parser_init(mqtt_parser_t *p);
mqtt_event_t mqtt_parser_feed(mqtt_parser_t *p, uint8_t byte);   /* call once per received byte */

#endif /* MQTT_PACKET_H */
