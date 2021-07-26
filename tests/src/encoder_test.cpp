#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

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
	const char *fixed_text = "streaming";
	cbor_encode_text_string_indefinite(&writer);
	cbor_encode_text_string(&writer, fixed_text, 5);
	cbor_encode_text_string(&writer, &fixed_text[5], 4);
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
IGNORE_TEST(Encoder, WhenIndefiniteLengthMapGiven) {
}
