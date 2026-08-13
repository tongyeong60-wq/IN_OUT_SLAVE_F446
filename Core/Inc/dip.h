/*
 * dip.h
 ********
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */

#ifndef DIP_H
#define DIP_H

#include <stdint.h>

// raw: DIP 입력 상태 그대로(1=HIGH, 0=LOW)
// addr: 정책 적용된 주소(0~15)
typedef struct {
    uint8_t raw;   // bit0=DIP0 ... bit3=DIP3
    uint8_t addr;  // 0..15
} dip_state_t;

// DIP 핀을 읽어서 raw/addr 계산
dip_state_t dip_read(void);

#endif

