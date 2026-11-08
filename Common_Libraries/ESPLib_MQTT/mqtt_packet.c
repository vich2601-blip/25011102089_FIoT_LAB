/**
  ******************************************************************************
  * @file    mqtt_packet.c
  * @brief   MQTT 3.1.1 packet encoder / stream decoder - see mqtt_packet.h
  ******************************************************************************
  */
#include "mqtt_packet.h"
#include <string.h>

/* ========================================================================== */
/*  Encoding                                                                  */
/* ========================================================================== */
size_t mqtt_encode_remaining_length(uint8_t *out, uint32_t value)
{
  size_t n = 0;
  if (value > MQTT_MAX_REMAINING)
    return 0;
  do
  {
    uint8_t b = (uint8_t)(value % 128U);
    value /= 128U;
    if (value > 0U)
      b |= 0x80U;              /* continuation bit */
    out[n++] = b;
  } while (value > 0U);
  return n;
}

/* Writer that refuses to run past the end of the buffer */
typedef struct { uint8_t *buf; size_t size; size_t pos; int overflow; } writer_t;

static void put_u8(writer_t *w, uint8_t v)
{
  if (w->pos < w->size) w->buf[w->pos] = v; else w->overflow = 1;
  w->pos++;
}
static void put_u16(writer_t *w, uint16_t v) { put_u8(w, (uint8_t)(v >> 8)); put_u8(w, (uint8_t)v); }
static void put_bytes(writer_t *w, const uint8_t *d, size_t n) { for (size_t i = 0; i < n; i++) put_u8(w, d[i]); }
static void put_str(writer_t *w, const char *s)          /* 2-byte length + UTF-8 bytes */
{
  size_t n = strlen(s);
  put_u16(w, (uint16_t)n);
  put_bytes(w, (const uint8_t *)s, n);
}

/* header byte + remaining length + body (body already measured) */
static int put_header(writer_t *w, uint8_t first, size_t body_len)
{
  uint8_t rl[4];
  size_t n = mqtt_encode_remaining_length(rl, (uint32_t)body_len);
  if (n == 0 || body_len > MQTT_MAX_REMAINING) return 0;
  put_u8(w, first);
  put_bytes(w, rl, n);
  return 1;
}

size_t mqtt_build_connect(uint8_t *buf, size_t size, const char *client_id,
                          const char *username, const char *password,
                          uint16_t keep_alive_s, int clean_session)
{
  writer_t w = { buf, size, 0, 0 };
  if (buf == NULL || client_id == NULL) return 0;
  uint8_t flags = clean_session ? 0x02U : 0x00U;
  size_t body = 10U + 2U + strlen(client_id);          /* variable header + client id */
  if (username && *username) { flags |= 0x80U; body += 2U + strlen(username); }
  if (password && *password) { flags |= 0x40U; body += 2U + strlen(password); }
  if (strlen(client_id) > 23U && !clean_session) return 0;  /* MQTT 3.1.1 rule */
  if ((flags & 0x40U) && !(flags & 0x80U)) return 0;         /* password requires a username (3.1.2.9) */

  if (!put_header(&w, (uint8_t)(MQTT_PKT_CONNECT << 4), body)) return 0;
  put_str(&w, "MQTT");          /* protocol name  */
  put_u8(&w, 0x04U);            /* level 4 = 3.1.1 */
  put_u8(&w, flags);
  put_u16(&w, keep_alive_s);
  put_str(&w, client_id);
  if (flags & 0x80U) put_str(&w, username);
  if (flags & 0x40U) put_str(&w, password);
  return w.overflow ? 0 : w.pos;
}

size_t mqtt_build_publish(uint8_t *buf, size_t size, const char *topic,
                          const uint8_t *payload, size_t payload_len, int retain)
{
  writer_t w = { buf, size, 0, 0 };
  if (buf == NULL || topic == NULL || (payload == NULL && payload_len > 0U)) return 0;
  size_t tlen = strlen(topic);
  if (tlen == 0U || tlen > 65535U) return 0;
  if (strpbrk(topic, "+#") != NULL) return 0;        /* wildcards are not allowed in a topic name */
  if (!put_header(&w, (uint8_t)((MQTT_PKT_PUBLISH << 4) | (retain ? 1U : 0U)), 2U + tlen + payload_len)) return 0;
  put_str(&w, topic);
  put_bytes(&w, payload, payload_len);   /* QoS 0: no packet identifier */
  return w.overflow ? 0 : w.pos;
}

size_t mqtt_build_subscribe(uint8_t *buf, size_t size, uint16_t packet_id,
                            const char *topic, uint8_t qos)
{
  writer_t w = { buf, size, 0, 0 };
  if (buf == NULL || topic == NULL) return 0;
  size_t tlen = strlen(topic);
  if (tlen == 0U || packet_id == 0U || qos > 2U) return 0;
  if (!put_header(&w, (uint8_t)((MQTT_PKT_SUBSCRIBE << 4) | 0x02U), 2U + 2U + tlen + 1U)) return 0;
  put_u16(&w, packet_id);
  put_str(&w, topic);
  put_u8(&w, qos);
  return w.overflow ? 0 : w.pos;
}

size_t mqtt_build_unsubscribe(uint8_t *buf, size_t size, uint16_t packet_id, const char *topic)
{
  writer_t w = { buf, size, 0, 0 };
  if (buf == NULL || topic == NULL) return 0;
  size_t tlen = strlen(topic);
  if (tlen == 0U || packet_id == 0U) return 0;
  if (!put_header(&w, (uint8_t)((MQTT_PKT_UNSUBSCRIBE << 4) | 0x02U), 2U + 2U + tlen)) return 0;
  put_u16(&w, packet_id);
  put_str(&w, topic);
  return w.overflow ? 0 : w.pos;
}

size_t mqtt_build_pingreq(uint8_t *buf, size_t size)
{
  if (size < 2U) return 0;
  buf[0] = (uint8_t)(MQTT_PKT_PINGREQ << 4); buf[1] = 0;
  return 2;
}

size_t mqtt_build_disconnect(uint8_t *buf, size_t size)
{
  if (size < 2U) return 0;
  buf[0] = (uint8_t)(MQTT_PKT_DISCONNECT << 4); buf[1] = 0;
  return 2;
}

/* ========================================================================== */
/*  Decoding                                                                  */
/* ========================================================================== */
enum { ST_HEADER = 0, ST_LENGTH, ST_BODY };

void mqtt_parser_init(mqtt_parser_t *p)
{
  if (p == NULL) return;
  memset(p, 0, sizeof(*p));
  p->state = ST_HEADER;
  p->topic = p->topic_buf;
  p->payload = p->payload_buf;
}

/* Interpret one complete packet held in p->body[0..remaining) */
static mqtt_event_t decode_packet(mqtt_parser_t *p)
{
  const uint8_t type = p->header >> 4;
  const uint8_t *b = p->body;
  const uint32_t n = p->remaining;

  p->code = 0; p->id = 0; p->qos = 0; p->session_present = 0;

  /* fixed-header flags are fixed for every type except PUBLISH:
     0010 for PUBREL / SUBSCRIBE / UNSUBSCRIBE, 0000 for the rest */
  if (type != MQTT_PKT_PUBLISH)
  {
    uint8_t want = (type == 6U || type == MQTT_PKT_SUBSCRIBE || type == MQTT_PKT_UNSUBSCRIBE) ? 0x02U : 0x00U;
    if ((p->header & 0x0FU) != want) return MQTT_EVT_ERROR;
  }
  p->topic_buf[0] = '\0'; p->payload_buf[0] = 0; p->payload_len = 0;

  switch (type)
  {
    case MQTT_PKT_CONNACK:                               /* 0x20 0x02 flags code */
      if (p->header != 0x20U || n != 2U) return MQTT_EVT_ERROR;
      if ((b[0] & 0xFEU) != 0U || b[1] > 5U) return MQTT_EVT_ERROR;   /* reserved bits / unknown code */
      if (b[1] != 0U && (b[0] & 0x01U)) return MQTT_EVT_ERROR;       /* refused => Session Present must be 0 */
      p->session_present = b[0] & 0x01U;
      p->code = b[1];                                   /* 0 = connection accepted */
      return MQTT_EVT_CONNACK;

    case MQTT_PKT_SUBACK:                                /* 0x90 0x03 id_hi id_lo code */
      if (p->header != 0x90U || n != 3U) return MQTT_EVT_ERROR;      /* single-topic API: exactly one code */
      p->id = (uint16_t)((b[0] << 8) | b[1]);
      if (p->id == 0U) return MQTT_EVT_ERROR;           /* packet identifier 0 is illegal */
      for (uint32_t i = 2; i < n; i++)
        if (b[i] > 2U && b[i] != 0x80U) return MQTT_EVT_ERROR;       /* only 0, 1, 2 or 0x80 */
      p->code = b[2];                                   /* granted QoS or 0x80 */
      return MQTT_EVT_SUBACK;

    case MQTT_PKT_UNSUBACK:                              /* 0xB0 0x02 id_hi id_lo */
      if (p->header != 0xB0U || n != 2U) return MQTT_EVT_ERROR;
      p->id = (uint16_t)((b[0] << 8) | b[1]);
      return (p->id != 0U) ? MQTT_EVT_UNSUBACK : MQTT_EVT_ERROR;

    case MQTT_PKT_PINGRESP:
      return (p->header == 0xD0U && n == 0U) ? MQTT_EVT_PINGRESP : MQTT_EVT_ERROR;

    case MQTT_PKT_PUBLISH:
    {
      uint8_t qos = (p->header >> 1) & 0x03U;
      if (qos == 3U || n < 2U) return MQTT_EVT_ERROR;
      if (qos == 0U && (p->header & 0x08U)) return MQTT_EVT_ERROR;   /* DUP must be 0 for QoS 0 */
      uint32_t tlen = ((uint32_t)b[0] << 8) | b[1];
      uint32_t pos = 2U + tlen;
      if (tlen == 0U || pos > n) return MQTT_EVT_ERROR;            /* topic must fit the packet */
      if (qos > 0U)
      {
        if (pos + 2U > n) return MQTT_EVT_ERROR;
        p->id = (uint16_t)((b[pos] << 8) | b[pos + 1U]);
        if (p->id == 0U) return MQTT_EVT_ERROR;                  /* QoS 1/2 need a non-zero id */
        pos += 2U;
      }
      if (tlen >= sizeof(p->topic_buf)) return MQTT_EVT_ERROR;     /* destination too small */
      if (memchr(&b[2], '+', tlen) || memchr(&b[2], '#', tlen) || memchr(&b[2], '\0', tlen))
        return MQTT_EVT_ERROR;                                       /* invalid topic name */
      memcpy(p->topic_buf, &b[2], tlen);
      p->topic_buf[tlen] = '\0';

      size_t plen = n - pos;
      if (plen >= sizeof(p->payload_buf)) plen = sizeof(p->payload_buf) - 1U;   /* truncate, flagged below */
      memcpy(p->payload_buf, &b[pos], plen);
      p->payload_buf[plen] = 0;
      p->payload_len = plen;
      p->code = (plen != n - pos) ? 1U : 0U;                       /* 1 = payload was truncated */
      p->qos = qos;
      return MQTT_EVT_PUBLISH;
    }

    default:
      return MQTT_EVT_OTHER;
  }
}

mqtt_event_t mqtt_parser_feed(mqtt_parser_t *p, uint8_t byte)
{
  if (p == NULL) return MQTT_EVT_ERROR;
  switch (p->state)
  {
    case ST_HEADER:
      if ((byte >> 4) == 0U || (byte >> 4) == 15U)   /* reserved types: not an MQTT header, resync */
        return MQTT_EVT_NONE;
      p->header = byte; p->remaining = 0; p->multiplier = 1; p->len_bytes = 0;
      p->received = 0; p->oversized = 0;
      p->state = ST_LENGTH;
      return MQTT_EVT_NONE;

    case ST_LENGTH:
      p->remaining += (uint32_t)(byte & 0x7FU) * p->multiplier;
      p->multiplier *= 128U;
      p->len_bytes++;
      if (byte & 0x80U)
      {
        if (p->len_bytes >= 4U) { p->state = ST_HEADER; return MQTT_EVT_ERROR; }   /* >4 length bytes */
        return MQTT_EVT_NONE;
      }
      if (p->len_bytes > 1U && byte == 0U)        /* e.g. 0x80 0x00: value encoded in more bytes than needed */
      {
        p->state = ST_HEADER;
        return MQTT_EVT_ERROR;
      }
      p->oversized = (p->remaining > MQTT_RX_PACKET_MAX);
      if (p->remaining == 0U) { p->state = ST_HEADER; return decode_packet(p); }
      p->state = ST_BODY;
      return MQTT_EVT_NONE;

    case ST_BODY:
      if (!p->oversized) p->body[p->received] = byte;   /* oversized packets are counted, not stored */
      p->received++;
      if (p->received < p->remaining) return MQTT_EVT_NONE;
      p->state = ST_HEADER;
      return p->oversized ? MQTT_EVT_ERROR : decode_packet(p);

    default:
      p->state = ST_HEADER;
      return MQTT_EVT_NONE;
  }
}
