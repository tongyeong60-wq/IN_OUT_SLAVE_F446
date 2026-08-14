/*
 * rs485_if.c
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */


#include "rs485_if.h"
#include "ringbuf.h"
#include "crc16.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>

static UART_HandleTypeDef *s_uart = NULL;
static GPIO_TypeDef *s_de_port = NULL;
static uint16_t s_de_pin = 0;

static uint8_t s_rb_mem[512];
static ringbuf_t s_rb;
static volatile uint32_t s_uart_ore_count = 0;
static volatile uint32_t s_uart_fe_count = 0;
static volatile uint32_t s_uart_ne_count = 0;
static volatile uint32_t s_uart_pe_count = 0;
static volatile uint32_t s_uart_rx_rearm_fail_count = 0;
static volatile uint32_t s_uart_last_error = 0;
static volatile uint32_t s_tx_fail_count = 0;
static volatile uint32_t s_tx_tc_timeout_count = 0;
static volatile uint8_t s_uart_error_pending = 0;
static volatile uint8_t s_tx_error_pending = 0;

static void delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000u) * us;
    cycles /= 5u;
    while (cycles--) __NOP();
}

void rs485_if_init(UART_HandleTypeDef *huart, GPIO_TypeDef *de_port, uint16_t de_pin)
{
    s_uart = huart;
    s_de_port = de_port;
    s_de_pin = de_pin;
    ringbuf_init(&s_rb, s_rb_mem, sizeof(s_rb_mem));
}

void rs485_if_on_rx_isr(uint8_t b)
{
    (void)ringbuf_push_isr(&s_rb, b);
}

void rs485_if_on_uart_error_isr(uint32_t error_code)
{
    s_uart_last_error = error_code;
    if (error_code & HAL_UART_ERROR_ORE) s_uart_ore_count++;
    if (error_code & HAL_UART_ERROR_FE)  s_uart_fe_count++;
    if (error_code & HAL_UART_ERROR_NE)  s_uart_ne_count++;
    if (error_code & HAL_UART_ERROR_PE)  s_uart_pe_count++;
    s_uart_error_pending = 1;
    ringbuf_abort_frame_isr(&s_rb);
}

void rs485_if_on_rx_rearm_fail_isr(void)
{
    s_uart_rx_rearm_fail_count++;
    s_uart_error_pending = 1;
}

void rs485_if_get_diag(rs485_diag_t *diag, bool clear_pending)
{
    ringbuf_diag_t rb_diag;
    if (!diag) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    ringbuf_get_diag(&s_rb, &rb_diag, clear_pending);
    diag->rx_overrun_count = rb_diag.overrun_count;
    diag->rx_oversize_count = rb_diag.oversize_count;
    diag->rx_overrun_pending = rb_diag.overrun_pending;
    diag->rx_oversize_pending = rb_diag.oversize_pending;
    diag->uart_ore_count = s_uart_ore_count;
    diag->uart_fe_count = s_uart_fe_count;
    diag->uart_ne_count = s_uart_ne_count;
    diag->uart_pe_count = s_uart_pe_count;
    diag->uart_rx_rearm_fail_count = s_uart_rx_rearm_fail_count;
    diag->uart_last_error = s_uart_last_error;
    diag->tx_fail_count = s_tx_fail_count;
    diag->tx_tc_timeout_count = s_tx_tc_timeout_count;
    diag->uart_error_pending = s_uart_error_pending;
    diag->tx_error_pending = s_tx_error_pending;
    if (clear_pending) {
        uint32_t uart_total = diag->uart_ore_count + diag->uart_fe_count +
                              diag->uart_ne_count + diag->uart_pe_count +
                              diag->uart_rx_rearm_fail_count;
        uint32_t uart_now = s_uart_ore_count + s_uart_fe_count + s_uart_ne_count +
                            s_uart_pe_count + s_uart_rx_rearm_fail_count;
        uint32_t tx_total = diag->tx_fail_count + diag->tx_tc_timeout_count;
        uint32_t tx_now = s_tx_fail_count + s_tx_tc_timeout_count;
        if (uart_now == uart_total) s_uart_error_pending = 0;
        if (tx_now == tx_total) s_tx_error_pending = 0;
    }
    if (primask == 0u) __enable_irq();
}

static int hex4_to_u16(const char *p, uint16_t *out)
{
    uint16_t v = 0;
    for (int i = 0; i < 4; i++) {
        char c = p[i];
        uint8_t n;
        if (c >= '0' && c <= '9') n = (uint8_t)(c - '0');
        else if (c >= 'A' && c <= 'F') n = (uint8_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') n = (uint8_t)(c - 'a' + 10);
        else return -1;
        v = (uint16_t)((v << 4) | n);
    }
    *out = v;
    return 0;
}

static void u16_to_hex4(uint16_t v, char out[5])
{
    static const char *hex = "0123456789ABCDEF";
    out[0] = hex[(v >> 12) & 0xF];
    out[1] = hex[(v >>  8) & 0xF];
    out[2] = hex[(v >>  4) & 0xF];
    out[3] = hex[(v >>  0) & 0xF];
    out[4] = 0;
}

static rs485_parse_result_t parse_line(const char *line, rs485_req_t *out)
{
    // "@AA|SSSS|CMD|ARGS*CCCC"
    if (!line || line[0] != '@' || !out) return RS485_MSG_BADFMT;

    const char *star = strrchr(line, '*');
    if (!star || (star - line) < 1) return RS485_MSG_BADFMT;
    if (strlen(star) < 5) return RS485_MSG_BADFMT;

    uint16_t rx_crc = 0;
    if (hex4_to_u16(star + 1, &rx_crc) != 0) return RS485_MSG_BADFMT;

    uint16_t calc = crc16_modbus((const uint8_t*)line, (size_t)(star - line));
    if (calc != rx_crc) return RS485_MSG_BADCRC;

    char tmp[RS485_MAX_LINE];
    size_t n = (size_t)(star - line);
    if (n >= sizeof(tmp)) return RS485_MSG_BADFMT;
    memcpy(tmp, line, n);
    tmp[n] = 0;

    // tmp: "@AA|SSSS|CMD|ARGS"
    // 토큰 파싱( strsep 금지 )
    char *p = tmp + 1; // '@' skip

    char *t1 = strchr(p, '|'); if (!t1) return RS485_MSG_BADFMT; *t1++ = 0; // addr
    char *t2 = strchr(t1, '|'); if (!t2) return RS485_MSG_BADFMT; *t2++ = 0; // seq
    char *t3 = strchr(t2, '|'); // cmd
    char *args = NULL;
    if (t3) { *t3++ = 0; args = t3; }
    else { args = (char*)""; }

    // addr (2자리 10진 가정)
    for (size_t i = 0; p[i]; i++) if (!isdigit((unsigned char)p[i])) return RS485_MSG_BADFMT;
    int aa = atoi(p);
    if (aa < 0 || aa > 255) return RS485_MSG_BADFMT;

    // seq (4자리 10진 가정)
    for (size_t i = 0; t1[i]; i++) if (!isdigit((unsigned char)t1[i])) return RS485_MSG_BADFMT;
    int ss = atoi(t1);
    if (ss < 0 || ss > 9999) return RS485_MSG_BADFMT;

    memset(out, 0, sizeof(*out));
    out->addr = (uint8_t)aa;
    out->seq  = (uint16_t)ss;
    strncpy(out->cmd, t2, sizeof(out->cmd)-1);
    if (args) strncpy(out->args, args, sizeof(out->args)-1);

    return RS485_MSG_OK;
}

bool rs485_if_poll(rs485_req_t *out, rs485_parse_result_t *res)
{
  char line[RS485_MAX_LINE];
  if (!ringbuf_readline(&s_rb, line, sizeof(line))) return false;

  rs485_parse_result_t r = parse_line(line, out);
  if (res) *res = r;

  /* 타 주소 프레임까지 여기서 찍지 않음 */
  if (r == RS485_MSG_BADCRC) log_printf("[RS485] RX bad CRC\r\n");
  else if (r == RS485_MSG_BADFMT) log_printf("[RS485] RX bad FMT\r\n");

  return true;
}

bool rs485_if_send(uint8_t addr, uint16_t seq, const char *cmd, const char *args)
{
    if (!s_uart || !s_de_port || !cmd) return false;
    if (!args) args = "";

    char core[RS485_MAX_LINE];
    int n = snprintf(core, sizeof(core), "@%02u|%04u|%s|%s",
                     (unsigned)addr, (unsigned)seq, cmd, args);
    if (n <= 0 || n >= (int)sizeof(core)) return false;

    uint16_t crc = crc16_modbus((const uint8_t*)core, (size_t)n);
    char crc_hex[5];
    u16_to_hex4(crc, crc_hex);

    char frame[RS485_MAX_LINE];
    n = snprintf(frame, sizeof(frame), "%s*%s\n", core, crc_hex);
    if (n <= 0 || n >= (int)sizeof(frame)) return false;

    // 송신(EN=1) -> TX -> TC -> EN=0
    HAL_GPIO_WritePin(s_de_port, s_de_pin, GPIO_PIN_SET);
    delay_us(50);

    HAL_StatusTypeDef st = HAL_UART_Transmit(s_uart, (uint8_t*)frame, (uint16_t)strlen(frame), 200);

    if (st != HAL_OK) {
        s_tx_fail_count++;
        s_tx_error_pending = 1;
        HAL_GPIO_WritePin(s_de_port, s_de_pin, GPIO_PIN_RESET);
        return false;
    }

    const uint32_t tc_timeout_ms = 10u;
    const uint32_t tc_start_ms = HAL_GetTick();
    bool tc_complete = false;
    while (__HAL_UART_GET_FLAG(s_uart, UART_FLAG_TC) == RESET) {
        if ((HAL_GetTick() - tc_start_ms) >= tc_timeout_ms) break;
    }
    if (__HAL_UART_GET_FLAG(s_uart, UART_FLAG_TC) != RESET) tc_complete = true;
    if (!tc_complete) {
        s_tx_tc_timeout_count++;
        s_tx_error_pending = 1;
    }

    HAL_GPIO_WritePin(s_de_port, s_de_pin, GPIO_PIN_RESET);

    log_printf("[RS485] TX %s", frame);
    return tc_complete;
}
