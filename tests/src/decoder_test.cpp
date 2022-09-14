/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "cbor/parser.h"
#include "cbor/decoder.h"
#include "cbor/helper.h"
#include <string.h>
#include <math.h>
#include <inttypes.h>

typedef void (*check_func_t)(size_t size);

static void f_unknown(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_integer(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_string(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_array(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_map(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_float(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}
static void f_simple(size_t size) {
	mock().actualCall(__func__).withParameter("size", size);
}

static check_func_t check_item_type[] = {
	f_unknown,
	f_integer,
	f_string,
	f_array,
	f_map,
	f_float,
	f_simple,
};

TEST_GROUP(Decoder) {
	cbor_reader_t reader;
	cbor_item_t items[256];

	void setup(void) {
		cbor_reader_init(&reader, items, sizeof(items)/sizeof(*items));
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

// most of test cases are brought from
// https://www.rfc-editor.org/rfc/rfc8949.html#name-examples-of-encoded-cbor-da
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEncodedGiven) {
	uint8_t m[] = { 0, 1, 10, 23};
	size_t n;
	for (int i = 0; i < (int)(sizeof(m)/sizeof(m[0])); i++) {
		LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m[i], sizeof(m[i]), &n));
		LONGS_EQUAL(1, n);
		LONGS_EQUAL(0, items[0].size);
		LONGS_EQUAL(0, items[0].offset);
		uint8_t v;
		LONGS_EQUAL(CBOR_SUCCESS,
				cbor_decode(&reader, &items[0], &v, sizeof(v)));
		LONGS_EQUAL(m[i], v);
	}
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenOneByteValueGiven) {
	uint8_t m1[] = { 0x18, 0x18 }; // 24
	uint8_t m2[] = { 0x18, 0x19 }; // 25
	uint8_t m3[] = { 0x18, 0x64 }; // 100
	uint8_t m4[] = { 0x18, 0xff }; // 255
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m1, sizeof(m1), NULL));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(24, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m2, sizeof(m2), NULL));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(25, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m3, sizeof(m3), NULL));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(100, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m4, sizeof(m4), NULL));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(255, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x19, 0x03, 0xe8 }; // 1000
	uint16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(1000, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenFourByteValueGiven) {
	uint8_t m[] = { 0x1a, 0x00, 0x0f, 0x42, 0x40 }; // 1000000
	uint32_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(1000000, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEightByteValueGiven) {
	uint8_t m[] = { 0x1b,0x00,0x00,0x00,0xe8,0xd4,0xa5,0x10,0x00 };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGLONGS_EQUAL(1000000000000ull, v);
}
TEST(Decoder, ShouldDecodeUnsignedInteger_WhenEightByteMaximumValueGiven) {
	uint8_t m[] = { 0x1b,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	uint64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGLONGS_EQUAL(18446744073709551615ull, v);
}

TEST(Decoder, ShouldDecodeNegativeInteger_WhenEncodedValueGiven) {
	uint8_t m;
	int8_t v;
	m = 0x20;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(-1, v);
	m = 0x29;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(-10, v);
	m = 0x37;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(-24, v);
}
TEST(Decoder, ShouldDecodeNegativeInteger_EvenWhenDataTypeSizeLargerThanValueSize) {
	uint8_t m = 0x20;
	int8_t v8;
	int16_t v16;
	int32_t v32;
	int64_t v64;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v8, sizeof(v8)));
	LONGS_EQUAL(-1, v8);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v16, sizeof(v16)));
	LONGS_EQUAL(-1, v16);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v32, sizeof(v32)));
	LONGS_EQUAL(-1, v32);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v64, sizeof(v64)));
	LONGS_EQUAL(-1, v64);
}
TEST(Decoder, ShouldDecodeNegativeInteger_WhenOneByteValueGiven) {
	uint8_t m[] = { 0x38, 0x63 };
	int8_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(-100, v);
}
TEST(Decoder, NegativeBecomesPositive_WhenDataTypeSizeIsSmallerThanValueSize) {
	uint8_t m1[] = { 0x38, 0x80 };
	uint8_t m2[] = { 0x38, 0xff };
	int8_t v;
	int16_t v16;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m1, sizeof(m1), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(0x7f, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v16, sizeof(v16)));
	LONGS_EQUAL(-129, v16);

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m2, sizeof(m2), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(0, v);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v16, sizeof(v16)));
	LONGS_EQUAL(-256, v16);
}
TEST(Decoder, ShouldDecodeNegativeInteger_WhenTwoByteValueGiven) {
	uint8_t m[] = { 0x39, 0x03, 0xe7 };
	int16_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(-1000, v);
}
TEST(Decoder, ShouldDecodeNegativeInteger_WhenEightByteValueGiven) {
	// -9223372036854775808 ~ 9223372036854775807
	uint8_t m[] = { 0x3b,0x7f,0xff,0xff,0xff,0xff,0xff,0xff,0xff };
	int64_t v;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGLONGS_EQUAL(0x8000000000000000, v);
}

TEST(Decoder, ShouldDecodeByteString) {
	uint8_t expected[] = { 1, 2, 3, 4 };
	uint8_t m[] = { 0x44,0x01,0x02,0x03,0x04 };
	uint8_t buf[sizeof(expected)];
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], buf, sizeof(buf)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeByteString_WhenZeroLengthStringGiven) {
	uint8_t m;
	uint8_t v;
	m = 0x40;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, &m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], &v, sizeof(v)));
	LONGS_EQUAL(0, v);
}
TEST(Decoder, ShouldDecodeTextString_WhenWrappedInArray) {
	uint8_t m[] = { 0x84,0x61,0x61,0x61,0x62,0x01,0x64,0xf0,0x9f,0x98,0x80 };
	uint8_t buf[sizeof(m)];
	size_t n;
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(5, n);

	LONGS_EQUAL(4, items[0].size); // array
	LONGS_EQUAL(1, items[1].size); // text
	LONGS_EQUAL(1, items[2].size); // text
	LONGS_EQUAL(0, items[3].size); // unsigned
	LONGS_EQUAL(4, items[4].size); // text

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[0], buf, sizeof(buf)));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[1], buf, sizeof(buf)));
	MEMCMP_EQUAL("a", buf, 1);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[2], buf, sizeof(buf)));
	MEMCMP_EQUAL("b", buf, 1);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[3], buf, sizeof(buf)));
	LONGS_EQUAL(1, buf[0]);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, &items[4], buf, sizeof(buf)));
	uint8_t expected[] = { 0xF0,0x9F,0x98,0x80 };
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}

TEST(Decoder, ShouldDecodeArray_WhenSingleLevelArrayGiven) {
	uint8_t m[] = { 0x83,0x01,0x02,0x03 };
	uint8_t v;
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);
	LONGS_EQUAL(CBOR_ITEM_ARRAY, items[0].type);
	for (size_t i = 1; i < n; i++) {
		LONGS_EQUAL(CBOR_SUCCESS,
				cbor_decode(&reader, &items[i], &v, sizeof(v)));
		LONGS_EQUAL(i, v);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenEmptyArrayGiven) {
	uint8_t expected[] = { 0 };
	uint8_t m[] = { 0x80 };
	uint8_t buf[1] = { 0, };
	cbor_parse(&reader, m, sizeof(m), 0);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, buf, sizeof(buf)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeArray_WhenSingleLevelWithDiffentTypeValueGiven) {
	uint8_t m[] = { 0x83,0x01,0x63,0x61,0x62,0x63,0x03 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);

	mock().expectOneCall("f_array").withParameter("size", 3);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_string").withParameter("size", 3);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelArrayGiven) {
	uint8_t m[] = { 0x83,0x01,0x82,0x02,0x03,0x82,0x04,0x05 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(8, n);

	mock().expectOneCall("f_array").withParameter("size", 3);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenOneByteLengthGiven) {
	// [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]
	uint8_t m[] = { 0x98,0x19,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
		0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,
		0x16,0x17,0x18,0x18,0x18,0x19 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(26, n);

	mock().expectOneCall("f_array").withParameter("size", 25);
	mock().expectNCalls(23, "f_integer").withParameter("size", 0);
	mock().expectNCalls(2, "f_integer").withParameter("size", 1);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}

TEST(Decoder, ShouldDecodeMap_WhenSingleLevelArrayGiven) {
	uint8_t m[] = { 0xa2,0x01,0x02,0x03,0x04 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(5, n);

	mock().expectOneCall("f_map").withParameter("size", 2);
	mock().expectNCalls(4, "f_integer").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeMap_WhenEmptyMapGiven) {
	uint8_t m[] = { 0xa0 };
	uint8_t buf[1] = { 0, };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, buf, sizeof(buf)));
}
TEST(Decoder, ShouldDecodeMap_WhenEncodedLenGiven) {
	// {"a": "A", "b": "B", "c": "C", "d": "D", "e": "E"}
	uint8_t m[] = { 0xa5,0x61,0x61,0x61,0x41,0x61,0x62,0x61,0x42,0x61,0x63,
		0x61,0x43,0x61,0x64,0x61,0x44,0x61,0x65,0x61,0x45 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(11, n);

	mock().expectOneCall("f_map").withParameter("size", 5);
	mock().expectNCalls(10, "f_string").withParameter("size", 1);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeMap_WhenArrayValueGiven) {
	// {"a": 1, "b": [2, 3]}
	uint8_t m[] = { 0xa2,0x61,0x61,0x01,0x61,0x62,0x82,0x02,0x03 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(7, n);

	mock().expectOneCall("f_map").withParameter("size", 2);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}

TEST(Decoder, ShouldReturnBreak_WhenBreakGiven) {
	uint8_t expected[] = { 0 };
	uint8_t m[] = { 0xff };
	uint8_t buf[sizeof(expected)] = { 0, };

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), 0));
	LONGS_EQUAL(CBOR_BREAK, cbor_decode(&reader, items, buf, sizeof(buf)));
	MEMCMP_EQUAL(expected, buf, sizeof(expected));
}
TEST(Decoder, ShouldDecodeByteString_WhenIndefiniteByteGiven) {
	// (_ h'F09FA7ACF09F9098', h'63626F72')
	uint8_t m[] = { 0x5f,0x48,0xf0,0x9f,0xa7,0xac,0xf0,0x9f,
		0x90,0x98,0x44,0x63,0x62,0x6f,0x72,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);

	mock().expectOneCall("f_string").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 8);
	mock().expectOneCall("f_string").withParameter("size", 4);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeTextString_WhenIndefiniteTextStringGiven) {
	// (_ "\u{1F9EC}\ud83d\udc18", "cbor")
	uint8_t m[] = { 0x7f,0x68,0xf0,0x9f,0xa7,0xac,0xf0,0x9f,
		0x90,0x98,0x64,0x63,0x62,0x6f,0x72,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);

	mock().expectOneCall("f_string").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 8);
	mock().expectOneCall("f_string").withParameter("size", 4);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}

TEST(Decoder, ShouldDecodeByteString_WhenIndefiniteByteGiven2) {
	// (_ h'0102', h'030405')
	uint8_t m[] = { 0x5f,0x42,0x01,0x02,0x43,0x03,0x04,0x05,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);

	mock().expectOneCall("f_string").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 2);
	mock().expectOneCall("f_string").withParameter("size", 3);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeTextString_WhenIndefiniteStringGiven) {
	// (_ "strea", "ming")
	uint8_t m[] = {	0x7f,0x65,0x73,0x74,0x72,0x65,0x61,0x64,0x6d,0x69,0x6e,0x67,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(4, n);

	mock().expectOneCall("f_string").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 5);
	mock().expectOneCall("f_string").withParameter("size", 4);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeEmptyLengthArray_WhenZeroLengthIndefiniteArrarGiven) {
	uint8_t m[] = { 0x9f, 0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(2, n);

	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven) {
	// [1, [2, 3], [4, 5]]
	uint8_t m[] = { 0x9f,0x01,0x82,0x02,0x03,0x9f,0x04,0x05,0xff,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(10, n);

	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_float").withParameter("size", 0xff);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven2) {
	// [1, [2, 3], [4, 5]]
	uint8_t m[] = { 0x9f,0x01,0x82,0x02,0x03,0x82,0x04,0x05,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(9, n);

	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven3) {
	// [1, [2, 3], [4, 5]]
	uint8_t m[] = { 0x83,0x01,0x82,0x02,0x03,0x9f,0x04,0x05,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(9, n);

	mock().expectOneCall("f_array").withParameter("size", 3);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenMultiLevelIndefiniteArrarGiven4) {
	// [1, [2, 3], [4, 5]]
	uint8_t m[] = { 0x83,0x01,0x9f,0x02,0x03,0xff,0x82,0x04,0x05 };
	size_t n;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(9, n);

	mock().expectOneCall("f_array").withParameter("size", 3);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_float").withParameter("size", 0xff);
	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenInfiniteLengthGiven) {
	// [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]
	uint8_t m[] = { 0x9f,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
		0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
		0x17,0x18,0x18,0x18,0x19,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(27, n);

	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectNCalls(23, "f_integer").withParameter("size", 0);
	mock().expectNCalls(2, "f_integer").withParameter("size", 1);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeMap_WhenMultiInfiniteLengthGiven) {
	// {"a": 1, "b": [2, 3]}
	uint8_t m[] = { 0xbf,0x61,0x61,0x01,0x61,0x62,0x9f,0x02,0x03,0xff,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(9, n);

	mock().expectOneCall("f_map").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_integer").withParameter("size", 0);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_array").withParameter("size", (size_t)-1);
	mock().expectNCalls(2, "f_integer").withParameter("size", 0);
	mock().expectOneCall("f_float").withParameter("size", 0xff);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}
TEST(Decoder, ShouldDecodeArray_WhenInfiniteMapIncluded) {
	// ["a", {"b": "c"}]
	uint8_t m[] = { 0x82,0x61,0x61,0xbf,0x61,0x62,0x61,0x63,0xff };
	size_t n;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(6, n);

	mock().expectOneCall("f_array").withParameter("size", 2);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_map").withParameter("size", (size_t)-1);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_string").withParameter("size", 1);
	mock().expectOneCall("f_float").withParameter("size", 0xff);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}
}

TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionGiven) {
	uint8_t m[] = { 0xfa,0x47,0xc3,0x50,0x00 };
	size_t n;
	float v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_float").withParameter("size", 4);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	DOUBLES_EQUAL(100000.0, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionGiven2) {
	uint8_t m[] = { 0xfa,0x7f,0x7f,0xff,0xff };
	size_t n;
	float v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_float").withParameter("size", 4);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	DOUBLES_EQUAL(3.4028234663852886e+38, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionInfinityGiven) {
	uint8_t m[] = { 0xfa,0x7f,0x80,0x00,0x00 };
	size_t n;
	float v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_float").withParameter("size", 4);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	DOUBLES_EQUAL((double)INFINITY, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionNegativeInfinityGiven) {
	uint8_t m[] = { 0xfa,0xff,0x80,0x00,0x00 };
	size_t n;
	float v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_float").withParameter("size", 4);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	DOUBLES_EQUAL((double)-INFINITY, (double)v, 0.1);
}
TEST(Decoder, ShouldDecodeFloat_WhenSinglePrecisionNanGiven) {
	uint8_t m[] = {	0xfa,0x7f,0xc0,0x00,0x00 };
	size_t n;
	float v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_float").withParameter("size", 4);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	CHECK(isnan(v) == true);
}

TEST(Decoder, ShouldDecodeFalse) {
	uint8_t m[] = {	0xf4 };
	size_t n;
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_simple").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	LONGS_EQUAL(0, v);
}
TEST(Decoder, ShouldDecodeTrue) {
	uint8_t m[] = {	0xf5 };
	size_t n;
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_simple").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	LONGS_EQUAL(1, v);
}
TEST(Decoder, ShouldDecodeNull) {
	uint8_t m[] = {	0xf6 };
	size_t n;
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_simple").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	LONGS_EQUAL(0, v);
}
TEST(Decoder, ShouldDecodeSimpleValue) {
	uint8_t m[] = {	0xf0 };
	size_t n;
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_simple").withParameter("size", 0);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	LONGS_EQUAL(16, v);
}
TEST(Decoder, ShouldDecodeSimpleValue_WhenFollowingByteGiven) {
	uint8_t m[] = {	0xf8, 0xff };
	size_t n;
	uint8_t v;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, m, sizeof(m), &n));
	LONGS_EQUAL(1, n);

	mock().expectOneCall("f_simple").withParameter("size", 1);

	for (size_t i = 0; i < n; i++) {
		check_item_type[items[i].type](items[i].size);
	}

	LONGS_EQUAL(CBOR_SUCCESS, cbor_decode(&reader, items, &v, sizeof(v)));
	LONGS_EQUAL(255, v);
}

TEST(Decoder, ShouldReturnIllegal_WhenIncompleteUnsignedIntegerGiven) {
	for (uint8_t i = 0x18; i < 0x20; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteNegativeIntegerGiven) {
	for (uint8_t i = 0x38; i < 0x40; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteStringGiven) {
	for (uint8_t i = 0x41; i < 0x5f; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteArrayGiven) {
	for (uint8_t i = 0x81; i < 0x98; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteArrayGiven2) {
	for (uint8_t i = 0x98; i < 0x9f; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteMapGiven) {
	for (uint8_t i = 0xa1; i < 0xb8; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}
TEST(Decoder, ShouldReturnIllegal_WhenIncompleteMapGiven2) {
	for (uint8_t i = 0xb8; i < 0xbf; i++) {
		LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, &i, sizeof(i), 0));
	}
}

#if 0
TEST(Decoder, ShouldParseIncremental) {
	// [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25]
	uint8_t m[] = { 0x9f,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,
		0x0b,0x0c,0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15,0x16,
		0x17,0x18,0x18,0x18,0x19,0xff };

	union {
		int8_t i8;
		int16_t i16;
		int32_t i32;
		int64_t i64;
	} val;

	cbor_reader_init(&reader, m, sizeof(m));

	for (int i = 0; i < 26; i++) {
		size_t n;
		cbor_parse(&reader, items, 1, &n);
		LONGS_EQUAL(1, n);
		if (items[0].type == CBOR_ITEM_INTEGER) {
			cbor_decode(&reader, &items[0], &val, sizeof(val));
			LONGS_EQUAL(i, val.i8);
		}
	}
}
#endif
