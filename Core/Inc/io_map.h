/*
 * io_map.h
 ****
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */

#ifndef IO_MAP_H
#define IO_MAP_H

#include "main.h"  // CubeMX가 생성한 *_Pin, *_GPIO_Port 사용

// ===== DIP (PC0~PC3) =====
// CubeMX User Label을 DIP0~DIP3로 했다는 전제
#define DIP0_PORT   DIP0_GPIO_Port
#define DIP0_PIN    DIP0_Pin

#define DIP1_PORT   DIP1_GPIO_Port
#define DIP1_PIN    DIP1_Pin

#define DIP2_PORT   DIP2_GPIO_Port
#define DIP2_PIN    DIP2_Pin

#define DIP3_PORT   DIP3_GPIO_Port
#define DIP3_PIN    DIP3_Pin

#endif

