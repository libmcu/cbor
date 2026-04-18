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

/* ------------------------------------------------------------------ */
/* Helpers shared by all groups                                         */
/* ------------------------------------------------------------------ */

static void count_item(const cbor_reader_t *, const cbor_item_t *,
		const cbor_item_t *, void *arg)
{
	(*static_cast<size_t *>(arg))++;
}

/* ------------------------------------------------------------------ */
/* Parser group                                                         */
/* ------------------------------------------------------------------ */

TEST_GROUP(TagParser)
{
	cbor_reader_t reader;
	cbor_item_t items[32];
	size_t n;

	void setup(void)
	{
		cbor_reader_init(&reader, items,
				sizeof(items) / sizeof(items[0]));
		n = 0;
	}
	void teardown(void) {}
};

TEST(TagParser, ShouldParseTag_WhenSimpleTagGiven)
{
	/* tag(1, 0)  →  0xC1 0x00 */
	uint8_t msg[] = { 0xC1, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[0].type);
	LONGS_EQUAL(1,                 items[0].size);  /* tag number */
	LONGS_EQUAL(0,                 items[0].offset);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[1].type);
}

TEST(TagParser, ShouldParseNestedTags_WhenTagWrapsAnotherTag)
{
	/* tag(1, tag(2, 0))  →  0xC1 0xC2 0x00 */
	uint8_t msg[] = { 0xC1, 0xC2, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(3, n);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[0].type);
	LONGS_EQUAL(1,                 items[0].size);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[1].type);
	LONGS_EQUAL(2,                 items[1].size);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[2].type);
}

TEST(TagParser, ShouldParseTagInsideArray)
{
	/* [tag(1, 0)]  →  0x81 0xC1 0x00 */
	uint8_t msg[] = { 0x81, 0xC1, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(3, n);
	LONGS_EQUAL(CBOR_ITEM_ARRAY,   items[0].type);
	LONGS_EQUAL(1,                 items[0].size);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[1].type);
	LONGS_EQUAL(1,                 items[1].size);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[2].type);
}

TEST(TagParser, ShouldParseTagInMapValue)
{
	/* {"k": tag(1, 42)}  →  0xA1 0x61 0x6B 0xC1 0x18 0x2A */
	uint8_t msg[] = { 0xA1, 0x61, 0x6B, 0xC1, 0x18, 0x2A };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
	LONGS_EQUAL(CBOR_ITEM_MAP,     items[0].type);
	LONGS_EQUAL(CBOR_ITEM_STRING,  items[1].type);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[2].type);
	LONGS_EQUAL(1,                 items[2].size);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[3].type);
}

TEST(TagParser, ShouldParseTagWrappingArray)
{
	/* tag(1, [1, 2])  →  0xC1 0x82 0x01 0x02 */
	uint8_t msg[] = { 0xC1, 0x82, 0x01, 0x02 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[0].type);
	LONGS_EQUAL(CBOR_ITEM_ARRAY,   items[1].type);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[2].type);
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[3].type);
}

TEST(TagParser, ShouldParseMultiByteTagNumber)
{
	/* tag(0x1000, 0)  →  0xD9 0x10 0x00 0x00 */
	uint8_t msg[] = { 0xD9, 0x10, 0x00, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
	LONGS_EQUAL(CBOR_ITEM_TAG, items[0].type);
	LONGS_EQUAL(0x1000,        items[0].size);
}

TEST(TagParser, ShouldReturnIllegal_WhenTagHasNoFollowingItem)
{
	/* tag(1) with nothing after it */
	uint8_t msg[] = { 0xC1 };

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));
}

TEST(TagParser, ShouldReturnIllegal_WhenIndefiniteTagGiven)
{
	/* 0xDF = major 6, additional_info 31 — indefinite tag (invalid) */
	uint8_t msg[] = { 0xDF };

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));
}

TEST(TagParser, ShouldReturnOverrun_WhenItemBufferTooSmallForTag)
{
	/* tag(1, 0): needs 2 item slots; give only 1 */
	cbor_item_t tiny[1];
	cbor_reader_t r;
	cbor_reader_init(&r, tiny, 1);

	uint8_t msg[] = { 0xC1, 0x00 };
	size_t parsed = 0;

	LONGS_EQUAL(CBOR_OVERRUN, cbor_parse(&r, msg, sizeof(msg), &parsed));
}

TEST(TagParser, ShouldCountTagItems_WhenCountingItems)
{
	/* tag(1, 0) — both TAG and INT must be counted */
	uint8_t msg[] = { 0xC1, 0x00 };
	size_t counted = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &counted));
	LONGS_EQUAL(2, counted);
}

TEST(TagParser, ShouldCountNestedTagItems)
{
	/* tag(1, tag(2, 0)) = 3 items */
	uint8_t msg[] = { 0xC1, 0xC2, 0x00 };
	size_t counted = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &counted));
	LONGS_EQUAL(3, counted);
}

TEST(TagParser, ShouldStringifyTag)
{
	cbor_item_t tag = { CBOR_ITEM_TAG, 0, 1 };
	STRCMP_EQUAL("tag", cbor_stringify_item(&tag));
}

TEST(TagParser, ShouldReturnTagNumber_WhenGetTagNumberCalled)
{
	/* tag(1, 0) */
	uint8_t msg[] = { 0xC1, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(CBOR_ITEM_TAG, items[0].type);
	LONGS_EQUAL(1, cbor_get_tag_number(&items[0]));
}

TEST(TagParser, ShouldReturnTagNumber_WhenGetItemSizeCalledForTag)
{
	uint8_t msg[] = { 0xC1, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(CBOR_ITEM_TAG, items[0].type);
	LONGS_EQUAL(1, cbor_get_item_size(&items[0]));
}

TEST(TagParser, ShouldReturnInvalid_WhenDecodingTagItem)
{
	uint8_t msg[] = { 0xC1, 0x00 };
	uint32_t out = 0x12345678;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(CBOR_ITEM_TAG, items[0].type);
	LONGS_EQUAL(CBOR_INVALID,
		    cbor_decode(&reader, &items[0], &out, sizeof(out)));
	LONGS_EQUAL(0x12345678, out);
}

TEST(TagParser, ShouldDecodeWrappedValue_WhenTaggedIntegerGiven)
{
	/* tag(1, 1234567890)  →  0xC1 0x1A 0x49 0x96 0x02 0xD2 */
	uint8_t msg[] = { 0xC1, 0x1A, 0x49, 0x96, 0x02, 0xD2 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
	LONGS_EQUAL(CBOR_ITEM_TAG,     items[0].type);
	LONGS_EQUAL(1,                 cbor_get_tag_number(&items[0]));
	LONGS_EQUAL(CBOR_ITEM_INTEGER, items[1].type);

	uint32_t timestamp = 0;
	LONGS_EQUAL(CBOR_SUCCESS,
		    cbor_decode(&reader, &items[1], &timestamp, sizeof(timestamp)));
	LONGS_EQUAL(1234567890, timestamp);
}

TEST(TagParser, ShouldDecodeWrappedString_WhenTaggedTextStringGiven)
{
	/* tag(0, "t")  →  0xC0 0x61 0x74 */
	uint8_t msg[] = { 0xC0, 0x61, 0x74 };
	char buf[8] = {};

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
	LONGS_EQUAL(0, cbor_get_tag_number(&items[0]));
	LONGS_EQUAL(CBOR_ITEM_STRING, items[1].type);

	LONGS_EQUAL(CBOR_SUCCESS,
		    cbor_decode(&reader, &items[1], buf, sizeof(buf)));
	LONGS_EQUAL('t', buf[0]);
}

/* ------------------------------------------------------------------ */
/* Encoder group                                                        */
/* ------------------------------------------------------------------ */

TEST_GROUP(TagEncoder)
{
	cbor_writer_t writer;
	uint8_t buf[64];

	void setup(void)
	{
		cbor_writer_init(&writer, buf, sizeof(buf));
	}
	void teardown(void) {}
};

TEST(TagEncoder, ShouldEncodeTag_WhenSmallTagNumberGiven)
{
	/* tag(1) → 0xC1 */
	const uint8_t expected[] = { 0xC1 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 1));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldEncodeTagWithUnsignedInteger)
{
	/* tag(1, 0) → 0xC1 0x00 */
	const uint8_t expected[] = { 0xC1, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 1));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer, 0));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldEncodeTagWithTextString)
{
	/* tag(0, "t") → 0xC0 0x61 0x74 */
	const uint8_t expected[] = { 0xC0, 0x61, 0x74 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 0));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_text_string(&writer, "t", 1));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldEncodeTwoByteTagNumber)
{
	/* tag(0x1000) → 0xD9 0x10 0x00 */
	const uint8_t expected[] = { 0xD9, 0x10, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 0x1000));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldEncodeFourByteTagNumber)
{
	/* tag(0x10000) → 0xDA 0x00 0x01 0x00 0x00 */
	const uint8_t expected[] = { 0xDA, 0x00, 0x01, 0x00, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 0x10000));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldEncodeNestedTags)
{
	/* tag(1, tag(2, 0)) → 0xC1 0xC2 0x00 */
	const uint8_t expected[] = { 0xC1, 0xC2, 0x00 };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 1));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_tag(&writer, 2));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_encode_unsigned_integer(&writer, 0));
	LONGS_EQUAL(sizeof(expected), cbor_writer_len(&writer));
	MEMCMP_EQUAL(expected, cbor_writer_get_encoded(&writer),
			sizeof(expected));
}

TEST(TagEncoder, ShouldReturnOverrun_WhenBufferTooSmall)
{
	uint8_t tiny[1];
	cbor_writer_t w;
	cbor_writer_init(&w, tiny, 0u);

	LONGS_EQUAL(CBOR_OVERRUN, cbor_encode_tag(&w, 1));
}

/* ------------------------------------------------------------------ */
/* Iterate group                                                        */
/* ------------------------------------------------------------------ */

TEST_GROUP(TagIterate)
{
	cbor_reader_t reader;
	cbor_item_t items[32];

	void setup(void)
	{
		cbor_reader_init(&reader, items,
				sizeof(items) / sizeof(items[0]));
	}
	void teardown(void) {}
};

TEST(TagIterate, ShouldVisitTagAndWrappedItem)
{
	/* tag(1, 0) → callback for TAG + callback for INT */
	uint8_t msg[] = { 0xC1, 0x00 };
	size_t n = 0;
	size_t count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	cbor_iterate(&reader, nullptr, count_item, &count);
	LONGS_EQUAL(2, count);
}

TEST(TagIterate, ShouldVisitBothNestedTags)
{
	/* tag(1, tag(2, 0)) → 3 callbacks */
	uint8_t msg[] = { 0xC1, 0xC2, 0x00 };
	size_t n = 0;
	size_t count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	cbor_iterate(&reader, nullptr, count_item, &count);
	LONGS_EQUAL(3, count);
}

TEST(TagIterate, ShouldVisitTagInsideArray)
{
	/* [tag(1, 0)] → TAG + INT visible; ARRAY container not leaf-counted */
	uint8_t msg[] = { 0x81, 0xC1, 0x00 };
	size_t n = 0;
	size_t count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(3, cbor_iterate(&reader, nullptr, count_item, &count));
	LONGS_EQUAL(2, count);  /* TAG + INT; ARRAY is a container, no leaf callback */
}

TEST(TagIterate, ShouldHandleTagFollowedBySibling)
{
	/* tag(1, 0), 99 — sibling after tagged item */
	uint8_t msg[] = { 0xC1, 0x00, 0x18, 0x63 };
	size_t n = 0;
	size_t count = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	cbor_iterate(&reader, nullptr, count_item, &count);
	LONGS_EQUAL(3, count);  /* TAG + INT(0) + INT(99) */
}

/* ------------------------------------------------------------------ */
/* Unmarshal group                                                      */
/* ------------------------------------------------------------------ */

static void on_value(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg)
{
	(void)reader;
	(void)parser;
	mock().actualCall("on_value")
		.withParameter("type", (int)item->type);
	if (arg) {
		int64_t *out = static_cast<int64_t *>(arg);
		cbor_decode(reader, item, out, sizeof(*out));
	}
}

static const struct cbor_path_segment path_k[] = { CBOR_STR_SEG("k") };
static const struct cbor_parser kv_parsers[] = {
	CBOR_PATH(path_k, on_value),
};

TEST_GROUP(TagUnmarshal)
{
	cbor_reader_t reader;
	cbor_item_t items[32];

	void setup(void)
	{
		cbor_reader_init(&reader, items,
				sizeof(items) / sizeof(items[0]));
	}
	void teardown(void)
	{
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(TagUnmarshal, ShouldDispatchWrappedItem_WhenMapValueIsTagged)
{
	/* {"k": tag(1, 42)}  →  0xA1 0x61 0x6B 0xC1 0x18 0x2A */
	uint8_t msg[] = { 0xA1, 0x61, 0x6B, 0xC1, 0x18, 0x2A };
	int64_t got = 0;

	mock().expectOneCall("on_value")
		.withParameter("type", (int)CBOR_ITEM_INTEGER);

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
			kv_parsers, sizeof(kv_parsers) / sizeof(*kv_parsers),
			msg, sizeof(msg), &got));
	LONGS_EQUAL(42, got);
}

TEST(TagUnmarshal, ShouldDispatchWrappedItem_WhenValueHasNestedTags)
{
	/* {"k": tag(1, tag(2, 7))}  →  0xA1 0x61 0x6B 0xC1 0xC2 0x07 */
	uint8_t msg[] = { 0xA1, 0x61, 0x6B, 0xC1, 0xC2, 0x07 };
	int64_t got = 0;

	mock().expectOneCall("on_value")
		.withParameter("type", (int)CBOR_ITEM_INTEGER);

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
			kv_parsers, sizeof(kv_parsers) / sizeof(*kv_parsers),
			msg, sizeof(msg), &got));
	LONGS_EQUAL(7, got);
}

TEST(TagUnmarshal, ShouldNotDispatch_WhenOnlyTagWithNoValueInMap)
{
	/* Malformed: {"k": tag(1)} — tag with no following item: parse fails */
	uint8_t msg[] = { 0xA1, 0x61, 0x6B, 0xC1 };

	LONGS_EQUAL(false, cbor_unmarshal(&reader,
			kv_parsers, sizeof(kv_parsers) / sizeof(*kv_parsers),
			msg, sizeof(msg), nullptr));
}
