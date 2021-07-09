#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "cbor/cbor.h"
#include <string.h>

struct u8map {
	uint8_t key;
	uint8_t value;
};

TEST_GROUP(Decoder) {
	void setup(void) {
	}
	void teardown(void) {
	}
};

// most of test cases are brought from
// https://www.rfc-editor.org/rfc/rfc8949.html#name-examples-of-encoded-cbor-da
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEncodedGiven) {
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
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenOneByteValueGiven) {
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
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x19, 0x03, 0xe8 }; // 1000
	uint16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(1000, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenFourByteValueGiven) {
	uint8_t m[] = { 0x1a, 0x00, 0x0f, 0x42, 0x40 }; // 1000000
	uint32_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(1000000, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEightByteValueGiven) {
	uint8_t m[] = { 0x1b,0x00,0x00,0x00,0xe8,0xd4,0xa5,0x10,0x00 };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(1000000000000ull, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEightByteMaximumValueGiven) {
	uint8_t m[] = { 0x1b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(18446744073709551615ull, v);
}

TEST(Decoder, ShouldDecodeNegativeInteger_WhenEncodedValueGiven) {
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
TEST(Decoder, ShouldDecodeNegativeInteger_EvenWhenDataTypeSizeLargerThanValueSize) {
	uint8_t m = 0x20;
	int32_t v; /* it should work not only in int8_t but also int16, int32,
		      and int64 */
	LONGS_EQUAL(CBOR_UNDERRUN, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(-1, v);
}
TEST(Decoder, ShouldDecodeNegativeInteger_WhenOneByteValueGiven) {
	uint8_t m[] = { 0x38, 0x63 };
	int8_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(-100, v);
}
TEST(Decoder, NegativeBecomesPositive_WhenDataTypeSizeIsSmallerThanValueSize) {
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
TEST(Decoder, ShouldDecodeNegativeInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x39, 0x03, 0xe7 };
	int16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGS_EQUAL(-1000, v);
}
TEST(Decoder, ShouldDecodeNegativeInteger_WhenEightByteValueGiven) {
	// -9223372036854775808 ~ 9223372036854775807
	uint8_t m[] = { 0x3b,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	int64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	LONGLONGS_EQUAL(0x8000000000000000, v);
}

TEST(Decoder, ShouldDecodeByteString) {
	uint8_t expected[] = { 1, 2, 3, 4 };
	uint8_t m[] = { 0x44,0x01,0x02,0x03,0x04 };
	uint8_t buf[sizeof(expected)];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeByteString_WhenZeroLengthStringGiven) {
	uint8_t m;
	uint8_t v;
	m = 0x40;
	LONGS_EQUAL(CBOR_UNDERRUN, cbor_decode(&v, sizeof(v), &m, sizeof(m)));
	LONGS_EQUAL(0, v);
}
TEST(Decoder, ShouldDecodeTextString_WhenWrappedInArray) {
	uint8_t expected[] = { 0x61, 0x62, 0x01, 0xf0, 0x9f, 0x98, 0x80 };
	uint8_t m[] = { 0x84,0x61,0x61,0x61,0x62,0x01,0x64,0xf0,0x9f,0x98,0x80 };
	uint8_t buf[sizeof(expected)];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}

TEST(Decoder, ShouldDecodeArray_WhenSingleLevelArrayGiven) {
	uint8_t expected[] = { 1, 2, 3 };
	uint8_t m[] = { 0x83,0x01,0x02,0x03 };
	uint8_t buf[sizeof(expected)];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenEmptyArrayGiven) {
	uint8_t expected[] = { 0 };
	uint8_t m[] = { 0x80 };
	uint8_t buf[1] = { 0, };
	LONGS_EQUAL(CBOR_UNDERRUN, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenSingleLevelWithDiffentTypeValueGiven) {
	uint8_t m[] = { 0x83,0x01,0x63,0x61,0x62,0x63,0x03 };
	struct {
		uint8_t u8_1;
		char s[3];
		uint8_t u8_2;
	} buf;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&buf, sizeof(buf), m, sizeof(m)));
	LONGS_EQUAL(1, buf.u8_1);
	LONGS_EQUAL(3, buf.u8_2);
	MEMCMP_EQUAL("abc", buf.s, 3);
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelArrayGiven) {
	uint8_t expected[] = { 1, 2, 3, 4, 5 };
	uint8_t m[] = { 0x83,0x01,0x82,0x02,0x03,0x82,0x04,0x05 };
	uint8_t buf[sizeof(expected)] = { 0, };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenOneByteLengthGiven) {
	uint8_t expected[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
		20,21,22,23,24,25 };
	uint8_t m[] = { 0x98,0x19,0x01,0x02,0x03,0x04,0x05,0x06, 0x07,0x08,0x09,
		0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,
		0x16,0x17,0x18,0x18,0x18,0x19 };
	uint8_t buf[sizeof(expected)] = { 0, };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}

TEST(Decoder, ShouldDecodeMap_WhenSingleLevelArrayGiven) {
	struct u8map_2 {
		struct u8map m1;
		struct u8map m2;
	};
	struct u8map_2 expected = {
		.m1 = { .key = 1, .value = 2 },
		.m2 = { .key = 3, .value = 4 },
	};
	struct u8map_2 t = { 0, };
	uint8_t m[] = { 0xa2,0x01,0x02,0x03,0x04 };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&t, sizeof(t), m, sizeof(m)));
	MEMCMP_EQUAL(&expected, &t, sizeof(expected));
}
TEST(Decoder, ShouldDecodeMap_WhenEmptyMapGiven) {
	uint8_t m[] = { 0xa0 };
	uint8_t buf[1] = { 0, };
	LONGS_EQUAL(CBOR_UNDERRUN, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
}
TEST(Decoder, ShouldDecodeMap_WhenEncodedLenGiven) {
	struct u8map_5 {
		struct u8map m1;
		struct u8map m2;
		struct u8map m3;
		struct u8map m4;
		struct u8map m5;
	};
	struct u8map_5 expected = {
		.m1 = { .key = 'a', .value = 'A' },
		.m2 = { .key = 'b', .value = 'B' },
		.m3 = { .key = 'c', .value = 'C' },
		.m4 = { .key = 'd', .value = 'D' },
		.m5 = { .key = 'e', .value = 'E' },
	};
	struct u8map_5 t = { 0, };
	uint8_t m[] = { 0xa5,0x61,0x61,0x61,0x41,0x61,0x62,0x61,0x42,0x61,0x63,
		0x61,0x43,0x61,0x64,0x61,0x44,0x61,0x65,0x61,0x45 };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&t, sizeof(t), m, sizeof(m)));
	MEMCMP_EQUAL(&expected, &t, sizeof(expected));
}
TEST(Decoder, ShouldDecodeMap_WhenArrayValueGiven) {
	struct u8map_2a {
		struct u8map m1;
		struct {
			uint8_t key;
			uint8_t value[2];
		} m2;
	};
	struct u8map_2a expected = {
		.m1 = { .key = 'a', .value = 1 },
		.m2 = { .key = 'b', .value = {2, 3} },
	};
	struct u8map_2a t = { 0, };
	uint8_t m[] = { 0xa2,0x61,0x61,0x01,0x61,0x62,0x82,0x02,0x03 };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&t, sizeof(t), m, sizeof(m)));
	MEMCMP_EQUAL(&expected, &t, sizeof(expected));
}

TEST(Decoder, ShouldReturnBreak_WhenBreakGiven) {
	uint8_t expected[] = { 0 };
	uint8_t m[] = { 0xff };
	uint8_t buf[sizeof(expected)] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeByteString_WhenIndefiniteByteGiven) {
	struct mymsg {
		uint8_t data[8];
		uint8_t ascii[4];
	};
	struct mymsg expected = {
		.data = { 0xf0,0x9f,0xa7,0xac,0xf0,0x9f,0x90,0x98 },
		.ascii = { 'c','b','o','r' },
	};
	uint8_t m[] = { 0x5f,0x48,0xf0,0x9f,0xa7,0xac,0xf0,0x9f,
		0x90,0x98,0x44,0x63,0x62,0x6f,0x72,0xff };
	uint8_t buf[sizeof(struct mymsg)+1];
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(&expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeTextString_WhenIndefiniteTextStringGiven) {
	struct mymsg {
		uint8_t utf_text[8];
		char ascii[4];
	};
	struct mymsg expected = {
		.utf_text = { 0xf0,0x9f,0xa7,0xac,0xf0,0x9f,0x90,0x98 },
		.ascii = { 'c','b','o','r' },
	};
	uint8_t m[] = { 0x7f,0x68,0xf0,0x9f,0xa7,0xac,0xf0,0x9f,
		0x90,0x98,0x64,0x63,0x62,0x6f,0x72,0xff };
	uint8_t buf[sizeof(struct mymsg)+1];
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(&expected, buf, sizeof(expected));
}

TEST(Decoder, ShouldDecodeByteString_WhenIndefiniteByteGiven2) {
	uint8_t expected[] = { 1,2,3,4,5 };
	uint8_t m[] = { 0x5f,0x42,0x01,0x02,0x43,0x03,0x04,0x05,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeTextString_WhenIndefiniteStringGiven) {
	char expected[] = "streaming";
	uint8_t m[] = {	0x7f,0x65,0x73,0x74,0x72,0x65,0x61,0x64,0x6d,0x69,0x6e,0x67,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeEmptyLengthArray_WhenZeroLengthIndefiniteArrarGiven) {
	uint8_t expected[] = { 0 };
	uint8_t m[] = { 0x9f, 0xff };
	uint8_t buf[sizeof(expected)] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven) {
	uint8_t expected[] = { 1, 2, 3, 4, 5 };
	uint8_t m[] = { 0x9f,0x01,0x82,0x02,0x03,0x9f,0x04,0x05,0xff,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven2) {
	uint8_t expected[] = { 1, 2, 3, 4, 5 };
	uint8_t m[] = { 0x9f,0x01,0x82,0x02,0x03,0x82,0x04,0x05,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven3) {
	uint8_t expected[] = { 1, 2, 3, 4, 5 };
	uint8_t m[] = { 0x83,0x01,0x82,0x02,0x03,0x9f,0x04,0x05,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven4) {
	uint8_t expected[] = { 1, 2, 3, 4, 5 };
	uint8_t m[] = { 0x83,0x01,0x9f,0x02,0x03,0xff,0x82,0x04,0x05 };
	uint8_t buf[sizeof(expected)] = { 0, };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenInfiniteLengthGiven) {
	uint8_t expected[] = { 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,
		20,21,22,23,24,25 };
	uint8_t m[] = { 0x9f,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
		0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
		0x17,0x18,0x18,0x18,0x19,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeMap_WhenMultiInfiniteLengthGiven) {
	uint8_t expected[] = { 0x61, 1, 0x62, 2, 3 };
	uint8_t m[] = { 0xbf,0x61,0x61,0x01,0x61,0x62,0x9f,0x02,0x03,0xff,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenInfiniteMapIncluded) {
	uint8_t expected[] = { 0x61, 0x62, 0x63 };
	uint8_t m[] = { 0x82,0x61,0x61,0xbf,0x61,0x62,0x61,0x63,0xff };
	uint8_t buf[sizeof(expected)+1] = { 0, };
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(buf, sizeof(buf), m, sizeof(m)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}

TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionGiven) {
	uint8_t m[] = { 0xfa,0x47,0xc3,0x50,0x00 };
	float v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	DOUBLES_EQUAL(100000.0, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionGiven2) {
	uint8_t m[] = { 0xfa,0x7f,0x7f,0xff,0xff };
	float v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	DOUBLES_EQUAL(3.4028234663852886e+38, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionInfinityGiven) {
	uint8_t m[] = { 0xfa,0x7f,0x80,0x00,0x00 };
	float v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	DOUBLES_EQUAL((double)INFINITY, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionNegativeInfinityGiven) {
	uint8_t m[] = { 0xfa,0xff,0x80,0x00,0x00 };
	float v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	DOUBLES_EQUAL((double)-INFINITY, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionNanGiven) {
	uint8_t m[] = {	0xfa,0x7f,0xc0,0x00,0x00 };
	float v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&v, sizeof(v), m, sizeof(m)));
	CHECK(isnan(v) == true);
}
