#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "example.h"
#include "cbor/encoder.h"

TEST_GROUP(Example) {
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

TEST(Example, ShouldConvertToUserDefinedDataType) {
	cbor_encode_map_indefinite(&writer);
	  cbor_encode_text_string(&writer, "t");
	  cbor_encode_unsigned_integer(&writer, 1661225893);
	  cbor_encode_text_string(&writer, "data");
	    cbor_encode_map(&writer, 2);
	      cbor_encode_text_string(&writer, "acc");
	      cbor_encode_map(&writer, 3);
	        cbor_encode_text_string(&writer, "x");
	        cbor_encode_float(&writer, 0.f);
	        cbor_encode_text_string(&writer, "y");
	        cbor_encode_float(&writer, 0.f);
	        cbor_encode_text_string(&writer, "z");
	        cbor_encode_float(&writer, 9.81f);
	      cbor_encode_text_string(&writer, "gyro");
	        cbor_encode_map(&writer, 3);
	        cbor_encode_text_string(&writer, "x");
	        cbor_encode_float(&writer, 6.3f);
	        cbor_encode_text_string(&writer, "y");
	        cbor_encode_float(&writer, 0.f);
	        cbor_encode_text_string(&writer, "z");
	        cbor_encode_float(&writer, 0.f);
	cbor_encode_break(&writer);

	struct udt udt = { 0, };
	complex_example(cbor_writer_get_encoded(&writer), cbor_writer_len(&writer), &udt);

	LONGS_EQUAL(1661225893, udt.time);
	DOUBLES_EQUAL(0., (double)udt.data.acc.x, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.acc.y, 0.01);
	DOUBLES_EQUAL(9.81, (double)udt.data.acc.z, 0.01);
	DOUBLES_EQUAL(6.3, (double)udt.data.gyro.x, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.gyro.y, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.gyro.z, 0.01);
}

TEST(Example, ShouldConvertToUserDefinedDataType2) {
	uint8_t const uuid[] = { 0x12, 0x3e, 0x45, 0x67, 0xe8, 0x9b, 0x12, 0xd3,
				0xa4, 0x56, 0x42, 0x66, 0x14, 0x17, 0x40, 0x00 };
	cbor_encode_array_indefinite(&writer);
	  cbor_encode_simple(&writer, 1);
	  cbor_encode_byte_string(&writer, uuid, sizeof(uuid));
	  cbor_encode_map_indefinite(&writer);
	    cbor_encode_text_string(&writer, "t");
	    cbor_encode_unsigned_integer(&writer, 1661225893);
	    cbor_encode_text_string(&writer, "data");
	      cbor_encode_map(&writer, 2);
	        cbor_encode_text_string(&writer, "acc");
	        cbor_encode_map(&writer, 3);
	          cbor_encode_text_string(&writer, "x");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "y");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "z");
	          cbor_encode_float(&writer, 9.81f);
	        cbor_encode_text_string(&writer, "gyro");
	          cbor_encode_map(&writer, 3);
	          cbor_encode_text_string(&writer, "x");
	          cbor_encode_float(&writer, 6.3f);
	          cbor_encode_text_string(&writer, "y");
	          cbor_encode_float(&writer, 0.f);
	          cbor_encode_text_string(&writer, "z");
	          cbor_encode_float(&writer, 0.f);
	  cbor_encode_break(&writer);
	cbor_encode_break(&writer);

	struct udt udt = { 0, };
	complex_example(cbor_writer_get_encoded(&writer), cbor_writer_len(&writer), &udt);

	LONGS_EQUAL(1661225893, udt.time);
	DOUBLES_EQUAL(0., (double)udt.data.acc.x, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.acc.y, 0.01);
	DOUBLES_EQUAL(9.81, (double)udt.data.acc.z, 0.01);
	DOUBLES_EQUAL(6.3, (double)udt.data.gyro.x, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.gyro.y, 0.01);
	DOUBLES_EQUAL(0., (double)udt.data.gyro.z, 0.01);

	LONGS_EQUAL(1, udt.type);
	MEMCMP_EQUAL(uuid, udt.uuid, sizeof(uuid));
}
