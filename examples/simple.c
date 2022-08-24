#include "example.h"

#include <stdio.h>

#include "cbor/encoder.h"
#include "cbor/ieee754.h"
#include "cbor/decoder.h"
#include "cbor/parser.h"
#include "cbor/helper.h"

union cbor_value {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	float f32;
	double f64;
	uint8_t *bin;
	char *str;
	uint8_t str_copy[16];
};

static void print_cbor(cbor_reader_t const *reader, cbor_item_t const *item, example_writer print)
{
	union cbor_value val;

	memset(&val, 0, sizeof(val));
	cbor_decode(reader, item, &val, sizeof(val));

	switch (item->type) {
	case CBOR_ITEM_INTEGER: {
		char buf[16];
		int len = sprintf(buf, "%d", val.i32);
		print(buf, (size_t)len);
		} break;
	case CBOR_ITEM_STRING:
		print(val.str_copy, item->size);
		break;
	case CBOR_ITEM_SIMPLE_VALUE:
		print(val.i8 == 1? "true" : "false", 4);
	default:
		break;
	}
}

static size_t encode_simple_data(void *buf, size_t bufsize)
{
	cbor_writer_t writer;
	cbor_writer_init(&writer, buf, bufsize);

        cbor_encode_text_string(&writer, "Hello, World!");
	cbor_encode_unsigned_integer(&writer, 1661225893);
	cbor_encode_bool(&writer, true);

	return cbor_writer_len(&writer);
}

static void decode_simple_data(void const *data, size_t datasize, example_writer print)
{
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(items[0]));
	cbor_parse(&reader, data, datasize, &n);

	for (size_t i = 0; i < n; i++) {
		print_cbor(&reader, &items[i], print);
	}
}

void simple_example(example_writer print)
{
	uint8_t buf[32];
	size_t len = encode_simple_data(buf, sizeof(buf));
	decode_simple_data(buf, len, print);
}
