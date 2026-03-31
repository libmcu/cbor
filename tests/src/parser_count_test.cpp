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

TEST(ParserCount,
     ShouldCountNestedIndefiniteString_WhenSiblingItemFollowsInContainer)
{
	uint8_t msg[] = { 0x82, 0x7f, 0x61, 0x61, 0x61, 0x62, 0xff, 0x01 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(6, n);
}

TEST(ParserCount, ShouldReturnOverrun_WhenIndefiniteStringExceedsItemBuffer)
{
	uint8_t msg[] = { 0x7f, 0x61, 0x61, 0xff };
	cbor_reader_t limited_reader;
	cbor_item_t limited_items[2];
	size_t parsed = 0;

	cbor_reader_init(&limited_reader, limited_items,
			 sizeof(limited_items) / sizeof(*limited_items));
	LONGS_EQUAL(CBOR_OVERRUN,
		    cbor_parse(&limited_reader, msg, sizeof(msg), &parsed));
	LONGS_EQUAL(2, parsed);
}

TEST(ParserCount, ShouldReturnOverrun_WhenIndefiniteArrayExceedsItemBuffer)
{
	uint8_t msg[] = { 0x9f, 0x01, 0xff };
	cbor_reader_t limited_reader;
	cbor_item_t limited_items[2];
	size_t parsed = 0;

	cbor_reader_init(&limited_reader, limited_items,
			 sizeof(limited_items) / sizeof(*limited_items));
	LONGS_EQUAL(CBOR_OVERRUN,
		    cbor_parse(&limited_reader, msg, sizeof(msg), &parsed));
	LONGS_EQUAL(2, parsed);
}

TEST(ParserCount,
     ShouldReturnIllegal_WhenDefiniteMapMissingValueAfterComplexKey)
{
	uint8_t msg[] = { 0xa1, 0x82, 0x01, 0x02 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

TEST(ParserCount,
     ShouldReturnIllegal_WhenDefiniteArrayMissingItemAfterComplexElement)
{
	uint8_t msg[] = { 0x82, 0x82, 0x01, 0x02 };
	size_t n = 0;

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

TEST(ParserCount,
	     ShouldParseSuccessfully_WhenDefiniteArrayContainsIndefiniteTextString)
{
	uint8_t msg[] = { 0x81, 0x7f, 0x61, 0x61, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

TEST(ParserCount,
	     ShouldCountSuccessfully_WhenDefiniteArrayContainsIndefiniteTextString)
{
	uint8_t msg[] = { 0x81, 0x7f, 0x61, 0x61, 0xff };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

TEST(ParserCount,
	     ShouldReturnIllegal_WhenDefiniteArrayIsMissingItemAfterIndefiniteTextString)
{
	uint8_t msg[] = { 0x82, 0x7f, 0x61, 0x61, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(4, n);
}

/* Regression tests: nested indefinite container BREAK must not terminate the
 * outer indefinite container prematurely. */

TEST(ParserCount,
     ShouldParseSuccessfully_WhenIndefiniteArrayContainsIndefiniteTextString)
{
	/* [_ (_ "a") 2 ]
	 * 9f         -- indefinite array
	 *   7f       -- indefinite text string
	 *     61 61  -- chunk "a"
	 *     ff     -- BREAK (inner string)
	 *   02       -- integer 2
	 *   ff       -- BREAK (outer array)
	 */
	uint8_t msg[] = { 0x9f, 0x7f, 0x61, 0x61, 0xff, 0x02, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	/* cbor_parse returns CBOR_BREAK when the top-level container is
	 * indefinite-length; inner indefinite strings must not terminate it
	 * prematurely. */
	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(6, n);
}

TEST(ParserCount,
     ShouldCountSuccessfully_WhenIndefiniteArrayContainsIndefiniteTextString)
{
	/* [_ (_ "a") 2 ] */
	uint8_t msg[] = { 0x9f, 0x7f, 0x61, 0x61, 0xff, 0x02, 0xff };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(6, n);
}

TEST(ParserCount,
     ShouldParseSuccessfully_WhenIndefiniteMapContainsIndefiniteByteString)
{
	/* {_ "a" h'01'(indef) "b" 2 }
	 * bf            -- indefinite map
	 *   61 61       -- key "a"
	 *   5f          -- indefinite byte string
	 *     41 01     -- chunk h'01'
	 *     ff        -- BREAK (inner byte string)
	 *   61 62       -- key "b"
	 *   02          -- value 2
	 *   ff          -- BREAK (outer map)
	 */
	uint8_t msg[] = { 0xbf, 0x61, 0x61, 0x5f, 0x41, 0x01,
			  0xff, 0x61, 0x62, 0x02, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	/* cbor_parse returns CBOR_BREAK for indefinite-length top container. */
	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(8, n);
}

TEST(ParserCount,
     ShouldCountSuccessfully_WhenIndefiniteMapContainsIndefiniteByteString)
{
	/* {_ "a" h'01'(indef) "b" 2 } */
	uint8_t msg[] = { 0xbf, 0x61, 0x61, 0x5f, 0x41, 0x01,
			  0xff, 0x61, 0x62, 0x02, 0xff };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(8, n);
}

TEST(ParserCount,
     ShouldParseSuccessfully_WhenIndefiniteArrayContainsNestedIndefiniteArray)
{
	/* [_ [_ 1] 2 ]
	 * 9f      -- outer indefinite array
	 *   9f    -- inner indefinite array
	 *     01  -- integer 1
	 *     ff  -- BREAK (inner array)
	 *   02    -- integer 2
	 *   ff    -- BREAK (outer array)
	 */
	uint8_t msg[] = { 0x9f, 0x9f, 0x01, 0xff, 0x02, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	/* cbor_parse returns CBOR_BREAK for indefinite-length top container. */
	LONGS_EQUAL(CBOR_BREAK, cbor_parse(&reader, msg, sizeof(msg), &n));
	LONGS_EQUAL(6, n);
}

TEST(ParserCount,
     ShouldCountSuccessfully_WhenIndefiniteArrayContainsNestedIndefiniteArray)
{
	/* [_ [_ 1] 2 ] */
	uint8_t msg[] = { 0x9f, 0x9f, 0x01, 0xff, 0x02, 0xff };
	size_t n = 0;

	LONGS_EQUAL(CBOR_SUCCESS, cbor_count_items(msg, sizeof(msg), &n));
	LONGS_EQUAL(6, n);
}

TEST(ParserCount,
     ShouldReturnIllegal_WhenOuterIndefiniteArrayMissingBreakAfterNestedBreak)
{
	uint8_t msg[] = { 0x9f, 0x7f, 0x61, 0x61, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[16];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}

/* Regression tests: BREAK byte inside a definite-length container must be
 * treated as CBOR_ILLEGAL, not propagated as CBOR_BREAK to the caller. */

TEST(ParserCount, ShouldReturnIllegal_WhenDefiniteArrayContainsBreakByte)
{
	/* [1] encoded as 0x81 but with 0xff (BREAK) as the sole item */
	uint8_t msg[] = { 0x81, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[4];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}

TEST(ParserCount, ShouldReturnIllegal_WhenDefiniteMapContainsBreakByte)
{
	/* {1: ?} encoded as 0xa1 but with 0xff (BREAK) as the first key */
	uint8_t msg[] = { 0xa1, 0xff };
	cbor_reader_t reader;
	cbor_item_t items[4];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}

TEST(ParserCount,
     ShouldReturnIllegal_WhenDefiniteArrayContainsBreakBeforeLastItem)
{
	uint8_t msg[] = { 0x82, 0xff, 0x01 };
	cbor_reader_t reader;
	cbor_item_t items[8];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}

TEST(ParserCount, ShouldReturnIllegal_WhenTopLevelBreakHasTrailingItem)
{
	uint8_t msg[] = { 0xff, 0x01 };
	cbor_reader_t reader;
	cbor_item_t items[8];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}

/* Regression test: a standalone 0xff (BREAK with no enclosing indefinite
 * container) is malformed CBOR and must not be reported as success. */
TEST(ParserCount, ShouldReturnIllegal_WhenTopLevelBreakIsStandalone)
{
	uint8_t msg[] = { 0xff };
	cbor_reader_t reader;
	cbor_item_t items[8];
	size_t n = 0;

	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_parse(&reader, msg, sizeof(msg), &n));

	n = 0;
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_count_items(msg, sizeof(msg), &n));
}
