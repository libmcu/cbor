/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_IEEE754_H
#define CBOR_IEEE754_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef union {
	uint16_t value;
	struct {
		uint32_t m: 10;
		uint32_t e: 5;
		uint32_t sign: 1;
	} components;
} ieee754_half_t;

typedef union {
	float value;
	struct {
		uint32_t m: 23;
		uint32_t e: 8;
		uint32_t sign: 1;
	} components;
} ieee754_single_t;

typedef union {
	double value;
	struct {
		uint64_t m: 52;
		uint64_t e: 11;
		uint64_t sign: 1;
	} components;
} ieee754_double_t;

uint16_t ieee754_convert_single_to_half(float value);
double ieee754_convert_half_to_double(uint16_t value);

bool ieee754_is_shrinkable_to_half(float value);
bool ieee754_is_shrinkable_to_single(double value);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_IEEE754_H */
