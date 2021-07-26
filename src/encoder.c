#include "cbor/encoder.h"

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

static cbor_error_t encode_integer(cbor_writer_t *writer,
		uint64_t value, bool negative)
{
	uint8_t *buf = &writer->buf[writer->bufidx];
	uint8_t major_type = negative? 1u << MAJOR_TYPE_BIT : 0;
	uint8_t additional_info = get_additional_info(value);
	uint8_t following_bytes = cbor_get_following_bytes(additional_info);

	if ((following_bytes + 1) > (writer->bufsize - writer->bufidx)) {
		return CBOR_OVERRUN;
	}

	buf[0] = major_type | additional_info;
	cbor_copy(&buf[1], (const uint8_t *)&value, following_bytes);

	writer->bufidx += following_bytes + 1;
	
	return CBOR_SUCCESS;
}

static cbor_error_t encode_string(cbor_writer_t *writer, bool is_text,
		const uint8_t *data, size_t datasize, bool indefinite)
{
	uint8_t *buf = &writer->buf[writer->bufidx];
	uint8_t major_type = (uint8_t)((is_text? 3u : 2u) << MAJOR_TYPE_BIT);
	uint8_t additional_info = get_additional_info(datasize);
	uint8_t following_bytes = cbor_get_following_bytes(additional_info);

	if (indefinite) {
		additional_info = 31;
		following_bytes = 0;
	}

	if ((datasize + following_bytes + 1)
			> (writer->bufsize - writer->bufidx)) {
		return CBOR_OVERRUN;
	}

	buf[0] = major_type | additional_info;
	cbor_copy(&buf[1], (const uint8_t *)&datasize, following_bytes);
	cbor_copy_be(&buf[1 + following_bytes], data, datasize);

	writer->bufidx += datasize + following_bytes + 1;

	return CBOR_SUCCESS;
}

static cbor_error_t encode_array(cbor_writer_t *writer,
		size_t length, bool indefinite)
{
	uint8_t *buf = &writer->buf[writer->bufidx];
	uint8_t major_type = 4u << MAJOR_TYPE_BIT;
	uint8_t additional_info = get_additional_info(length);
	uint8_t following_bytes = cbor_get_following_bytes(additional_info);

	if (indefinite) {
		additional_info = 31;
		following_bytes = 0;
	}

	if ((following_bytes + 1) > (writer->bufsize - writer->bufidx)) {
		return CBOR_OVERRUN;
	}

	buf[0] = major_type | additional_info;
	cbor_copy(&buf[1], (const uint8_t *)&length, following_bytes);

	writer->bufidx += following_bytes + 1;
	
	return CBOR_SUCCESS;
}

cbor_error_t cbor_encode_unsigned_integer(cbor_writer_t *writer, uint64_t value)
{
	return encode_integer(writer, value, false);
}

cbor_error_t cbor_encode_negative_integer(cbor_writer_t *writer, int64_t value)
{
	return encode_integer(writer, ((uint64_t)-value) - 1, true);
}

cbor_error_t cbor_encode_byte_string(cbor_writer_t *writer,
		const uint8_t *data, size_t datasize)
{
	return encode_string(writer, false, data, datasize, false);
}

cbor_error_t cbor_encode_byte_string_indefinite(cbor_writer_t *writer)
{
	return encode_string(writer, false, NULL, 0, true);
}

cbor_error_t cbor_encode_text_string(cbor_writer_t *writer,
		const char *text, size_t textsize)
{
	return encode_string(writer, true,
			(const uint8_t *)text, textsize, false);
}

cbor_error_t cbor_encode_text_string_indefinite(cbor_writer_t *writer)
{
	return encode_string(writer, true, NULL, 0, true);
}

cbor_error_t cbor_encode_array(cbor_writer_t *writer, size_t length)
{
	return encode_array(writer, length, false);
}

cbor_error_t cbor_encode_array_indefinite(cbor_writer_t *writer)
{
	return encode_array(writer, 0, true);
}
