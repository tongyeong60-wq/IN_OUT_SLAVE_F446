/*
 * dip.c
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */


#include "dip.h"
#include "io_map.h"
#include "stm32f4xx_hal.h"

// DIP은 Pull-up 구성(OFF=1, ON=0, ON이 GND로 떨어짐) 전제.
// 사람이 보는 주소는 보통 "스위치 ON이 1"로 느끼므로 반전해서 addr을 만든다.
// addr = (~raw) & 0x0F
dip_state_t dip_read(void)
{
    dip_state_t s;
    s.raw = 0;

    // bit0..3 구성
    if (HAL_GPIO_ReadPin(DIP0_PORT, DIP0_PIN) == GPIO_PIN_SET) s.raw |= (1u << 0);
    if (HAL_GPIO_ReadPin(DIP1_PORT, DIP1_PIN) == GPIO_PIN_SET) s.raw |= (1u << 1);
    if (HAL_GPIO_ReadPin(DIP2_PORT, DIP2_PIN) == GPIO_PIN_SET) s.raw |= (1u << 2);
    if (HAL_GPIO_ReadPin(DIP3_PORT, DIP3_PIN) == GPIO_PIN_SET) s.raw |= (1u << 3);

    s.addr = (uint8_t)((~s.raw) & 0x0Fu); // ON(0) -> 1로 해석

    return s;
}
