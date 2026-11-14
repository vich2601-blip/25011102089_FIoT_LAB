/*
 * bme280.h - Simple BME280 driver (I2C, HAL)
 */
#ifndef BME280_H
#define BME280_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* I2C address: SDO -> GND = 0x76, SDO -> 3.3V = 0x77 */
#define BME280_ADDR   (0x76 << 1)

#define BME280_CHIP_ID   0x60   /* BME280 */
#define BMP280_CHIP_ID   0x58   /* BMP280 (no humidity) */

typedef struct {
    int32_t  temperature;   /* in 0.01 degC   (2534 = 25.34 C) */
    uint32_t pressure;      /* in Pa          (101325 = 1013.25 hPa) */
    uint32_t humidity;      /* in 0.01 %RH    (4567 = 45.67 %) */
} BME280_Data;

/* Returns the chip ID on success (0x60 or 0x58), 0 on failure */
uint8_t BME280_Init(I2C_HandleTypeDef *hi2c);

/* Returns HAL_OK on success */
HAL_StatusTypeDef BME280_Read(BME280_Data *data);

#endif /* BME280_H */
