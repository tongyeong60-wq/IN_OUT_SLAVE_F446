#include "log.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>   // strlen

static UART_HandleTypeDef *s_huart = NULL;

void log_init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
}

void log_printf(const char *fmt, ...)
{
    if (!s_huart) return;

    char buf[256];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    HAL_UART_Transmit(s_huart, (uint8_t*)buf, (uint16_t)strlen(buf), 100);
}

