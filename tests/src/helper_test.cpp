/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "CppUTest/TestHarness_c.h"
#include "CppUTestExt/MockSupport.h"

#include "cbor/cbor.h"

static void parse_cert(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg) {
	mock().actualCall(__func__);
}

static void parse_key(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg) {
	mock().actualCall(__func__);
}

static const struct cbor_parser parsers[] = {
	{ .key = "certificate", .run = parse_cert },
	{ .key = "privateKey",  .run = parse_key },
};

TEST_GROUP(Helper) {
	cbor_reader_t reader;
	cbor_item_t items[256];

	void setup(void) {
		cbor_reader_init(&reader, items, sizeof(items)/sizeof(*items));
	}
	void teardown(void) {
		mock().checkExpectations();
		mock().clear();
	}
};

TEST(Helper, unmarshal_ShouldReturnTrue_WhenSucceed) {
	// {"certificate": "your-cert", "privateKey": "your-private-key" }
	uint8_t msg[] = {
		0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66,
		0x69, 0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F,
		0x75, 0x72, 0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A,
		0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65, 0x4B,
		0x65, 0x79, 0x70, 0x79, 0x6F, 0x75, 0x72, 0x2D,
		0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65, 0x2D,
		0x6B, 0x65, 0x79
	};

	mock().expectOneCall("parse_cert");
	mock().expectOneCall("parse_key");

	cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), 0);
}

TEST(Helper, unmarshal_ShouldReturnFalse_WhenInvalidMessageGiven) {
	uint8_t msg[] = {
		0xA2, 0x6B, 0x63, 0x65, 0x72, 0x74, 0x69, 0x66,
		0x69, 0x63, 0x61, 0x74, 0x65, 0x69, 0x79, 0x6F,
		0x75, 0x72, 0x2D, 0x63, 0x65, 0x72, 0x74, 0x6A,
		0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65, 0x4B,
		0x65, 0x79, 0x70, 0x79, 0x6F, 0x75, 0x72, 0x2D,
		0x70, 0x72, 0x69, 0x76, 0x61, 0x74, 0x65, 0x2D,
		0x6B, 0x65
	};

	LONGS_EQUAL(false, cbor_unmarshal(&reader, parsers,
			sizeof(parsers) / sizeof(*parsers),
			msg, sizeof(msg), 0));
}
