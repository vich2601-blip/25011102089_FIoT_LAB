# Host-side unit tests (run on the PC, no hardware needed)
CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined
LIB      = ../Common_Libraries/ESPLib_MQTT

test: test_mqtt_packet test_esplib test_esp_at
	./test_mqtt_packet
	./test_esplib
	./test_esp_at

test_mqtt_packet: test_mqtt_packet.c $(LIB)/mqtt_packet.c $(LIB)/mqtt_packet.h
	$(CC) $(CFLAGS) -I$(LIB) test_mqtt_packet.c $(LIB)/mqtt_packet.c -o $@

# ESPLib.c compiled against a mock HAL and a simulated ESP8266 / broker
test_esplib: test_esplib.c mock/stm32f4xx_hal.h $(LIB)/ESPLib.c $(LIB)/ESPLib.h $(LIB)/mqtt_packet.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -Imock -I$(LIB) test_esplib.c $(LIB)/ESPLib.c $(LIB)/mqtt_packet.c -o $@

# esp_at.c (CAT 10D / 12B AT driver) against a simulated ESP8266
ATLIB    = ../Common_Libraries/ESP8266_AT_Driver
test_esp_at: test_esp_at.c mock/stm32f4xx_hal.h mock/main.h $(ATLIB)/esp_at.c $(ATLIB)/esp_at.h
	$(CC) $(CFLAGS) -Wno-unused-parameter -Imock -I$(ATLIB) test_esp_at.c $(ATLIB)/esp_at.c -o $@

clean:
	rm -f test_mqtt_packet test_esplib test_esp_at

.PHONY: test clean
