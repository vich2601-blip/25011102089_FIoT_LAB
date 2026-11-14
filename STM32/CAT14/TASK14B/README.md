# EXPERIMENT DETAILS

**EXP:** CONTINUOUS ASSESSMENT 14

**LAB SHEET:** FPIoT Lab Evaluation CAT13 (Week 14 session)

**DATE:** 27/10/2026

**NAME OF EXP:** BME280 Sensor Interfacing Using I²C and MQTT using Custom Built ESP8266 API

**HARDWARE SETUP / PLATFORM:** NUCLEO-F401RE (STM32F401RET6, Cortex-M4F, 84 MHz) + BME280 + Wemos D1 mini — STM32CubeMX + Keil MDK-ARM (STM32CubeIDE project also included), STM32 HAL, ESPLib API (reworked implementation), Node-RED, broker.emqx.io

---

## SUBSCRIBE EXAMPLE : LED Control

### 1. Circuit Diagram

Same wiring as CAT 13; Task 14B also uses LD2 (PA5). Broker **broker.emqx.io:1883**.

![Circuit Diagram](circuit.png)

*Drawn on [Wokwi](https://wokwi.com). To open and edit it: start a new project on wokwi.com, paste [`wokwi/diagram.json`](wokwi/diagram.json) into its diagram.json tab, and add the `wokwi/*.chip.json` files (if any) with **New file**. The AT89S52, USB-TTL, ULN2003, USBasp and ST-LINK boards are custom chips (pin names added as labels). The ESP-01 part stands in for the Wemos D1 mini and the BMP180 module for the BME280 (same AT firmware / same 4-pin I²C header); the Nucleo-64 graphic is labelled as the F401RE.*

![Data flow](dataflow.png)

### 2. Core Logic / Pseudocode / Code Base

**Working Principle:**

After CONNACK the board subscribes to `vishrudh/stm32/led` and checks the SUBACK. `MQTTDataHandler()` returns 1 when a complete, validated PUBLISH has arrived. Only the exact commands `ON` / `OFF` / `TOGGLE` act on LD2; anything else is ignored. PINGREQ every 60 s keeps the session alive; if the broker does not answer, the board reconnects and resubscribes. `MQTTTimer()` is still called from `SysTick_Handler` (the lab-sheet hook) but is no longer needed.

> **Security note:** broker.emqx.io is a public test broker — anyone who knows the topic can publish to it. For anything beyond a lab demo, use a private broker with a login: define `MQTT_USERNAME` / `MQTT_PASSWORD` in `wifi_secrets.h` and the client sends them in CONNECT.

**Pseudocode:**

1. LD2 init, `mqtt_start()`, `MQTT_subscribeTopic()`, `HAL_UART_Receive_IT()`.
2. Loop: `MQTTDataHandler()`; on a message print it and act on it.
3. Every 60 s `MQTT_pingReq()`.

**Code Base:**

- Project files: `Task 14B_MQTT_Subscribe_LED_Control.ioc` (CubeMX configuration), `MDK-ARM/Task 14B_MQTT_Subscribe_LED_Control.uvprojx` (Keil MDK), `.project` / `.cproject` (STM32CubeIDE); application code in `Core/Src/main.c`.
- `subscribe_commands()` — SUBSCRIBE + SUBACK check; reconnect + resubscribe in the main loop
- `handle_message()`

**Supporting notes:**

| ESPLib function | What it sends |
|---|---|
| `ESP8266_Init(StationMode, huart1)` | `AT`, `AT+CWMODE=1` |
| `ESP8266_wifiConnect()` | `AT+CWQAP`, `AT+CWJAP` |
| `ESP8266_portConnect("TCP", broker, "1883")` | `AT+CIPSTART` |
| `MQTT_connectBroker()` | CONNECT 0x10, "MQTT", level 4, clean session, keep-alive, client ID |
| `MQTT_publishTopic()` | PUBLISH 0x30, topic length, topic, payload |
| `MQTT_subscribeTopic()` | SUBSCRIBE 0x82 |

**Reworked library.** `ESPLib.h` keeps the course function names, so the lab-sheet code compiles unchanged, but `ESPLib.c` + `mqtt_packet.c` are a new implementation:

| Problem in the original | Now |
|---|---|
| RX interrupt aborted during every send, so packets arriving meanwhile were lost | RX interrupt armed once and never aborted; 1 KB ring buffer |
| Packet end guessed from 100 ms of UART silence; `colonIndex` could be used uninitialised (SUBACK / PINGRESP have no `:` structure) | `+IPD,<len>:` framing removed, then a byte-wise MQTT parser using the packet type and Remaining Length |
| 1-byte Remaining Length only, lengths and destination sizes not checked, strings not always terminated | Remaining Length up to 4 bytes, every length validated before copying, output always NUL-terminated, oversized packets skipped |
| CONNACK / SUBACK never checked; `do {} while` loops could hang forever | CONNECT waits for CONNACK = accepted, SUBSCRIBE for SUBACK ≠ 0x80; every wait has a timeout and returns `funcOk` / `funcErr` |

Also: PINGREQ waits for PINGRESP (no answer → reconnect), topics that do not fit the buffer are dropped and counted, and the applications print the RX-overflow / dropped-packet counters when they change.

The client is **QoS 0 only**: publish/subscribe calls asking for QoS 1/2 return `funcErr`, a SUBACK must grant QoS 0, and incoming QoS 1/2 messages (which would need PUBACK) are dropped and counted. UNSUBSCRIBE waits for the matching UNSUBACK.

If the UART ring buffer ever overflows, the buffered bytes are discarded, the parser is reset and both the TCP link and the MQTT session are marked down, so the board does a full reconnect instead of parsing a broken stream. A malformed `+IPD` frame takes the same full-recovery path, and if HAL ever refuses to re-arm UART reception the library retries from the main loop. CONNACK is also checked for the Session Present rules, and topics with wildcards are rejected.

Tested on the PC: `make -C tests` (54 parser checks + 56 ESPLib checks, including fault injection against a simulated ESP8266, including both `AT+CIPSEND` reply styles, packets split across and combined in TCP chunks, wrong ACK ids, a silent broker and an overflow in the middle of a packet).

### 3. Inference

The same library supports the reverse direction, so Node-RED or any client can control the board.
