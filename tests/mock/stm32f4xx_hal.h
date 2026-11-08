/* Minimal STM32 HAL stand-in so ESPLib.c can be unit-tested on a PC. */
#ifndef MOCK_STM32F4XX_HAL_H
#define MOCK_STM32F4XX_HAL_H

#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int id; } UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
uint32_t HAL_GetTick(void);
void     HAL_Delay(uint32_t ms);
#define  __HAL_UART_CLEAR_OREFLAG(h) ((void)(h))

/* provided by ESPLib.c */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif
