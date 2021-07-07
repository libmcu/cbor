#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "cbor/cbor.h"
#include <string.h>

TEST_GROUP(cbor) {
	void setup(void) {
	}
	void teardown(void) {
	}
};

TEST(cbor, ShouldDecodeUnsignedInteger_WhenEncodedGiven) {
	uint8_t m;
	uint8_t v;
	m = 0;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(0, v);
	m = 1;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(1, v);
	m = 10;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(10, v);
	m = 23;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(23, v);
}
TEST(cbor, ShouldDecodeUnsignedInteger_WhenOneByteValueGiven) {
	uint8_t m1[] = { 0x18, 0x18 }; // 24
	uint8_t m2[] = { 0x18, 0x19 }; // 25
	uint8_t m3[] = { 0x18, 0x64 }; // 100
	uint8_t m4[] = { 0x18, 0xff }; // 255
	uint8_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m1, sizeof(m1)));
	LONGS_EQUAL(24, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m2, sizeof(m2)));
	LONGS_EQUAL(25, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m3, sizeof(m3)));
	LONGS_EQUAL(100, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m4, sizeof(m4)));
	LONGS_EQUAL(255, v);
}
TEST(cbor, ShouldDecodeUnsignedInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x19, 0x03, 0xe8 }; // 1000
	uint16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(1000, v);
}
TEST(cbor, ShouldDecodeUnsignedInteger_WhenFourByteValueGiven) {
	uint8_t m[] = { 0x1a, 0x00, 0x0f, 0x42, 0x40 }; // 1000000
	uint32_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(1000000, v);
}
TEST(cbor, ShouldDecodeUnsignedInteger_WhenEightByteValueGiven) {
	uint8_t m[] = { 0x1b,0x00,0x00,0x00,0xe8,0xd4,0xa5,0x10,0x00 };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(1000000000000ull, v);
}
TEST(cbor, ShouldDecodeUnsignedInteger_WhenEightByteMaximumValueGiven) {
	uint8_t m[] = { 0x1b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(18446744073709551615ull, v);
}

TEST(cbor, ShouldDecodeNegativeInteger_WhenEncodedValueGiven) {
	uint8_t m;
	int8_t v;
	m = 0x20;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(-1, v);
	m = 0x29;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(-10, v);
	m = 0x37;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(-24, v);
}
TEST(cbor, ShouldDecodeNegativeInteger_EvenWhenDataTypeSizeLargerThanValueSize) {
	uint8_t m = 0x20;
	int32_t v; /* it should work not only in int8_t but also int16, int32,
		      and int64 */
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(-1, v);
}
TEST(cbor, ShouldDecodeNegativeInteger_WhenOneByteValueGiven) {
	uint8_t m[] = { 0x38, 0x63 };
	int8_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(-100, v);
}
TEST(cbor, NegativeBecomesPositive_WhenDataTypeSizeIsSmallerThanValueSize) {
	uint8_t m1[] = { 0x38, 0x80 };
	uint8_t m2[] = { 0x38, 0xff };
	int8_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m1, sizeof(m1)));
	LONGS_EQUAL(0x7f, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m2, sizeof(m2)));
	LONGS_EQUAL(0, v);

	int16_t v16;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v16, sizeof(v16), m1, sizeof(m1)));
	LONGS_EQUAL(-129, v16);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v16, sizeof(v16), m2, sizeof(m2)));
	LONGS_EQUAL(-256, v16);
}
TEST(cbor, ShouldDecodeNegativeInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x39, 0x03, 0xe7 };
	int16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(-1000, v);
}
TEST(cbor, ShouldDecodeNegativeInteger_WhenEightByteValueGiven) {
	// -9223372036854775808 ~ 9223372036854775807
	uint8_t m[] = { 0x3b,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	int64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(0x8000000000000000, v);
}

TEST(cbor, ShouldDecodeByteString) {
	uint8_t expected[] = { 1, 2, 3, 4 };
	uint8_t m[] = { 0x44,0x01,0x02,0x03,0x04 };
	uint8_t buf[16];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(cbor, ShouldDecodeByteString_WhenZeroLengthStringGiven) {
	uint8_t m;
	uint8_t v;
	m = 0x40;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(0, v);
}

TEST(cbor, ShouldDecodeArray_WhenSingleLevelArrayGiven) {
	uint8_t expected[] = { 1, 2, 3 };
	uint8_t m[] = { 0x83,0x01,0x02,0x03 };
	uint8_t buf[8];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
IGNORE_TEST(cbor, ShouldDecodeArray_WhenSingleLevelWithDiffentTypeValueGiven) {
}

IGNORE_TEST(cbor, decode_ShouldReturnUnsignedInteger_WhenTagBignum) {
#if 0
	uint8_t m[] = { 0xc2,0x49,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(18446744073709551616, v);
	uint8_t m[] = { 0xc3,0x49,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 };
	LONGLONGS_EQUAL(-18446744073709551617, v);
#endif
}
