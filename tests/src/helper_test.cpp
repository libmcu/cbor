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
		cbor_reader_init(&reader, items, sizeof(items) / sizeof(items[0]));
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
	LONGS_EQUAL(4, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldHandleNestedIndefiniteContainerAndSibling)
{
	uint8_t msg[] = { 0x82, 0x9f, 0x01, 0xff, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
	LONGS_EQUAL(5, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldKeepSiblingAfterIndefiniteStringInContainer)
{
	uint8_t msg[] = { 0x82, 0x7f, 0x61, 0x61, 0xff, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
	LONGS_EQUAL(5, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(3, scalar_count);
}

TEST(Helper, iterate_ShouldKeepSiblingAfterTopLevelDefiniteArray)
{
	/* array(1)[1] followed by sibling integer 2 */
	uint8_t msg[] = { 0x81, 0x01, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(3, n);
	LONGS_EQUAL(3, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldKeepSiblingAfterTopLevelDefiniteMap)
{
	/* map(1){"a":"b"} followed by sibling integer 2 */
	uint8_t msg[] = { 0xa1, 0x61, 0x61, 0x61, 0x62, 0x02 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
	LONGS_EQUAL(4, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(3, scalar_count);
}

TEST(Helper, iterate_ShouldVisitNoItems_WhenDefiniteArrayIsEmpty)
{
	/* array(0) = [] */
	uint8_t msg[] = { 0x80 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(1, n);
	LONGS_EQUAL(1, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(0, scalar_count);
}

TEST(Helper, iterate_ShouldVisitNoItems_WhenDefiniteMapIsEmpty)
{
	/* map(0) = {} */
	uint8_t msg[] = { 0xa0 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(1, n);
	LONGS_EQUAL(1, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(0, scalar_count);
}

TEST(Helper, iterate_ShouldVisitItemsInIndefiniteMap)
{
	/*
	 * {_ "a": "b"} as a top-level indefinite map.
	 * cbor_parse returns CBOR_BREAK because the message ends with 0xff;
	 * this is the expected, correct return for a top-level indefinite
	 * container that has no trailing sibling items.
	 * cbor_iterate must still visit both key and value.
	 */
	uint8_t msg[] = { 0xbf, 0x61, 0x61, 0x61, 0x62, 0xff };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
	LONGS_EQUAL(4, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(2, scalar_count);
}

TEST(Helper, iterate_ShouldKeepSiblingAfterTopLevelIndefiniteMap)
{
	/* {_ "a": "b"} followed by sibling integer 1 */
	uint8_t msg[] = { 0xbf, 0x61, 0x61, 0x61, 0x62, 0xff, 0x01 };
	size_t n = 0;
	size_t scalar_count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
	LONGS_EQUAL(5, cbor_iterate(&reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(3, scalar_count);
}

TEST(Helper, iterate_ShouldStopIteration_WhenMapSizeWouldOverflow)
{
	/*
	 * SIZE_MAX == (size_t)CBOR_INDEFINITE_VALUE, so the overflow guard
	 * applies to sizes in (SIZE_MAX/2, SIZE_MAX).  Use SIZE_MAX/2+1 as the
	 * minimum value that crosses the boundary.
	 */
	cbor_item_t crafted_items[3];
	cbor_reader_t crafted_reader;
	uint8_t dummy[4] = { 0 };

	cbor_reader_init(&crafted_reader, crafted_items,
			 sizeof(crafted_items) / sizeof(crafted_items[0]));
	crafted_reader.msg = dummy;
	crafted_reader.msgsize = sizeof(dummy);
	crafted_reader.itemidx = 3;

	crafted_items[0] = { CBOR_ITEM_MAP,     0, SIZE_MAX / 2 + 1 };
	crafted_items[1] = { CBOR_ITEM_INTEGER, 1, 1 };
	crafted_items[2] = { CBOR_ITEM_INTEGER, 2, 1 };

	size_t scalar_count = 0;
	LONGS_EQUAL(0, cbor_iterate(&crafted_reader, nullptr, count_scalar_item,
				    &scalar_count));
	LONGS_EQUAL(0, scalar_count);
}

TEST(Helper, iterate_ShouldNotStopIteration_WhenMapSizeIsAtOverflowBoundary)
{
	/*
	 * SIZE_MAX/2 is the last value that does NOT trigger the guard:
	 * size*2 == SIZE_MAX-1, which does not overflow.
	 *
	 * With only 1 remaining node, the MAP recursion visits crafted_items[1]
	 * as the MAP's first declared child (capped by remaining_nodes=1), then
	 * returns.  scalar_count==1 proves stop_iteration was NOT set and the
	 * callback was reached — unlike the overflow case where count stays 0.
	 */
	cbor_item_t crafted_items[2];
	cbor_reader_t crafted_reader;
	uint8_t dummy[4] = { 0 };

	cbor_reader_init(&crafted_reader, crafted_items,
			 sizeof(crafted_items) / sizeof(crafted_items[0]));
	crafted_reader.msg = dummy;
	crafted_reader.msgsize = sizeof(dummy);
	crafted_reader.itemidx = 2;

	crafted_items[0] = { CBOR_ITEM_MAP,     0, SIZE_MAX / 2 };
	crafted_items[1] = { CBOR_ITEM_INTEGER, 0, 0 };

	size_t scalar_count = 0;
	cbor_iterate(&crafted_reader, nullptr, count_scalar_item, &scalar_count);
	LONGS_EQUAL(1, scalar_count);
}

TEST(Helper, unmarshal_ShouldInvokeCallbacks_WhenIndefiniteMapGiven)
{
	/* {_ "certificate": "your-cert", "privateKey": "your-private-key"} */
	uint8_t msg[] = {
		0xBF, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
		0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
		0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
		0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
		0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
		0x74, 0x65, 0x2D, 0x6B, 0x65, 0x79, 0xFF,
	};

	mock().expectOneCall("parse_cert");
	mock().expectOneCall("parse_key");

	LONGS_EQUAL(true, cbor_unmarshal(&reader, parsers,
					 sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldStringifyExcessiveWhenLimitExceeded)
{
	STRCMP_EQUAL("excessive nesting", cbor_stringify_error(CBOR_EXCESSIVE));
}
