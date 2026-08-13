/*
 * seg5955.c
 *
 *  Created on: 2026. 4. 6.
 *      Author: VIEW
 */


#include "seg595.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* =========================
 * module option
 * 0 = common cathode style pattern
 * 1 = common anode style pattern (invert)
 * ========================= */
#define SEG595_COMMON_ANODE      1

/* =========================
 * digit byte order
 * 0 = first byte:tens, second byte:ones
 * 1 = first byte:ones, second byte:tens
 * ========================= */
#define SEG595_SWAP_DIGIT_ORDER  1

static void seg595_pin_write(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState st)
{
  HAL_GPIO_WritePin(port, pin, st);
}

static void seg595_delay(void)
{
  __NOP();
  __NOP();
  __NOP();
  __NOP();
}

static void seg595_clk_pulse(void)
{
  seg595_pin_write(SCLK_GPIO_Port, SCLK_Pin, GPIO_PIN_SET);
  seg595_delay();
  seg595_pin_write(SCLK_GPIO_Port, SCLK_Pin, GPIO_PIN_RESET);
  seg595_delay();
}

static void seg595_load_pulse(void)
{
  seg595_pin_write(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_SET);
  seg595_delay();
  seg595_pin_write(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_RESET);
  seg595_delay();
}

static void seg595_shift_byte(uint8_t data)
{
  for (int i = 0; i < 8; i++) {
    GPIO_PinState st = (data & 0x80u) ? GPIO_PIN_SET : GPIO_PIN_RESET;
    seg595_pin_write(SDI_GPIO_Port, SDI_Pin, st);
    seg595_delay();
    seg595_clk_pulse();
    data <<= 1;
  }
}

static uint8_t seg595_encode_digit(uint8_t n)
{
  /* bit: gfedcba 기준이 아니라 일반적인 0x3F~ 패턴 사용
   * 모듈에 따라 순서가 다르면 여기만 수정하면 됨
   */
  static const uint8_t cc_map[10] = {
    0x3F, /* 0 */
    0x06, /* 1 */
    0x5B, /* 2 */
    0x4F, /* 3 */
    0x66, /* 4 */
    0x6D, /* 5 */
    0x7D, /* 6 */
    0x07, /* 7 */
    0x7F, /* 8 */
    0x6F  /* 9 */
  };

  uint8_t v = 0x00;
  if (n < 10) v = cc_map[n];

#if SEG595_COMMON_ANODE
  v = (uint8_t)~v;
#endif

  return v;
}

static void seg595_show_raw(uint8_t left, uint8_t right)
{
#if SEG595_SWAP_DIGIT_ORDER
  seg595_shift_byte(right);
  seg595_shift_byte(left);
#else
  seg595_shift_byte(left);
  seg595_shift_byte(right);
#endif
  seg595_load_pulse();

  /* 유휴 상태 고정 */
  seg595_pin_write(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_RESET);
  seg595_pin_write(SCLK_GPIO_Port, SCLK_Pin, GPIO_PIN_RESET);
  seg595_pin_write(SDI_GPIO_Port,  SDI_Pin,  GPIO_PIN_RESET);
}

void seg595_init(void)
{
  seg595_pin_write(LOAD_GPIO_Port, LOAD_Pin, GPIO_PIN_RESET);
  seg595_pin_write(SCLK_GPIO_Port, SCLK_Pin, GPIO_PIN_RESET);
  seg595_pin_write(SDI_GPIO_Port,  SDI_Pin,  GPIO_PIN_RESET);

  seg595_blank();
}

void seg595_blank(void)
{
#if SEG595_COMMON_ANODE
  seg595_show_raw(0xFF, 0xFF);
#else
  seg595_show_raw(0x00, 0x00);
#endif
}

void seg595_show_u8(uint8_t value)
{
  uint8_t tens = (uint8_t)((value / 10u) % 10u);
  uint8_t ones = (uint8_t)(value % 10u);

  uint8_t left  = seg595_encode_digit(tens);
  uint8_t right = seg595_encode_digit(ones);

  seg595_show_raw(left, right);
}
