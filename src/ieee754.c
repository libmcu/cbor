/*
 * | precision | sign | exponent | mantissa | bias |
 * | --------- | ---- | -------- | -------- | ---- |
 * | half      | 1    |  5       | 10       |   15 |
 * | single    | 1    |  8       | 23       |  127 |
 * | double    | 1    | 11       | 52       | 1023 |
 *
 * ## Secial cases
 * | s |   e  | m  |     desc.     |
 * | - | -----| -- | ------------- |
 * | 0 |    0 |  0 | +0.0          |
 * | 1 |    0 |  0 | -0.0          |
 * | 0 |    0 | !0 | 0.m * 2^-126  |
 * | 1 |    0 | !0 | -0.m * 2^-126 |
 * | 0 | 0xff |  0 | infinity      |
 * | 1 | 0xff |  0 | -infinity     |
 * | X | 0xff | -1 | Quiet NaN     |
 * | X | 0xff | !0 | Signaling NaN |
 */

#include "cbor/ieee754.h"

#define BIAS_HALF				15
#define BIAS_SINGLE				127
#define BIAS_DOUBLE				1023

#define E_MASK_HALF				((1u << 5) - 1)
#define E_MASK_SINGLE				((1ul << 8) - 1)
#define E_MASK_DOUBLE				((1ul << 11) - 1)

#define M_BIT_HALF				10
#define M_BIT_SINGLE				23
#define M_BIT_DOUBLE				52

#define M_MASK_HALF				((1u << M_BIT_HALF) - 1)
#define M_MASK_SINGLE				((1ul << M_BIT_SINGLE) - 1)
//#define M_MASK_DOUBLE				((1ull << M_BIT_DOUBLE) - 1)

static bool is_in_range(unsigned int e, unsigned int f, unsigned int t)
{
	if (e <= (f + t) && e >= (f - t + 1)) {
		return true;
	}

	return false;
}

static bool is_precision_lost(uint64_t m, unsigned int f, unsigned int t)
{
	if ((m & ((1ull << (f - t)) - 1)) != 0) {
		return true;
	}

	return false;
}

uint16_t ieee754_convert_single_to_half(float value)
{
	ieee754_single_t single = { .value = value };
	ieee754_half_t half = { .value = 0 };

	half.components.sign = single.components.sign;
	if (single.components.e == E_MASK_SINGLE) { /* NaN or infinity */
		half.components.e = E_MASK_HALF;
	} else { /* normal and subnormal */
		if (single.components.e != 0) {
			half.components.e = (uint8_t)(single.components.e
				- BIAS_SINGLE + BIAS_HALF) & E_MASK_HALF;
		}
	}

	half.components.m = (single.components.m >> (M_BIT_SINGLE - M_BIT_HALF))
		& M_MASK_HALF;

	return half.value;
}

float ieee754_convert_half_to_single(uint16_t value)
{
	ieee754_half_t half = { .value = value };
	ieee754_single_t single;

	single.components.sign = half.components.sign;
	single.components.e = half.components.e;
	single.components.m = half.components.m;

	if (half.components.e == E_MASK_HALF) {
		single.components.e = E_MASK_SINGLE;
	}
	if (half.components.m == M_MASK_HALF) {
		single.components.m = M_MASK_SINGLE;
	}

	return single.value;
}

bool ieee754_is_shrinkable_to_half(float value)
{
	ieee754_single_t single = { .value = value };

	if (single.components.e == 0) {
		if (single.components.m == 0) { /* zero */
			return true;
		}
		/* subnormal */
		if (!is_precision_lost(single.components.m,
					M_BIT_SINGLE, M_BIT_HALF)) {
			return true;
		}
	} else if (single.components.e == E_MASK_SINGLE) { /* NaN or infinity */
		return true;
	} else if (is_in_range(single.components.e, BIAS_SINGLE, BIAS_HALF) &&
			!is_precision_lost(single.components.m, M_BIT_SINGLE,
				M_BIT_HALF)) {
		return true;
	}

	return false;
}

bool ieee754_is_shrinkable_to_single(double value)
{
	ieee754_double_t d = { .value = value };

	if (d.components.e == 0) {
		if (d.components.m == 0) { /* zero */
			return true;
		}
		/* subnormal */
		if (!is_precision_lost(d.components.m,
					M_BIT_DOUBLE, M_BIT_SINGLE)) {
			return true;
		}
	} else if (d.components.e == E_MASK_DOUBLE) { /* NaN or infinity */
		return true;
	} else if (is_in_range(d.components.e, BIAS_DOUBLE, BIAS_SINGLE) &&
			!is_precision_lost(d.components.m, M_BIT_DOUBLE,
				M_BIT_SINGLE)) {
		return true;
	}

	return false;
}
