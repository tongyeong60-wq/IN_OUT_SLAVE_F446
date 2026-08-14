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

typedef struct {
    uint32_t rx_overrun_count;
    uint32_t rx_oversize_count;
    uint32_t uart_ore_count;
    uint32_t uart_fe_count;
    uint32_t uart_ne_count;
    uint32_t uart_pe_count;
    uint32_t uart_rx_rearm_fail_count;
    uint32_t tx_fail_count;
    uint32_t tx_tc_timeout_count;
    uint32_t uart_last_error;
    uint8_t rx_overrun_pending;
    uint8_t rx_oversize_pending;
    uint8_t uart_error_pending;
    uint8_t tx_error_pending;
} rs485_diag_t;

void rs485_if_init(UART_HandleTypeDef *huart, GPIO_TypeDef *de_port, uint16_t de_pin);
void rs485_if_on_rx_isr(uint8_t b);
void rs485_if_on_uart_error_isr(uint32_t error_code);
void rs485_if_on_rx_rearm_fail_isr(void);
void rs485_if_get_diag(rs485_diag_t *diag, bool clear_pending);

// 한 줄(@... \n) 단위로 읽어서 파싱. true면 out에 유효 데이터(OK/에러 포함)
bool rs485_if_poll(rs485_req_t *out, rs485_parse_result_t *res);

// 송신: @AA|SSSS|CMD|ARGS*CCCC\n
bool rs485_if_send(uint8_t addr, uint16_t seq, const char *cmd, const char *args);

#endif
