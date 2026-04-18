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

static void on_item(const cbor_reader_t *reader,
		    const struct cbor_parser *parser,
		    const cbor_item_t *item, void *arg)
{
	(void)reader;
	(void)item;
	(void)arg;

	const char *name = static_cast<const char *>(parser->path[parser->depth - 1].key.str.ptr);
	mock().actualCall(name);
}

static void on_int_item(const cbor_reader_t *reader,
			const struct cbor_parser *parser,
			const cbor_item_t *item, void *arg)
{
	(void)reader;
	(void)item;
	(void)arg;

	intmax_t key = parser->path[parser->depth - 1].key.idx;
	if (key == 1) {
		mock().actualCall("parse_intkey_1");
	} else if (key == 2) {
		mock().actualCall("parse_intkey_2");
	}
}


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

TEST(Helper, ShouldDispatch_WhenDepth1StringKey)
{
	/* {"certificate": "your-cert", "privateKey": "your-private-key"} */
	static const uint8_t msg[] = {
		0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
		0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
		0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
		0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
		0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
		0x74, 0x65, 0x2D, 0x6B, 0x65, 0x79
	};

	static const struct cbor_path_segment path_cert[] = {
		CBOR_STR_SEG("certificate")
	};
	static const struct cbor_path_segment path_key[] = {
		CBOR_STR_SEG("privateKey")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_cert, on_item),
		CBOR_PATH(path_key,  on_item),
	};

	mock().expectOneCall("certificate");
	mock().expectOneCall("privateKey");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldDispatch_WhenDepth1IntegerKey)
{
	/* {1: "hello", 2: "world"} */
	static const uint8_t msg[] = {
		0xA2, 0x01, 0x65, 0x68, 0x65, 0x6C, 0x6C, 0x6F,
		0x02, 0x65, 0x77, 0x6F, 0x72, 0x6C, 0x64
	};

	static const struct cbor_path_segment path_1[] = { CBOR_INT_SEG(1) };
	static const struct cbor_path_segment path_2[] = { CBOR_INT_SEG(2) };
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_1, on_int_item),
		CBOR_PATH(path_2, on_int_item),
	};

	mock().expectOneCall("parse_intkey_1");
	mock().expectOneCall("parse_intkey_2");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldDispatchCorrectly_WhenSameKeyUnderDifferentParents)
{
	/*
	 * {"dev":  {"ba_id": "evse", "url": "wss://a"},
	 *  "prod": {"ba_id": "user", "url": "wss://b"}}
	 */
	static const uint8_t msg[] = {
		0xA2, 0x63, 0x64, 0x65, 0x76, 0xA2, 0x65, 0x62, 0x61,
		0x5F, 0x69, 0x64, 0x64, 0x65, 0x76, 0x73, 0x65, 0x63,
		0x75, 0x72, 0x6C, 0x67, 0x77, 0x73, 0x73, 0x3A, 0x2F,
		0x2F, 0x61, 0x64, 0x70, 0x72, 0x6F, 0x64, 0xA2, 0x65,
		0x62, 0x61, 0x5F, 0x69, 0x64, 0x64, 0x75, 0x73, 0x65,
		0x72, 0x63, 0x75, 0x72, 0x6C, 0x67, 0x77, 0x73, 0x73,
		0x3A, 0x2F, 0x2F, 0x62
	};

	static const struct cbor_path_segment path_dev_baid[]  = {
		CBOR_STR_SEG("dev"),  CBOR_STR_SEG("ba_id")
	};
	static const struct cbor_path_segment path_dev_url[]   = {
		CBOR_STR_SEG("dev"),  CBOR_STR_SEG("url")
	};
	static const struct cbor_path_segment path_prod_baid[] = {
		CBOR_STR_SEG("prod"), CBOR_STR_SEG("ba_id")
	};
	static const struct cbor_path_segment path_prod_url[]  = {
		CBOR_STR_SEG("prod"), CBOR_STR_SEG("url")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_dev_baid,  on_item),
		CBOR_PATH(path_dev_url,   on_item),
		CBOR_PATH(path_prod_baid, on_item),
		CBOR_PATH(path_prod_url,  on_item),
	};

	mock().expectOneCall("ba_id");
	mock().expectOneCall("url");
	mock().expectOneCall("ba_id");
	mock().expectOneCall("url");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldNotDispatch_WhenPathDoesNotMatch)
{
	/*
	 * {"dev": {"ba_id": "evse"}}
	 * Register only "prod/ba_id" - must NOT fire.
	 */
	static const uint8_t msg[] = {
		0xA1, 0x63, 0x64, 0x65, 0x76, 0xA1, 0x65, 0x62,
		0x61, 0x5F, 0x69, 0x64, 0x64, 0x65, 0x76, 0x73, 0x65
	};

	static const struct cbor_path_segment path_prod_baid[] = {
		CBOR_STR_SEG("prod"), CBOR_STR_SEG("ba_id")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_prod_baid, on_item),
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldDispatch_WhenDepth3ArrayIndex)
{
	/*
	 * {"items": [{"name": "a"}, {"name": "b"}]}
	 * paths: items/[0]/name  and  items/[1]/name
	 */
	static const uint8_t msg[] = {
		0xA1, 0x65, 0x69, 0x74, 0x65, 0x6D, 0x73, 0x82,
		0xA1, 0x64, 0x6E, 0x61, 0x6D, 0x65, 0x61, 0x61,
		0xA1, 0x64, 0x6E, 0x61, 0x6D, 0x65, 0x61, 0x62
	};

	static const struct cbor_path_segment path_item0_name[] = {
		CBOR_STR_SEG("items"), CBOR_IDX_SEG(0), CBOR_STR_SEG("name")
	};
	static const struct cbor_path_segment path_item1_name[] = {
		CBOR_STR_SEG("items"), CBOR_IDX_SEG(1), CBOR_STR_SEG("name")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_item0_name, on_item),
		CBOR_PATH(path_item1_name, on_item),
	};

	mock().expectOneCall("name");
	mock().expectOneCall("name");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldDispatch_WhenDepth4NestedMap)
{
	/*
	 * {"config": {"network": {"wifi": {"ssid": "MyNet", "pass": "secret"},
	 *                         "eth":  {"ip":   "10.0.0.1"}}}}
	 */
	static const uint8_t msg[] = {
		0xA1, 0x66, 0x63, 0x6F, 0x6E, 0x66, 0x69, 0x67,
		0xA1, 0x67, 0x6E, 0x65, 0x74, 0x77, 0x6F, 0x72,
		0x6B, 0xA2, 0x64, 0x77, 0x69, 0x66, 0x69, 0xA2,
		0x64, 0x73, 0x73, 0x69, 0x64, 0x65, 0x4D, 0x79,
		0x4E, 0x65, 0x74, 0x64, 0x70, 0x61, 0x73, 0x73,
		0x66, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74, 0x63,
		0x65, 0x74, 0x68, 0xA1, 0x62, 0x69, 0x70, 0x68,
		0x31, 0x30, 0x2E, 0x30, 0x2E, 0x30, 0x2E, 0x31
	};

	static const struct cbor_path_segment path_wifi_ssid[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("wifi"),   CBOR_STR_SEG("ssid")
	};
	static const struct cbor_path_segment path_wifi_pass[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("wifi"),   CBOR_STR_SEG("pass")
	};
	static const struct cbor_path_segment path_eth_ip[] = {
		CBOR_STR_SEG("config"), CBOR_STR_SEG("network"),
		CBOR_STR_SEG("eth"),    CBOR_STR_SEG("ip")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_wifi_ssid, on_item),
		CBOR_PATH(path_wifi_pass, on_item),
		CBOR_PATH(path_eth_ip,    on_item),
	};

	mock().expectOneCall("ssid");
	mock().expectOneCall("pass");
	mock().expectOneCall("ip");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldReturnFalse_WhenMalformedMessage)
{
	static const uint8_t msg[] = {
		0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
		0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
		0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
		0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
		0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
		0x74, 0x65, 0x2D, 0x6B, 0x65  /* truncated */
	};

	static const struct cbor_path_segment path_cert[] = {
		CBOR_STR_SEG("certificate")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_cert, on_item),
	};

	LONGS_EQUAL(false, cbor_unmarshal(&reader,
					  parsers, sizeof(parsers) / sizeof(*parsers),
					  msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldReturnFalse_WhenParserDepthExceedsLimit)
{
	static const uint8_t msg[] = { 0xA1, 0x61, 0x61, 0x01 };

	/* Manually set depth to CBOR_RECURSION_MAX_LEVEL + 1 */
	static const struct cbor_path_segment path[] = {
		CBOR_STR_SEG("a")
	};
	struct cbor_parser parser = CBOR_PATH(path, on_item);
	parser.depth = CBOR_RECURSION_MAX_LEVEL + 1;

	LONGS_EQUAL(false, cbor_unmarshal(&reader,
					  &parser, 1,
					  msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldDispatch_WhenWildcardMatchesAnyArrayElement)
{
	/* {"items": [10, 20, 30]} */
	static const uint8_t msg[] = {
		0xA1, 0x65, 0x69, 0x74, 0x65, 0x6D, 0x73,
		0x83, 0x0A, 0x14, 0x18, 0x1E
	};

	static const struct cbor_path_segment path[] = {
		CBOR_STR_SEG("items"), CBOR_ANY_SEG()
	};
	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	const struct cbor_parser parsers[] = {
		{ path, sizeof(path)/sizeof(*path), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(3, count);
}

TEST(Helper, ShouldDispatch_WhenWildcardMatchesAnyTopLevelMapValue)
{
	/* {"a": 1, "b": 2, "c": 3} */
	static const uint8_t msg[] = {
		0xA3,
		0x61, 0x61, 0x01,
		0x61, 0x62, 0x02,
		0x61, 0x63, 0x03
	};

	static const struct cbor_path_segment path[] = { CBOR_ANY_SEG() };
	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	const struct cbor_parser parsers[] = {
		{ path, sizeof(path)/sizeof(*path), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(3, count);
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
	static const uint8_t msg[] = {
		0xBF, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66, 0x69,
		0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F, 0x75, 0x72,
		0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A, 0x70, 0x72, 0x69,
		0x76, 0x61, 0x74, 0x65, 0x4B, 0x65, 0x79, 0x70, 0x79,
		0x6F, 0x75, 0x72, 0x2D, 0x70, 0x72, 0x69, 0x76, 0x61,
		0x74, 0x65, 0x2D, 0x6B, 0x65, 0x79, 0xFF,
	};

	static const struct cbor_path_segment path_cert[] = {
		CBOR_STR_SEG("certificate")
	};
	static const struct cbor_path_segment path_key[] = {
		CBOR_STR_SEG("privateKey")
	};
	static const struct cbor_parser parsers[] = {
		CBOR_PATH(path_cert, on_item),
		CBOR_PATH(path_key,  on_item),
	};

	mock().expectOneCall("certificate");
	mock().expectOneCall("privateKey");

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldStringifyExcessiveWhenLimitExceeded)
{
	STRCMP_EQUAL("excessive nesting", cbor_stringify_error(CBOR_EXCESSIVE));
}

TEST(Helper, ShouldReturnFalse_WhenParserPathIsNullWithNonZeroDepth)
{
	static const uint8_t msg[] = { 0xA1, 0x61, 0x61, 0x01 };

	struct cbor_parser parser = { NULL, 1, on_item };

	LONGS_EQUAL(false, cbor_unmarshal(&reader,
					  &parser, 1,
					  msg, sizeof(msg), nullptr));
}

TEST(Helper, ShouldSkipDispatch_WhenMapKeyIsNonStringNonInteger)
{
	/* {1.5: 42} — float key, value 42 */
	static const uint8_t msg[] = {
		0xA1,                   /* map(1) */
		0xF9, 0x3E, 0x00,       /* float16(1.5) as key */
		0x18, 0x2A              /* unsigned(42) as value */
	};

	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	static const struct cbor_path_segment path[] = { CBOR_ANY_SEG() };
	const struct cbor_parser parsers[] = {
		{ path, sizeof(path)/sizeof(*path), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(0, count);
}

TEST(Helper, ShouldSkipDispatch_WhenMapKeyIsArray)
{
	/* {[1, 2]: 42} — array key, value 42 */
	static const uint8_t msg[] = {
		0xA1,                   /* map(1) */
		0x82, 0x01, 0x02,       /* [1, 2] as key */
		0x18, 0x2A              /* unsigned(42) as value */
	};

	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	static const struct cbor_path_segment path[] = { CBOR_ANY_SEG() };
	const struct cbor_parser parsers[] = {
		{ path, sizeof(path)/sizeof(*path), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(0, count);
}

TEST(Helper, ShouldNotDispatchInsideContainerKey)
{
	/* {[99]: 42, "x": 1} — array key followed by a valid string key */
	static const uint8_t msg[] = {
		0xA2,                   /* map(2) */
		0x81, 0x18, 0x63,       /* [99] as key */
		0x18, 0x2A,             /* unsigned(42) as value */
		0x61, 0x78,             /* "x" as key */
		0x01                    /* 1 as value */
	};

	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	static const struct cbor_path_segment path_x[] = { CBOR_STR_SEG("x") };
	const struct cbor_parser parsers[] = {
		{ path_x, sizeof(path_x)/sizeof(*path_x), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(1, count);
}

TEST(Helper, ShouldDispatch_WhenTaggedValueUnderStringKey)
{
	/* {"ts": tag(1, 1000000000)} */
	static const uint8_t msg[] = {
		0xA1,                           /* map(1) */
		0x62, 0x74, 0x73,               /* "ts" */
		0xC1, 0x1A, 0x3B, 0x9A, 0xCA, 0x00  /* tag(1, 1000000000) */
	};

	int count = 0;
	auto counter_cb = [](const cbor_reader_t *, const struct cbor_parser *,
			     const cbor_item_t *, void *arg) {
		(*static_cast<int *>(arg))++;
	};
	static const struct cbor_path_segment path[] = { CBOR_STR_SEG("ts") };
	const struct cbor_parser parsers[] = {
		{ path, sizeof(path)/sizeof(*path), counter_cb },
	};

	LONGS_EQUAL(true, cbor_unmarshal(&reader,
					 parsers, sizeof(parsers) / sizeof(*parsers),
					 msg, sizeof(msg), &count));
	LONGS_EQUAL(1, count);
}
