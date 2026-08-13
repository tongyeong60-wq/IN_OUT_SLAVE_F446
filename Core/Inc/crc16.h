/*
 * crc16.h
 *
 *  Created on: 2026. 1. 16.
 *      Author: VIEW
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

uint16_t crc16_modbus(const uint8_t *data, size_t len);

#endif

