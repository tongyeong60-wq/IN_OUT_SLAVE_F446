/*
 * rs485_if.h
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */
#ifndef RS485_IF_H
#define RS485_IF_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define RS485_MAX_LINE 256

typedef enum {
    RS485_MSG_NONE = 0,
    RS485_MSG_OK,      // 정상 파싱됨
    RS485_MSG_BADCRC,  // CRC 오류
    RS485_MSG_BADFMT   // 포맷 오류
} rs485_parse_result_t;

typedef struct {
    uint8_t  addr;      // 수신된 목적지 주소(AA)
    uint16_t seq;       // 수신된 seq(SSSS)
    char     cmd[16];   // "RUN" 등
    char     args[128]; // "K=...;V=..." 등
} rs485_req_t;

void rs485_if_init(UART_HandleTypeDef *huart, GPIO_TypeDef *de_port, uint16_t de_pin);
void rs485_if_on_rx_isr(uint8_t b);

// 한 줄(@... \n) 단위로 읽어서 파싱. true면 out에 유효 데이터(OK/에러 포함)
bool rs485_if_poll(rs485_req_t *out, rs485_parse_result_t *res);

// 송신: @AA|SSSS|CMD|ARGS*CCCC\n
bool rs485_if_send(uint8_t addr, uint16_t seq, const char *cmd, const char *args);

#endif
