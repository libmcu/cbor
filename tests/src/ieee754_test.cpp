#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "cbor/ieee754.h"
#include <math.h>

TEST_GROUP(IEEE754) {
	ieee754_single_t s;
	ieee754_double_t d;

	void setup(void) {
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionZeroGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(0.));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(-0.));
}
TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionInfinityGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(INFINITY));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(-INFINITY));
}
TEST(IEEE754, ShouldConvertToHalf_WhenSinglePrecisionNaNGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(NAN));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(-NAN));
}
TEST(IEEE754, ShouldConvertToHalf_WhenHalfPrecisionSingleValueGiven) {
	s.components.sign = 0;
	s.components.m = 0;
	for (uint8_t i = 127-15+1; i <= 127+15; i++) {
		s.components.e = i;
		LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(s.value));
	}

	s.components.e = 127;
	s.components.m = 0x7FE000; //111_1111_1110_0000_0000_0000
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(s.value));

	s.components.e = 0;
	s.components.m = 0x2000;
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(s.value));
	s.components.m = 0x7FE000;
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_half(s.value));
}
TEST(IEEE754, ShouldKeepSingle_WhenSinglePrecisionGiven) {
	s.components.sign = 0;
	s.components.m = 0;
	s.components.e = 127-15;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_half(s.value));
	s.components.e = 127+16;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_half(s.value));

	s.components.e = 127;
	s.components.m = 0x7FF000; //111_1111_1111_0000_0000_0000
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_half(s.value));

	s.components.e = 0;
	s.components.m = 0x800;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_half(s.value));
}

TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionZeroGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(0.));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(-0.));
}
TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionInfinityGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single((double)INFINITY));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single((double)-INFINITY));
}
TEST(IEEE754, ShouldConvertToHalf_WhenDoublePrecisionNaNGiven) {
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single((double)NAN));
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single((double)-NAN));
}
TEST(IEEE754, ShouldConvertToSingle_WhenSinglePrecisionDoubleValueGiven) {
	d.components.sign = 0;
	d.components.m = 0;
	for (unsigned int i = 1023-127+1; i <= 1023+127; i++) {
		d.components.e = i;
		LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(d.value));
	}

	d.components.e = 1023;
	// 1111_1111_1111_1111_1111_1110_0000_0000_0000_0000_0000_0000_0000
	d.components.m = 0xFFFFFE0000000ull;
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(d.value));

	d.components.e = 0;
	d.components.m = 0x20000000ull; // smallest subnormal
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(d.value));
	d.components.m = 0xFFFFFE0000000ull; // largest subnormal
	LONGS_EQUAL(1, ieee754_is_shrinkable_to_single(d.value));
}
TEST(IEEE754, ShouldKeepDouble_WhenDoublePrecisionGiven) {
	d.components.sign = 0;
	d.components.m = 0;
	d.components.e = 1023-127;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_single(d.value));
	d.components.e = 1023+128;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_single(d.value));

	d.components.e = 1023;
	d.components.m = 0xFFFFFF0000000ull;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_single(d.value));

	d.components.e = 0;
	d.components.m = 0x10000000ull;
	LONGS_EQUAL(0, ieee754_is_shrinkable_to_single(d.value));
}
