/*
 * seg595.h
 *
 *  Created on: 2026. 4. 6.
 *      Author: VIEW
 */

#ifndef SEG595_H
#define SEG595_H

#include <stdint.h>

void seg595_init(void);
void seg595_blank(void);
void seg595_show_u8(uint8_t value);

#endif /* SEG595_H */

