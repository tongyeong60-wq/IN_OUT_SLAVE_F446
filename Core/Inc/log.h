/*
 * log.h
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */

#ifndef LOG_H
#define LOG_H

#include "stm32f4xx_hal.h"

void log_init(UART_HandleTypeDef *huart);
void log_printf(const char *fmt, ...);

#endif
