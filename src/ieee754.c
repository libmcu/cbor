/*
 * | precision | sign | exponent | mantissa | bias |   exp range  |
 * | --------- | ---- | -------- | -------- | ---- | ------------ |
 * | half      | 1    |  5       | 10       |   15 |   -14 ~   15 |
 * | single    | 1    |  8       | 23       |  127 |  -126 ~  127 |
 * | double    | 1    | 11       | 52       | 1023 | -1022 ~ 1023 |
 *
 * ## Special cases
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
 *
 * ## Terms
 * - f: source bias
 * - t: target bias
 * - e: exponent
 * - m: mantissa
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
#define M_MASK_DOUBLE				((1ull << M_BIT_DOUBLE) - 1)

static int find_last_set_bit(unsigned int value)
{
	int cnt = 0;

	while (value != 0) {
		value >>= 1;
		cnt++;
	}

	return cnt;
}

static bool is_over_range(unsigned int e, unsigned int f, unsigned int t)
{
	return e > (f + t);
}

static bool is_under_range(unsigned int e, unsigned int f, unsigned int t)
{
	return e < (f - t + 1);
}

static bool is_in_range(unsigned int e, unsigned int f, unsigned int t)
{
	return !is_over_range(e, f, t) && !is_under_range(e, f, t);
}

static bool is_in_subrange(unsigned int e, unsigned int target_m_bits,
		unsigned int f, unsigned int t)
{
	return is_under_range(e, f, t) && ((f - e - t) < target_m_bits);
}

static bool is_precision_lost(uint64_t m, unsigned int f, unsigned int t)
{
	return (m & ((1ull << (f - t)) - 1)) != 0;
}

uint16_t ieee754_convert_single_to_half(float value)
{
	ieee754_single_t single = { .value = value };
	ieee754_half_t half = { .value = 0 };
	uint8_t exp = M_BIT_SINGLE - M_BIT_HALF;

	half.components.sign = single.components.sign;
	if (single.components.e == E_MASK_SINGLE) { /* NaN or infinity */
		half.components.e = E_MASK_HALF;
	} else if (is_over_range(single.components.e, BIAS_SINGLE, BIAS_HALF)) {
		/* make it NaN */
		half.components.e = E_MASK_HALF;
		single.components.m = 0;
	} else if (is_under_range(single.components.e, BIAS_SINGLE, BIAS_HALF)) {
		/* expand the exponent to the mantissa to make it subnormal */
		exp = (uint8_t)(exp + ((BIAS_SINGLE - single.components.e) - BIAS_HALF));
		single.components.m = M_MASK_SINGLE;
	} else { /* zero, normal */
		if (single.components.e != 0) {
			half.components.e = (uint8_t)(single.components.e
				- BIAS_SINGLE + BIAS_HALF) & E_MASK_HALF;
		}
	}

	/* precision may be lost discarding outrange lower bits */
	half.components.m = ((uint32_t)single.components.m >> exp) & M_MASK_HALF;

	return half.value;
}

double ieee754_convert_half_to_double(uint16_t value)
{
	ieee754_half_t half = { .value = value };
	ieee754_double_t d;

	d.components.sign = half.components.sign;
	d.components.e = half.components.e;
	d.components.m = half.components.m;

	if (half.components.e == E_MASK_HALF) { /* NaN or infinity */
		d.components.e = E_MASK_DOUBLE;
		if (half.components.m == M_MASK_HALF) { /* Quiet NaN */
			d.components.m = M_MASK_DOUBLE;
		} else if (half.components.m != 0){ /* Signaling NaN */
			d.components.m = (1ull << (M_BIT_DOUBLE - 1));
		}
	} else if (half.components.e == 0) { /* zero or subnormal */
		if (half.components.m != 0) { /* subnormal */
			/* find the leading 1 to nomalize */
			uint64_t leading_shift = (uint64_t)(M_BIT_HALF -
					find_last_set_bit(half.components.m) + 1);
			d.components.m <<= leading_shift;
			d.components.e =
				(BIAS_DOUBLE - BIAS_HALF - leading_shift + 1)
				& E_MASK_DOUBLE;
		}
	} else { /* normal */
		d.components.e = (uint32_t)(BIAS_DOUBLE + (half.components.e
					- BIAS_HALF)) & 0x7FFu/*11-bit*/;
	}

	d.components.m <<= M_BIT_DOUBLE - M_BIT_HALF;

	return d.value;
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
	} else if (is_in_subrange(single.components.e, M_BIT_HALF,
				BIAS_SINGLE, BIAS_HALF)) {
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
	} else if (is_in_subrange(d.components.e, M_BIT_SINGLE,
				BIAS_DOUBLE, BIAS_SINGLE)) {
		return true;
	}

	return false;
}
