#include "cbor/encoder.h"
#include "cbor/ieee754.h"

#define MAJOR_TYPE_BIT			5

static uint8_t get_additional_info(uint64_t value)
{
	uint8_t additional_info = 0;

	if (value & ~(0x100000000ull - 1)) { /* 8-byte following */
		additional_info = 27;
	} else if (value & ~(0x10000ull - 1)) { /* 4-byte following */
		additional_info = 26;
	} else if (value & ~(0x100ull - 1)) { /* 2-byte following */
		additional_info = 25;
	} else if (value >= 24) { /* 1-byte following */
		additional_info = 24;
	} else { /* 0 ~ 23 */
		additional_info = (uint8_t)value;
	}

	return additional_info;
}

static cbor_error_t encode_core(cbor_writer_t *writer, uint8_t major_type,
		uint8_t const *data, uint64_t datasize, bool indefinite)
{
	uint8_t *buf = &writer->buf[writer->bufidx];
	uint8_t additional_info = get_additional_info(datasize);
	uint8_t following_bytes = cbor_get_following_bytes(additional_info);

	if (indefinite) {
		additional_info = 31;
		following_bytes = 0;
	}

	size_t bytes_to_write = (size_t)datasize + following_bytes + 1;
	/* NOTE: if not string, `datasize` is the actual value to be written.
	 * And the `following_bytes` is the length of it. */
	if (!(major_type == 2 || major_type == 3)) {
		bytes_to_write -= (size_t)datasize;
	}

	if (bytes_to_write > (writer->bufsize - writer->bufidx)) {
		return CBOR_OVERRUN;
	}

	buf[0] = (uint8_t)(major_type << MAJOR_TYPE_BIT) | additional_info;
	cbor_copy(&buf[1], (uint8_t const *)&datasize, following_bytes);
	if (data != NULL) {
		cbor_copy_be(&buf[1 + following_bytes], data, (size_t)datasize);
	}

	writer->bufidx += bytes_to_write;

	return CBOR_SUCCESS;
}

static cbor_error_t encode_simple(cbor_writer_t *writer, uint8_t value)
{
	return encode_core(writer, 7, NULL, value, false);
}

cbor_error_t cbor_encode_unsigned_integer(cbor_writer_t *writer, uint64_t value)
{
	return encode_core(writer, 0, NULL, value, false);
}

cbor_error_t cbor_encode_negative_integer(cbor_writer_t *writer, int64_t value)
{
	return encode_core(writer, 1, NULL, ((uint64_t)-value) - 1, false);
}

cbor_error_t cbor_encode_byte_string(cbor_writer_t *writer,
		uint8_t const *data, size_t datasize)
{
	return encode_core(writer, 2, data, datasize, false);
}

cbor_error_t cbor_encode_byte_string_indefinite(cbor_writer_t *writer)
{
	return encode_core(writer, 2, NULL, 0, true);
}

cbor_error_t cbor_encode_text_string(cbor_writer_t *writer,
		char const *text, size_t textsize)
{
	return encode_core(writer, 3,
			(uint8_t const *)text, textsize, false);
}

cbor_error_t cbor_encode_text_string_indefinite(cbor_writer_t *writer)
{
	return encode_core(writer, 3, NULL, 0, true);
}

cbor_error_t cbor_encode_array(cbor_writer_t *writer, size_t length)
{
	return encode_core(writer, 4, NULL, length, false);
}

cbor_error_t cbor_encode_array_indefinite(cbor_writer_t *writer)
{
	return encode_core(writer, 4, NULL, 0, true);
}

cbor_error_t cbor_encode_map(cbor_writer_t *writer, size_t length)
{
	return encode_core(writer, 5, NULL, length, false);
}

cbor_error_t cbor_encode_map_indefinite(cbor_writer_t *writer)
{
	return encode_core(writer, 5, NULL, 0, true);
}

cbor_error_t cbor_encode_break(cbor_writer_t *writer)
{
	return encode_core(writer, 7, NULL, 0xff, true);
}

cbor_error_t cbor_encode_simple(cbor_writer_t *writer, uint8_t value)
{
	return encode_simple(writer, value);
}

cbor_error_t cbor_encode_bool(cbor_writer_t *writer, bool value)
{
	return encode_simple(writer, (uint8_t)(value + 20));
}

cbor_error_t cbor_encode_null(cbor_writer_t *writer)
{
	return encode_simple(writer, 22);
}

cbor_error_t cbor_encode_undefined(cbor_writer_t *writer)
{
	return encode_simple(writer, 23);
}

static cbor_error_t encode_float(cbor_writer_t *writer, float value)
{
	if (ieee754_is_shrinkable_to_half(value)) {
		uint16_t half = ieee754_convert_single_to_half(value);

		writer->buf[writer->bufidx++] = 0xF9;
		writer->bufidx += cbor_copy(&writer->buf[writer->bufidx],
				(uint8_t const *)&half, sizeof(half));

		return CBOR_SUCCESS;
	}

	writer->buf[writer->bufidx++] = 0xFA;
	writer->bufidx += cbor_copy(&writer->buf[writer->bufidx],
			(uint8_t const *)&value, sizeof(value));

	return CBOR_SUCCESS;
}

cbor_error_t cbor_encode_float(cbor_writer_t *writer, float value)
{
	return encode_float(writer, value);
}

cbor_error_t cbor_encode_double(cbor_writer_t *writer, double value)
{
	if (ieee754_is_shrinkable_to_single(value)) {
		return encode_float(writer, (float)value);
	}

	writer->buf[writer->bufidx++] = 0xFB;
	writer->bufidx += cbor_copy(&writer->buf[writer->bufidx],
			(uint8_t const *)&value, sizeof(value));

	return CBOR_SUCCESS;
}
