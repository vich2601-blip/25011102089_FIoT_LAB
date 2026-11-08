/*
 * Host-side unit tests for Common_Libraries/ESPLib_MQTT/mqtt_packet.c
 * Build and run:  make -C tests      (or: cc -I../Common_Libraries/ESPLib_MQTT ...)
 */
#include <stdio.h>
#include <string.h>
#include "mqtt_packet.h"

static int failures = 0, checks = 0;
#define CHECK(cond) do { checks++; if (!(cond)) { failures++; printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static mqtt_event_t feed_all(mqtt_parser_t *p, const uint8_t *d, size_t n, int *events)
{
  mqtt_event_t last = MQTT_EVT_NONE;
  *events = 0;
  for (size_t i = 0; i < n; i++)
  {
    mqtt_event_t e = mqtt_parser_feed(p, d[i]);
    if (e != MQTT_EVT_NONE) { last = e; (*events)++; }
  }
  return last;
}

static void test_remaining_length(void)
{
  uint8_t b[4];
  CHECK(mqtt_encode_remaining_length(b, 0) == 1 && b[0] == 0x00);
  CHECK(mqtt_encode_remaining_length(b, 127) == 1 && b[0] == 0x7F);
  CHECK(mqtt_encode_remaining_length(b, 128) == 2 && b[0] == 0x80 && b[1] == 0x01);
  CHECK(mqtt_encode_remaining_length(b, 16383) == 2 && b[0] == 0xFF && b[1] == 0x7F);
  CHECK(mqtt_encode_remaining_length(b, 16384) == 3);
  CHECK(mqtt_encode_remaining_length(b, 268435455UL) == 4);
  CHECK(mqtt_encode_remaining_length(b, 268435456UL) == 0);           /* too large */
}

static void test_connect(void)
{
  uint8_t b[128];
  size_t n = mqtt_build_connect(b, sizeof b, "stm32", NULL, NULL, 60, 1);
  const uint8_t expect[] = { 0x10, 17, 0, 4, 'M', 'Q', 'T', 'T', 4, 0x02, 0, 60, 0, 5, 's', 't', 'm', '3', '2' };
  CHECK(n == sizeof expect && memcmp(b, expect, n) == 0);

  n = mqtt_build_connect(b, sizeof b, "id", "user", "pw", 30, 1);
  CHECK(n > 0 && b[9] == (0x02 | 0x80 | 0x40));                       /* username + password flags */
  CHECK(mqtt_build_connect(b, 10, "stm32", NULL, NULL, 60, 1) == 0);    /* buffer too small */
  CHECK(mqtt_build_connect(b, sizeof b, "id", NULL, "pw", 30, 1) == 0); /* password without username is illegal */
}

static void test_publish_long_payload(void)
{
  uint8_t b[512], payload[200];
  memset(payload, 'x', sizeof payload);
  size_t n = mqtt_build_publish(b, sizeof b, "a/b", payload, sizeof payload, 0);
  /* remaining = 2 + 3 + 200 = 205 -> needs 2 length bytes (the old library only had 1) */
  CHECK(n == 1 + 2 + 205 && b[0] == 0x30 && b[1] == (205 % 128 | 0x80) && b[2] == 205 / 128);
  CHECK(mqtt_build_publish(b, 50, "a/b", payload, sizeof payload, 0) == 0);   /* does not fit */
  CHECK(mqtt_build_publish(b, sizeof b, "", payload, 1, 0) == 0);             /* empty topic */
}

static void test_subscribe(void)
{
  uint8_t b[64];
  size_t n = mqtt_build_subscribe(b, sizeof b, 7, "t/x", 0);
  const uint8_t expect[] = { 0x82, 8, 0, 7, 0, 3, 't', '/', 'x', 0 };
  CHECK(n == sizeof expect && memcmp(b, expect, n) == 0);
  CHECK(mqtt_build_subscribe(b, sizeof b, 0, "t", 0) == 0);           /* packet id 0 is illegal */
  n = mqtt_build_unsubscribe(b, sizeof b, 9, "t/x");
  const uint8_t unsub[] = { 0xA2, 7, 0, 9, 0, 3, 't', '/', 'x' };
  CHECK(n == sizeof unsub && memcmp(b, unsub, n) == 0);
  CHECK(mqtt_build_unsubscribe(b, sizeof b, 0, "t") == 0);
}

static void test_parse_acks(void)
{
  mqtt_parser_t p; int ev;
  mqtt_parser_init(&p);
  const uint8_t connack_ok[] = { 0x20, 0x02, 0x00, 0x00 };
  CHECK(feed_all(&p, connack_ok, sizeof connack_ok, &ev) == MQTT_EVT_CONNACK && p.code == 0 && ev == 1);
  const uint8_t connack_refused[] = { 0x20, 0x02, 0x00, 0x05 };
  CHECK(feed_all(&p, connack_refused, sizeof connack_refused, &ev) == MQTT_EVT_CONNACK && p.code == 5);
  const uint8_t suback[] = { 0x90, 0x03, 0x00, 0x07, 0x00 };
  CHECK(feed_all(&p, suback, sizeof suback, &ev) == MQTT_EVT_SUBACK && p.id == 7 && p.code == 0);
  const uint8_t suback_fail[] = { 0x90, 0x03, 0x00, 0x08, 0x80 };
  CHECK(feed_all(&p, suback_fail, sizeof suback_fail, &ev) == MQTT_EVT_SUBACK && p.code == 0x80);
  const uint8_t pingresp[] = { 0xD0, 0x00 };
  CHECK(feed_all(&p, pingresp, sizeof pingresp, &ev) == MQTT_EVT_PINGRESP);
  const uint8_t bad_connack[] = { 0x20, 0x03, 0, 0, 0 };              /* wrong length */
  CHECK(feed_all(&p, bad_connack, sizeof bad_connack, &ev) == MQTT_EVT_ERROR);
  const uint8_t connack_flags[] = { 0x20, 0x02, 0x02, 0x00 };         /* reserved ack-flag bits set */
  CHECK(feed_all(&p, connack_flags, sizeof connack_flags, &ev) == MQTT_EVT_ERROR);
  const uint8_t connack_code[] = { 0x20, 0x02, 0x00, 0x09 };          /* return code > 5 */
  CHECK(feed_all(&p, connack_code, sizeof connack_code, &ev) == MQTT_EVT_ERROR);
  const uint8_t connack_hdr[] = { 0x21, 0x02, 0x00, 0x00 };           /* fixed-header flags must be 0 */
  CHECK(feed_all(&p, connack_hdr, sizeof connack_hdr, &ev) == MQTT_EVT_ERROR);
  const uint8_t suback_id0[] = { 0x90, 0x03, 0x00, 0x00, 0x00 };      /* packet id 0 */
  CHECK(feed_all(&p, suback_id0, sizeof suback_id0, &ev) == MQTT_EVT_ERROR);
  const uint8_t suback_bad[] = { 0x90, 0x03, 0x00, 0x09, 0x05 };      /* return code 5 is not allowed */
  CHECK(feed_all(&p, suback_bad, sizeof suback_bad, &ev) == MQTT_EVT_ERROR);
  const uint8_t unsuback[] = { 0xB0, 0x02, 0x00, 0x09 };
  CHECK(feed_all(&p, unsuback, sizeof unsuback, &ev) == MQTT_EVT_UNSUBACK && p.id == 9);
  const uint8_t unsuback_id0[] = { 0xB0, 0x02, 0x00, 0x00 };
  CHECK(feed_all(&p, unsuback_id0, sizeof unsuback_id0, &ev) == MQTT_EVT_ERROR);
  const uint8_t unsuback_hdr[] = { 0xB2, 0x02, 0x00, 0x09 };          /* flags must be 0 */
  CHECK(feed_all(&p, unsuback_hdr, sizeof unsuback_hdr, &ev) == MQTT_EVT_ERROR);
  const uint8_t ping_hdr[] = { 0xD1, 0x00 };                          /* PINGRESP flags must be 0 */
  CHECK(feed_all(&p, ping_hdr, sizeof ping_hdr, &ev) == MQTT_EVT_ERROR);
  const uint8_t dup_q0[] = { 0x38, 0x04, 0x00, 0x01, 'a', 'x' };      /* DUP set on a QoS 0 PUBLISH */
  CHECK(feed_all(&p, dup_q0, sizeof dup_q0, &ev) == MQTT_EVT_ERROR);
  CHECK(feed_all(&p, connack_ok, sizeof connack_ok, &ev) == MQTT_EVT_CONNACK);   /* still in sync afterwards */
}

static void test_parse_publish(void)
{
  mqtt_parser_t p; int ev;
  mqtt_parser_init(&p);
  /* SUBACK and PUBLISH arriving back to back in one TCP segment (no ':' anywhere) */
  const uint8_t stream[] = { 0x90, 0x03, 0x00, 0x01, 0x00,
                             0x30, 0x0B, 0x00, 0x05, 'a', '/', 'l', 'e', 'd', 'O', 'N', '!', '!' };
  mqtt_event_t last = feed_all(&p, stream, sizeof stream, &ev);
  CHECK(ev == 2 && last == MQTT_EVT_PUBLISH);
  CHECK(strcmp(p.topic, "a/led") == 0);
  CHECK(p.payload_len == 4 && memcmp(p.payload, "ON!!", 4) == 0 && p.payload[4] == 0);

  /* QoS 1 publish: packet id must be skipped */
  const uint8_t q1[] = { 0x32, 0x09, 0x00, 0x01, 'x', 0x12, 0x34, 'h', 'e', 'y', '!' };
  CHECK(feed_all(&p, q1, sizeof q1, &ev) == MQTT_EVT_PUBLISH && p.id == 0x1234 && p.qos == 1 && strcmp((const char *)p.payload, "hey!") == 0);
  const uint8_t q1_id0[] = { 0x32, 0x06, 0x00, 0x01, 'x', 0x00, 0x00, '!' };   /* QoS 1 with packet id 0 */
  CHECK(feed_all(&p, q1_id0, sizeof q1_id0, &ev) == MQTT_EVT_ERROR);

  /* topic length larger than the packet -> rejected, nothing copied */
  const uint8_t lying[] = { 0x30, 0x04, 0x00, 0x50, 'a', 'b' };
  CHECK(feed_all(&p, lying, sizeof lying, &ev) == MQTT_EVT_ERROR);

  /* packet larger than the receive buffer is skipped, and the next packet still parses */
  uint8_t big[3 + 300 + 2];
  big[0] = 0x30; big[1] = (uint8_t)(300 % 128) | 0x80; big[2] = 300 / 128;
  memset(&big[3], 'z', 300);
  big[303] = 0xD0; big[304] = 0x00;                                  /* PINGRESP after it */
  mqtt_parser_init(&p);
  mqtt_event_t e1 = MQTT_EVT_NONE, e2 = MQTT_EVT_NONE;
  for (size_t i = 0; i < 303; i++) { mqtt_event_t e = mqtt_parser_feed(&p, big[i]); if (e) e1 = e; }
  for (size_t i = 303; i < sizeof big; i++) { mqtt_event_t e = mqtt_parser_feed(&p, big[i]); if (e) e2 = e; }
  CHECK(e1 == MQTT_EVT_ERROR && e2 == MQTT_EVT_PINGRESP);

  /* more than 4 Remaining Length bytes is malformed */
  const uint8_t non_canonical[] = { 0xD0, 0x80, 0x00 };              /* length 0 written in two bytes */
  CHECK(feed_all(&p, non_canonical, sizeof non_canonical, &ev) == MQTT_EVT_ERROR);
  const uint8_t suback_two[] = { 0x90, 0x04, 0x00, 0x05, 0x00, 0x00 }; /* two codes for a one-topic request */
  CHECK(feed_all(&p, suback_two, sizeof suback_two, &ev) == MQTT_EVT_ERROR);
  const uint8_t too_long_len[] = { 0x30, 0xFF, 0xFF, 0xFF, 0xFF };
  CHECK(feed_all(&p, too_long_len, sizeof too_long_len, &ev) == MQTT_EVT_ERROR);
}

static void test_roundtrip(void)
{
  uint8_t b[256]; mqtt_parser_t p; int ev;
  const char *json = "{\"status\":\"success\",\"temperature\":28.45,\"humidity\":62.30,\"pressure\":1008.72}";
  size_t n = mqtt_build_publish(b, sizeof b, "vishrudh/stm32/bme280", (const uint8_t *)json, strlen(json), 0);
  mqtt_parser_init(&p);
  CHECK(n > 0 && feed_all(&p, b, n, &ev) == MQTT_EVT_PUBLISH);
  CHECK(strcmp(p.topic, "vishrudh/stm32/bme280") == 0 && strcmp((const char *)p.payload, json) == 0);
}

int main(void)
{
  test_remaining_length();
  test_connect();
  test_publish_long_payload();
  test_subscribe();
  test_parse_acks();
  test_parse_publish();
  test_roundtrip();
  printf("mqtt_packet: %d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
