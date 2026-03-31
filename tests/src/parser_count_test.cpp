#include "CppUTest/TestHarness.h"

#include "cbor/parser.h"

TEST_GROUP(ParserCount){};

TEST(ParserCount, ShouldCountItemsWithoutBuffer_WhenValidMessageGiven)
{
	uint8_t msg[] = { 0xA2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x02 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(5, n);
}

TEST(ParserCount, ShouldCountItemsEvenWhenParseBufferWouldOverrun)
{
	uint8_t msg[] = { 0x84, 0x01, 0x02, 0x03, 0x04 };
	cbor_reader_t reader;
	cbor_item_t items[2];
	size_t parsed = 0;
	size_t counted = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_OVERRUN,
		    cbor_parse(&reader, msg, sizeof(msg), &parsed));
	LONGS_EQUAL(2, parsed);

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &counted));
	LONGS_EQUAL(5, counted);
}

TEST(ParserCount, ShouldReturnIllegal_WhenMessageIsIncomplete)
{
	uint8_t msg[] = { 0x18 };
	size_t n = 1234;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(0, n);
}

TEST(ParserCount, ShouldContinueCountingAfterBreak_WhenMoreTopLevelItemsExist)
{
	uint8_t msg[] = { 0x9f, 0x01, 0xff, 0x02 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

TEST(ParserCount, ShouldReturnIllegal_WhenDefiniteArrayIsTruncated)
{
	uint8_t msg[] = { 0x82, 0x01 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
}

TEST(ParserCount, ShouldReturnIllegal_WhenDefiniteMapIsTruncated)
{
	uint8_t msg[] = { 0xa1, 0x01 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
}

TEST(ParserCount, ShouldReturnIllegal_WhenIndefiniteArrayMissingBreak)
{
	uint8_t msg[] = { 0x9f, 0x01 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(2, n);
}
