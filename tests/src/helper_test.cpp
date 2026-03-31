/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include <cstdint>
#include <cstring>
#include <iterator>

#include "cbor/cbor.h"

static const char *get_mock_name(const struct cbor_parser *parser)
{
	if (parser->keylen == 11 &&
	    std::memcmp(parser->key, "certificate", parser->keylen) == 0) {
		return "parse_cert";
	}

	if (parser->keylen == 10 &&
	    std::memcmp(parser->key, "privateKey", parser->keylen) == 0) {
		return "parse_key";
	}

	if (parser->keylen == 7 &&
	    (std::memcmp(parser->key, "strkey1", parser->keylen) == 0 ||
	     std::memcmp(parser->key, "strkey2", parser->keylen) == 0)) {
		return "parse_strkey";
	}

	if ((std::intptr_t)parser->key == 1 ||
	    (std::intptr_t)parser->key == 2) {
		return "parse_intkey";
	}

	return "parse_unknown";
}

static void parse_item(const cbor_reader_t *reader,
		       const struct cbor_parser *parser,
		       const cbor_item_t *item, void *arg)
{
	(void)reader;
	(void)item;
	(void)arg;

	mock().actualCall(get_mock_name(parser));
}

static const struct cbor_parser parsers[] = {
	{ .key = "certificate", .keylen = 11, .run = parse_item },
	{ .key = "privateKey", .keylen = 10, .run = parse_item },
	{ .key = "strkey1", .keylen = 7, .run = parse_item },
	{ .key = (const void *)1, .run = parse_item },
	{ .key = "strkey2", .keylen = 7, .run = parse_item },
	{ .key = (const void *)2, .run = parse_item },
};

static void count_scalar_item(const cbor_reader_t *reader,
			      const cbor_item_t *item,
			      const cbor_item_t *parent, void *arg)
{
	(void)reader;
	(void)item;
	(void)parent;

	auto *count = static_cast<size_t *>(arg);
	(*count)++;
}

TEST_GROUP(Helper)
{
	cbor_reader_t reader;
	cbor_item_t items[256];

	void setup(void)
	{
		cbor_reader_init(&reader, items, std::size(items));
	}
	void teardown(void)
	{
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(Helper, unmarshal_ShouldReturnTrue_WhenSucceed)
{
	// {"certificate": "your-cert", "privateKey": "your-private-key" }
	uint8_t msg[] = { 0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
			  0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
			  0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
			  0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
			  0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
			  0x74, 0x65, 0x2D, 0x6B, 0x65, 0x79 };

	mock().expectOneCall("parse_cert");
	mock().expectOneCall("parse_key");

	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
		       msg, sizeof(msg), nullptr);
}

TEST(Helper, unmarshal_ShouldReturnFalse_WhenInvalidMessageGiven)
{
	uint8_t msg[] = { 0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
			  0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
			  0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
			  0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
			  0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
			  0x74, 0x65, 0x2D, 0x6B, 0x65 };

	LONGS_EQUAL(false, cbor_unmarshal(&reader, parsers,
					  sizeof(parsers) / sizeof(*parsers),
					  msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldUnmarshal_WhenBothOfStrKeyAndIntKeyGiven)
{
	uint8_t msg[] = {
		0xA4, 0x01, 0x66, 0x69, 0x6E, 0x74, 0x6B, 0x65,
		0x79, 0x67, 0x73, 0x74, 0x72, 0x6B, 0x65, 0x79,
		0x32, 0x63, 0x61, 0x62, 0x63, 0x02, 0x01, 0x67,
		0x73, 0x74, 0x72, 0x6B, 0x65, 0x79, 0x31, 0x02,
	};

	mock().expectNCalls(2, "parse_intkey");
	mock().expectNCalls(2, "parse_strkey");

	LONGS_EQUAL(true, cbor_unmarshal(&reader, parsers,
					 sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, iterate_ShouldKeepSiblingAfterTopLevelIndefiniteContainer)
{
	uint8_t msg[] = { 0x9f, 0x01, 0xff, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
	(void)cbor_iterate(&reader, nullptr, count_scalar_item, &scalar_count);
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldHandleNestedIndefiniteContainerAndSibling)
{
	uint8_t msg[] = { 0x82, 0x9f, 0x01, 0xff, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
	(void)cbor_iterate(&reader, nullptr, count_scalar_item, &scalar_count);
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldKeepSiblingAfterIndefiniteStringInContainer)
{
	uint8_t msg[] = { 0x82, 0x7f, 0x61, 0x61, 0xff, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
	(void)cbor_iterate(&reader, nullptr, count_scalar_item, &scalar_count);
	LONGS_EQUAL(3, scalar_count);
}
