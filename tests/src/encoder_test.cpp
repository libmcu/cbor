/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include <math.h>
#include <string.h>

#include "cbor/encoder.h"

TEST_GROUP(Encoder) {
	cbor_writer_t writer;
	uint8_t writer_buffer[1024];

	void setup(void) {
		cbor_writer_init(&writer, writer_buffer, sizeof(writer_buffer));
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(Encoder, WhenEncodedUnsignedGiven) {
	for (int i = 0; i < 24; i++) {
		LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer,
					(uint64_t)i));
		LONGS_EQUAL(i+1, writer.bufidx);
		LONGS_EQUAL(i, writer.buf[i]);
	}
}
TEST(Encoder, WhenOneByteUnsignedGiven) {
	for (int i = 24; i < 256; i++) {
		LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer,
					(uint64_t)i));
		LONGS_EQUAL((i-23) * 2, writer.bufidx);
		LONGS_EQUAL(24, writer.buf[(i-24)*2]);
		LONGS_EQUAL(i, writer.buf[(i-24)*2+1]);
	}
}
TEST(Encoder, WhenTwoByteUnsignedGiven) {
	const uint8_t expected[] = { 25, 1, 0, 25, 0xff, 0xff };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer, 256));
	LONGS_EQUAL(3, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer, 65535));
	LONGS_EQUAL(6, writer.bufidx);

	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenFourByteUnsignedGiven) {
	const uint8_t expected[] = { 26, 0, 1, 0, 0, 26, 0xff, 0xff, 0xff, 0xff };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer, 65536));
	LONGS_EQUAL(5, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer,
				0xFFFFFFFFul));
	LONGS_EQUAL(10, writer.bufidx);

	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenEightByteUnsignedGiven) {
	const uint8_t expected[] = { 27, 0, 0, 0, 1, 0, 0, 0, 0,
		27, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer,
				0x100000000ull));
	LONGS_EQUAL(9, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer,
				0xFFFFFFFFFFFFFFFFull));
	LONGS_EQUAL(18, writer.bufidx);

	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenNegativeIntegerGiven) {
	const uint8_t expected[] = { 0x20, 0x37, 0x38, 0x18, 0x38, 0xff,
		0x39, 0x01, 0x00, 0x3A, 0x00, 0x01, 0x00, 0x00,
		0x3B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -1));
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -24));
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -25));
	LONGS_EQUAL(4, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -256));
	LONGS_EQUAL(6, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -257));
	LONGS_EQUAL(9, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer, -65537));
	LONGS_EQUAL(14, writer.bufidx);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_negative_integer(&writer,
				-4294967297));
	LONGS_EQUAL(23, writer.bufidx);

	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenEncodedLengthByteStringGiven) {
	for (int i = 0; i < 23; i++) {
		LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_byte_string(&writer,
					(const uint8_t *)&i, 1));
		LONGS_EQUAL((i+1)*2, writer.bufidx);
		LONGS_EQUAL(0x41, writer.buf[i*2]);
		LONGS_EQUAL(i, writer.buf[i*2+1]);
	}
}
TEST(Encoder, WhenOneByteLengthByteStringGiven) {
	uint8_t fixed_str[24];
	memset(fixed_str, 0xA5, sizeof(fixed_str));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_byte_string(&writer,
				fixed_str, sizeof(fixed_str)));
	LONGS_EQUAL(sizeof(fixed_str) + 2, writer.bufidx);
	LONGS_EQUAL(0x58, writer.buf[0]);
	LONGS_EQUAL(24, writer.buf[1]);
	LONGS_EQUAL(0xA5, writer.buf[2]);
	LONGS_EQUAL(0xA5, writer.buf[sizeof(fixed_str) + 2 - 1]);
}
TEST(Encoder, WhenTwoByteLengthByteStringGiven) {
	uint8_t fixed_str[256];
	memset(fixed_str, 0x5A, sizeof(fixed_str));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_byte_string(&writer,
				fixed_str, sizeof(fixed_str)));
	LONGS_EQUAL(sizeof(fixed_str) + 3, writer.bufidx);
	LONGS_EQUAL(0x59, writer.buf[0]);
	LONGS_EQUAL(1, writer.buf[1]);
	LONGS_EQUAL(0, writer.buf[2]);
	LONGS_EQUAL(0x5A, writer.buf[3]);
	LONGS_EQUAL(0x5A, writer.buf[sizeof(fixed_str) + 3 - 1]);
}

TEST(Encoder, WhenIndefiniteTextStringGiven) {
	cbor_encode_text_string_indefinite(&writer);
	cbor_encode_text_string(&writer, "strea", 5);
	cbor_encode_text_string(&writer, "ming", 4);
	LONGS_EQUAL(12, writer.bufidx);
	LONGS_EQUAL(0x7F, writer.buf[0]);
	LONGS_EQUAL(0x65, writer.buf[1]);
	LONGS_EQUAL('s', writer.buf[2]);
	LONGS_EQUAL('t', writer.buf[3]);
	LONGS_EQUAL('r', writer.buf[4]);
	LONGS_EQUAL('e', writer.buf[5]);
	LONGS_EQUAL('a', writer.buf[6]);
	LONGS_EQUAL(0x64, writer.buf[7]);
	LONGS_EQUAL('m', writer.buf[8]);
	LONGS_EQUAL('i', writer.buf[9]);
	LONGS_EQUAL('n', writer.buf[10]);
	LONGS_EQUAL('g', writer.buf[11]);
}

TEST(Encoder, WhenEncodedLengthArrayGiven) {
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 0));
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0x80, writer.buf[0]);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 3));
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(0x83, writer.buf[1]);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 23));
	LONGS_EQUAL(3, writer.bufidx);
	LONGS_EQUAL(0x97, writer.buf[2]);
}
TEST(Encoder, WhenOneByteLengthArrayGiven) {
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 24));
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(0x98, writer.buf[0]);
	LONGS_EQUAL(24, writer.buf[1]);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 255));
	LONGS_EQUAL(4, writer.bufidx);
	LONGS_EQUAL(0x98, writer.buf[2]);
	LONGS_EQUAL(255, writer.buf[3]);
}
TEST(Encoder, WhenTwoByteLengthArrayGiven) {
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 256));
	LONGS_EQUAL(3, writer.bufidx);
	LONGS_EQUAL(0x99, writer.buf[0]);
	LONGS_EQUAL(1, writer.buf[1]);
	LONGS_EQUAL(0, writer.buf[2]);
}
TEST(Encoder, WhenFourByteLengthArrayGiven) {
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_array(&writer, 65536));
	LONGS_EQUAL(5, writer.bufidx);
	LONGS_EQUAL(0x9A, writer.buf[0]);
	LONGS_EQUAL(0, writer.buf[1]);
	LONGS_EQUAL(1, writer.buf[2]);
	LONGS_EQUAL(0, writer.buf[3]);
	LONGS_EQUAL(0, writer.buf[4]);
}
TEST(Encoder, ShouldEncodeInArray_WhenThreeIntegersGiven) {
	cbor_encode_array(&writer, 3);
	cbor_encode_unsigned_integer(&writer, 1);
	cbor_encode_unsigned_integer(&writer, 2);
	cbor_encode_unsigned_integer(&writer, 3);
	LONGS_EQUAL(4, writer.bufidx);
	LONGS_EQUAL(0x83, writer.buf[0]);
	LONGS_EQUAL(1, writer.buf[1]);
	LONGS_EQUAL(2, writer.buf[2]);
	LONGS_EQUAL(3, writer.buf[3]);
}
TEST(Encoder, ShouldEncodeInArray_When25IntegersGiven) {
	cbor_encode_array(&writer, 25);
	for (int i = 0; i < 25; i++) {
		cbor_encode_unsigned_integer(&writer, (uint64_t)i);
	}
	LONGS_EQUAL(28, writer.bufidx);
	LONGS_EQUAL(0x98, writer.buf[0]);
	LONGS_EQUAL(25, writer.buf[1]);

	for (int i = 0; i < 25; i++) {
		LONGS_EQUAL(i, writer.buf[i + 2]);

	}
}
TEST(Encoder, WhenIndefiniteArrayGiven) {
	cbor_encode_array_indefinite(&writer);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0x9F, writer.buf[0]);
}
TEST(Encoder, WhenNestedArrayGiven) {
	const uint8_t expected[] = { 0x83,0x01,0x82,0x02,0x03,0x82,0x04,0x05 };
	cbor_encode_array(&writer, 3);
	  cbor_encode_unsigned_integer(&writer, 1);
	  cbor_encode_array(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 3);
	  cbor_encode_array(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 4);
	    cbor_encode_unsigned_integer(&writer, 5);
	LONGS_EQUAL(sizeof(expected), writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenEncodedLengthMapGiven) {
	cbor_encode_map(&writer, 0);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xA0, writer.buf[0]);
}
TEST(Encoder, WhenEncodedLengthMapGiven2) {
	const uint8_t expected[] = { 0xa2,0x01,0x02,0x03,0x04 };
	cbor_encode_map(&writer, 2);
	  cbor_encode_unsigned_integer(&writer, 1); // key
	  cbor_encode_unsigned_integer(&writer, 2); // value
	  cbor_encode_unsigned_integer(&writer, 3); // key
	  cbor_encode_unsigned_integer(&writer, 4); // value
	LONGS_EQUAL(5, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenEncodedLengthMapGiven3) {
	const uint8_t expected[] = { 0xa5,0x61,0x61,0x61,0x41,0x61,0x62,0x61,
		0x42,0x61,0x63,0x61,0x43,0x61,0x64,0x61,
		0x44,0x61,0x65,0x61,0x45 };
	cbor_encode_map(&writer, 5);
	  // 1st item
	  cbor_encode_text_string(&writer, "a", 1);
	  cbor_encode_text_string(&writer, "A", 1);
	  // 2nd
	  cbor_encode_text_string(&writer, "b", 1);
	  cbor_encode_text_string(&writer, "B", 1);
	  // 3
	  cbor_encode_text_string(&writer, "c", 1);
	  cbor_encode_text_string(&writer, "C", 1);
	  // 4
	  cbor_encode_text_string(&writer, "d", 1);
	  cbor_encode_text_string(&writer, "D", 1);
	  // 5
	  cbor_encode_text_string(&writer, "e", 1);
	  cbor_encode_text_string(&writer, "E", 1);
	LONGS_EQUAL(21, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenIndefiniteLengthMapGiven) {
	const uint8_t expected[] = { 0xbf,0x61,0x61,0x01,0x61,0x62,0x9f,0x02,
		0x03,0xff,0xff };
	cbor_encode_map_indefinite(&writer);
	  cbor_encode_text_string(&writer, "a", 1);
	  cbor_encode_unsigned_integer(&writer, 1);
	  cbor_encode_text_string(&writer, "b", 1);
	  cbor_encode_array_indefinite(&writer);
	    cbor_encode_unsigned_integer(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 3);
	  cbor_encode_break(&writer);
	cbor_encode_break(&writer);
	LONGS_EQUAL(11, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenIndefiniteLengthMapInArrayGiven) {
	const uint8_t expected[] = { 0x82,0x61,0x61,0xbf,0x61,0x62,0x61,0x63,
		0xff };
	cbor_encode_array(&writer, 2);
	  cbor_encode_text_string(&writer, "a", 1);
	  cbor_encode_map_indefinite(&writer);
	    cbor_encode_text_string(&writer, "b", 1);
	    cbor_encode_text_string(&writer, "c", 1);
	  cbor_encode_break(&writer);
	LONGS_EQUAL(9, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenBreakGiven) {
	cbor_encode_break(&writer);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xff, writer.buf[0]);
}
TEST(Encoder, WhenSimpleValueGiven) {
	cbor_encode_simple(&writer, 16);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xf0, writer.buf[0]);
}
TEST(Encoder, WhenOneByteSimpleValueGiven) {
	cbor_encode_simple(&writer, 255);
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(0xf8, writer.buf[0]);
	LONGS_EQUAL(0xff, writer.buf[1]);
}
TEST(Encoder, WhenBoolenGiven) {
	cbor_encode_bool(&writer, false);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xf4, writer.buf[0]);
	cbor_encode_bool(&writer, true);
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(0xf5, writer.buf[1]);
}
TEST(Encoder, WhenNullGiven) {
	cbor_encode_null(&writer);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xf6, writer.buf[0]);
}
TEST(Encoder, WhenUndefinedGiven) {
	cbor_encode_undefined(&writer);
	LONGS_EQUAL(1, writer.bufidx);
	LONGS_EQUAL(0xf7, writer.buf[0]);
}

TEST(Encoder, WhenIndefiniteNestedArrayWithBreakGiven) {
	const uint8_t expected[] = { 0x9f,0x01,0x82,0x02,0x03,0x9f,0x04,0x05,
		0xff,0xff };
	cbor_encode_array_indefinite(&writer);
	  cbor_encode_unsigned_integer(&writer, 1);
	  cbor_encode_array(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 2);
	    cbor_encode_unsigned_integer(&writer, 3);
	  cbor_encode_array_indefinite(&writer);
	    cbor_encode_unsigned_integer(&writer, 4);
	    cbor_encode_unsigned_integer(&writer, 5);
	  cbor_encode_break(&writer);
	cbor_encode_break(&writer);

	LONGS_EQUAL(10, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenInfinityGiven) {
	const uint8_t expected[] = { 0xf9,0x7c,0x00 };
	cbor_encode_float(&writer, INFINITY);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenNegativeInfinityGiven) {
	const uint8_t expected[] = { 0xf9,0xfc,0x00 };
	cbor_encode_float(&writer, -INFINITY);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenNegativeNaNGiven) {
	const uint8_t expected[] = { 0xf9,0x7e,0x00 };
	cbor_encode_float(&writer, NAN);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenZeroFloatGiven) {
	const uint8_t expected[] = { 0xf9,0x00,0x00 };
	cbor_encode_float(&writer, 0.f);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenNegativeZeroFloatGiven) {
	const uint8_t expected[] = { 0xf9,0x80,0x00 };
	cbor_encode_float(&writer, -0.f);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenFloatOneGiven) {
	const uint8_t expected[] = { 0xf9,0x3c,0x00 };
	cbor_encode_float(&writer, 1.f);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenFloatOneHalfGiven) {
	const uint8_t expected[] = { 0xf9,0x3e,0x00 };
	cbor_encode_float(&writer, 1.5f);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenLargestHalfPrecisionFloatGiven) {
	const uint8_t expected[] = { 0xf9,0x7b,0xff };
	cbor_encode_float(&writer, 65504.f);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenSmallestHalfPrecisionFloatGiven) {
	const uint8_t expected[] = { 0xf9,0x03,0xff };
	cbor_encode_float(&writer, 0.00006097555160522461);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenLargestHalfSubnormalGiven) {
	const uint8_t expected[] = { 0xf9,0x04,0x00 };
	cbor_encode_double(&writer, 0.00006103515625);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenSmallestHalfSubNormalGiven) {
	const uint8_t expected[] = { 0xf9,0x00,0x01 };
	cbor_encode_double(&writer, 5.960464477539063e-8);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenHalfPrecisionFloatGiven) {
	const uint8_t expected[] = { 0xf9,0xc4,0x00 };
	cbor_encode_double(&writer, -4.);
	LONGS_EQUAL(3, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenSinglePrecisionFloatGiven) {
	const uint8_t expected[] = { 0xfa,0x47,0xc3,0x50,0x00 };
	cbor_encode_double(&writer, 100000.);
	LONGS_EQUAL(5, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenSinglePrecisionFloatGiven2) {
	const uint8_t expected[] = { 0xfa,0x7f,0x7f,0xff,0xff };
	cbor_encode_double(&writer, 3.4028234663852886e+38);
	LONGS_EQUAL(5, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenDoublePrecisionFloatGiven) {
	const uint8_t expected[] = { 0xfb,0x3f,0xf1,0x99,0x99,0x99,0x99,0x99,0x9a };
	cbor_encode_double(&writer, 1.1);
	LONGS_EQUAL(9, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenDoublePrecisionFloatGiven2) {
	const uint8_t expected[] = { 0xfb,0x7e,0x37,0xe4,0x3c,0x88,0x00,0x75,0x9c };
	cbor_encode_double(&writer, 1.0e+300);
	LONGS_EQUAL(9, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}
TEST(Encoder, WhenDoublePrecisionFloatGiven3) {
	const uint8_t expected[] = { 0xfb,0xc0,0x10,0x66,0x66,0x66,0x66,0x66,0x66 };
	cbor_encode_double(&writer, -4.1);
	LONGS_EQUAL(9, writer.bufidx);
	MEMCMP_EQUAL(expected, writer.buf, sizeof(expected));
}

TEST(Encoder, WhenNullTerminatedTextStringGiven) {
	char const *expected = "A null terminated string";
	cbor_encode_text_string_indefinite(&writer);
	cbor_encode_null_terminated_text_string(&writer, expected);
	MEMCMP_EQUAL(expected, &writer.buf[3], sizeof(expected));
}

TEST(Encoder, When_null_terminated_text_string_WithNullParameterGiven) {
	cbor_encode_text_string_indefinite(&writer);
	cbor_encode_null_terminated_text_string(&writer, NULL);
	LONGS_EQUAL(2, writer.bufidx);
	LONGS_EQUAL(0x7F, writer.buf[0]);
	LONGS_EQUAL(0x60, writer.buf[1]);
}
