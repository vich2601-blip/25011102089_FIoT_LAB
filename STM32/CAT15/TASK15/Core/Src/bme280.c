/*
 * bme280.c - Simple BME280 driver (I2C, HAL)
 * Compensation formulas are from the Bosch BME280 datasheet (integer version).
 */
#include "bme280.h"

#define REG_ID          0xD0
#define REG_RESET       0xE0
#define REG_CTRL_HUM    0xF2
#define REG_CTRL_MEAS   0xF4
#define REG_CONFIG      0xF5
#define REG_DATA        0xF7   /* 8 bytes: press[3], temp[3], hum[2] */
#define REG_CALIB_00    0x88   /* 26 bytes */
#define REG_CALIB_26    0xE1   /* 7 bytes  */

#define I2C_TIMEOUT     100

static I2C_HandleTypeDef *bme_i2c;
uint8_t chip_id;               /* global for debugger */

/* Calibration values */
static uint16_t dig_T1;
static int16_t  dig_T2, dig_T3;
static uint16_t dig_P1;
static int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t  dig_H1, dig_H3;
static int16_t  dig_H2, dig_H4, dig_H5;
static int8_t   dig_H6;

static int32_t t_fine;

static HAL_StatusTypeDef read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(bme_i2c, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, I2C_TIMEOUT);
}

static HAL_StatusTypeDef write_reg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(bme_i2c, BME280_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                             &value, 1, I2C_TIMEOUT);
}

static HAL_StatusTypeDef read_calibration(void)
{
    uint8_t c[26];
    uint8_t h[7];

    if (read_regs(REG_CALIB_00, c, 26) != HAL_OK) return HAL_ERROR;

    dig_T1 = (uint16_t)(c[1] << 8 | c[0]);
    dig_T2 = (int16_t)(c[3] << 8 | c[2]);
    dig_T3 = (int16_t)(c[5] << 8 | c[4]);
    dig_P1 = (uint16_t)(c[7] << 8 | c[6]);
    dig_P2 = (int16_t)(c[9] << 8 | c[8]);
    dig_P3 = (int16_t)(c[11] << 8 | c[10]);
    dig_P4 = (int16_t)(c[13] << 8 | c[12]);
    dig_P5 = (int16_t)(c[15] << 8 | c[14]);
    dig_P6 = (int16_t)(c[17] << 8 | c[16]);
    dig_P7 = (int16_t)(c[19] << 8 | c[18]);
    dig_P8 = (int16_t)(c[21] << 8 | c[20]);
    dig_P9 = (int16_t)(c[23] << 8 | c[22]);
    dig_H1 = c[25];

    if (chip_id == BME280_CHIP_ID)
    {
        if (read_regs(REG_CALIB_26, h, 7) != HAL_OK) return HAL_ERROR;

        dig_H2 = (int16_t)(h[1] << 8 | h[0]);
        dig_H3 = h[2];
        dig_H4 = (int16_t)((int8_t)h[3] * 16 | (h[4] & 0x0F));
        dig_H5 = (int16_t)((int8_t)h[5] * 16 | (h[4] >> 4));
        dig_H6 = (int8_t)h[6];
    }
    return HAL_OK;
}

uint8_t BME280_Init(I2C_HandleTypeDef *hi2c)
{
    bme_i2c = hi2c;
    chip_id = 0;

    if (read_regs(REG_ID, &chip_id, 1) != HAL_OK) return 0;
    if (chip_id != BME280_CHIP_ID && chip_id != BMP280_CHIP_ID) return 0;

    write_reg(REG_RESET, 0xB6);   /* soft reset */
    HAL_Delay(10);

    if (read_calibration() != HAL_OK) return 0;

    /* Humidity oversampling x1 (must be written before CTRL_MEAS) */
    if (chip_id == BME280_CHIP_ID) write_reg(REG_CTRL_HUM, 0x01);

    /* Standby 1000 ms, filter off */
    write_reg(REG_CONFIG, 0xA0);

    /* Temp x1, Pressure x1, Normal mode */
    if (write_reg(REG_CTRL_MEAS, 0x27) != HAL_OK) return 0;

    HAL_Delay(10);
    return chip_id;
}

static int32_t compensate_temp(int32_t adc_T)
{
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * (int32_t)dig_T2) >> 11;
    var2 = (((((adc_T >> 4) - (int32_t)dig_T1) * ((adc_T >> 4) - (int32_t)dig_T1)) >> 12)
            * (int32_t)dig_T3) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_press(int32_t adc_P)
{
    int64_t var1, var2, p;
    var1 = (int64_t)t_fine - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (uint32_t)(p / 256);   /* Pa */
}

static uint32_t compensate_hum(int32_t adc_H)
{
    int32_t v;
    v = t_fine - 76800;
    v = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v))
           + 16384) >> 15)
         * (((((((v * (int32_t)dig_H6) >> 10)
                * (((v * (int32_t)dig_H3) >> 11) + 32768)) >> 10) + 2097152)
             * (int32_t)dig_H2 + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)dig_H1) >> 4);
    if (v < 0) v = 0;
    if (v > 419430400) v = 419430400;
    /* (v >> 12) is %RH * 1024 -> convert to 0.01 %RH */
    return (uint32_t)(((v >> 12) * 100) / 1024);
}

HAL_StatusTypeDef BME280_Read(BME280_Data *data)
{
    uint8_t d[8];
    int32_t adc_P, adc_T, adc_H;

    if (chip_id == 0) return HAL_ERROR;
    if (read_regs(REG_DATA, d, 8) != HAL_OK) return HAL_ERROR;

    adc_P = (int32_t)((d[0] << 12) | (d[1] << 4) | (d[2] >> 4));
    adc_T = (int32_t)((d[3] << 12) | (d[4] << 4) | (d[5] >> 4));
    adc_H = (int32_t)((d[6] << 8) | d[7]);

    data->temperature = compensate_temp(adc_T);   /* must be first (sets t_fine) */
    data->pressure    = compensate_press(adc_P);
    data->humidity    = (chip_id == BME280_CHIP_ID) ? compensate_hum(adc_H) : 0;

    return HAL_OK;
}
