/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "cbor/stream.h"
#include <stdint.h>
#include <string.h>

/* ---------- helpers ---------- */

struct RecordedEvent {
	cbor_stream_event_type_t type;
	uint8_t  depth;
	bool     is_map_key;
	bool     has_tag;
	uint64_t tag;

	/* scalar payloads */
	uint64_t uint_val;
	int64_t  sint_val;
	double   flt_val;
	bool     bool_val;
	uint8_t  simple_val;

	/* string fields */
	uint8_t  str_buf[256];
	size_t   str_len;
	int64_t  str_total;
	bool     str_first;
	bool     str_last;

	/* container size */
	int64_t container_size;
};

static const int MAX_EVENTS = 64;

struct Recorder {
	RecordedEvent events[MAX_EVENTS];
	int           count;
	bool          abort_after_first;
	int           abort_at;   /* abort when count reaches this value */
};

static bool record_cb(const cbor_stream_event_t *event,
		const cbor_stream_data_t *data, void *arg)
{
	Recorder *r = (Recorder *)arg;

	if (r->abort_after_first && r->count >= r->abort_at) {
		return false;
	}

	if (r->count >= MAX_EVENTS) {
		return true;
	}

	RecordedEvent &ev = r->events[r->count++];
	memset(&ev, 0, sizeof(ev));
	ev.type       = event->type;
	ev.depth      = event->depth;
	ev.is_map_key = event->is_map_key;
	ev.has_tag    = event->has_tag;
	ev.tag        = event->tag;

	if (!data) {
		return true;
	}

	switch (event->type) {
	case CBOR_STREAM_EVENT_UINT:
		ev.uint_val = data->uint;
		break;
	case CBOR_STREAM_EVENT_INT:
		ev.sint_val = data->sint;
		break;
	case CBOR_STREAM_EVENT_FLOAT:
		ev.flt_val = data->flt;
		break;
	case CBOR_STREAM_EVENT_BOOL:
		ev.bool_val = data->boolean;
		break;
	case CBOR_STREAM_EVENT_SIMPLE:
		ev.simple_val = data->simple;
		break;
	case CBOR_STREAM_EVENT_BYTES:
	case CBOR_STREAM_EVENT_TEXT:
		ev.str_total = data->str.total;
		ev.str_first = data->str.first;
		ev.str_last  = data->str.last;
		ev.str_len   = data->str.len;
		if (data->str.ptr && data->str.len <= sizeof(ev.str_buf)) {
			memcpy(ev.str_buf, data->str.ptr, data->str.len);
		}
		break;
	case CBOR_STREAM_EVENT_ARRAY_START:
	case CBOR_STREAM_EVENT_MAP_START:
		ev.container_size = data->container.size;
		break;
	case CBOR_STREAM_EVENT_ARRAY_END:
	case CBOR_STREAM_EVENT_MAP_END:
	case CBOR_STREAM_EVENT_NULL:
	case CBOR_STREAM_EVENT_UNDEFINED:
	default:
		break;
	}

	return true;
}

static void feed_all(cbor_stream_decoder_t *d, const uint8_t *buf, size_t len)
{
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(d, buf, len));
}

/* Feed one byte at a time to stress the streaming logic */
static void feed_byte_by_byte(cbor_stream_decoder_t *d,
		const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(d, &buf[i], 1));
	}
}

/* ---------- TEST_GROUP: Integers ---------- */

TEST_GROUP(StreamInteger)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamInteger, ShouldEmitUint_WhenSmallUintGiven)
{
	uint8_t msg[] = { 0x05 }; /* uint 5 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGS_EQUAL(5, rec.events[0].uint_val);
	LONGS_EQUAL(0, rec.events[0].depth);
}

TEST(StreamInteger, ShouldEmitUint_WhenOneByteLengthGiven)
{
	uint8_t msg[] = { 0x18, 0xff }; /* uint 255 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGS_EQUAL(255, rec.events[0].uint_val);
}

TEST(StreamInteger, ShouldEmitUint_WhenTwoByteLengthGiven)
{
	uint8_t msg[] = { 0x19, 0x01, 0x00 }; /* uint 256 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGS_EQUAL(256, rec.events[0].uint_val);
}

TEST(StreamInteger, ShouldEmitNegInt_WhenNegativeIntegerGiven)
{
	uint8_t msg[] = { 0x20 }; /* -1 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_INT, rec.events[0].type);
	LONGS_EQUAL(-1, rec.events[0].sint_val);
}

TEST(StreamInteger, ShouldEmitNegInt_WhenNeg100Given)
{
	uint8_t msg[] = { 0x38, 0x63 }; /* -100 = 0x38, 0x63 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_INT, rec.events[0].type);
	LONGS_EQUAL(-100, rec.events[0].sint_val);
}

TEST(StreamInteger, ShouldEmitMultipleUints_WhenFedByteByByte)
{
	uint8_t msg[] = { 0x01, 0x02, 0x03 };
	feed_byte_by_byte(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(3, rec.count);
	LONGS_EQUAL(1, rec.events[0].uint_val);
	LONGS_EQUAL(2, rec.events[1].uint_val);
	LONGS_EQUAL(3, rec.events[2].uint_val);
}

/* ---------- TEST_GROUP: Strings ---------- */

TEST_GROUP(StreamString)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamString, ShouldEmitEmptyTextString_WhenZeroLengthTextGiven)
{
	uint8_t msg[] = { 0x60 }; /* "" */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].str_len);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[0].str_last);
}

TEST(StreamString, ShouldEmitTextString_WhenShortTextGiven)
{
	/* "a" = 0x61, 0x61 */
	uint8_t msg[] = { 0x61, 0x61 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[0].type);
	LONGS_EQUAL(1, rec.events[0].str_len);
	LONGS_EQUAL('a', rec.events[0].str_buf[0]);
	LONGS_EQUAL(1, rec.events[0].str_total);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[0].str_last);
}

TEST(StreamString, ShouldEmitTextString_WhenFedByteByByte)
{
	/* "hello" */
	uint8_t msg[] = { 0x65, 'h', 'e', 'l', 'l', 'o' };
	feed_byte_by_byte(&decoder, msg, sizeof(msg));

	/* may arrive in multiple chunks; total content matters */
	size_t total_len = 0;
	for (int i = 0; i < rec.count; i++) {
		total_len += rec.events[i].str_len;
	}
	LONGS_EQUAL(5, total_len);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[rec.count - 1].str_last);
}

TEST(StreamString, ShouldEmitByteString_WhenByteStringGiven)
{
	uint8_t msg[] = { 0x43, 0xAA, 0xBB, 0xCC }; /* h'aabbcc' */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_BYTES, rec.events[0].type);
	LONGS_EQUAL(3, rec.events[0].str_len);
	LONGS_EQUAL(0xAA, rec.events[0].str_buf[0]);
}

TEST(StreamString, ShouldEmitChunks_WhenPayloadSplitAcrossFeeds)
{
	/* 5-byte text "hello" split into 3+2 */
	uint8_t header[] = { 0x65 };
	uint8_t part1[]  = { 'h', 'e', 'l' };
	uint8_t part2[]  = { 'l', 'o' };

	feed_all(&decoder, header, sizeof(header));
	feed_all(&decoder, part1, sizeof(part1));
	feed_all(&decoder, part2, sizeof(part2));

	/* At least first chunk has first=true, last chunk has last=true */
	CHECK(rec.count >= 1);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[rec.count - 1].str_last);
}

/* ---------- TEST_GROUP: Indefinite Strings ---------- */

TEST_GROUP(StreamIndefString)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamIndefString, ShouldEmitSingleBreakChunk_WhenEmptyIndefStringGiven)
{
	/* (_ ) = 0x5f 0xff */
	uint8_t msg[] = { 0x5f, 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_BYTES, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].str_len);
	LONGS_EQUAL(-1, rec.events[0].str_total);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[0].str_last);
}

TEST(StreamIndefString, ShouldEmitSubChunkThenBreak_WhenIndefStringWithDataGiven)
{
	/* (_ h'0102') = 0x5f 0x42 0x01 0x02 0xff */
	uint8_t msg[] = { 0x5f, 0x42, 0x01, 0x02, 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(2, rec.count);

	/* first sub-chunk */
	LONGS_EQUAL(CBOR_STREAM_EVENT_BYTES, rec.events[0].type);
	LONGS_EQUAL(2, rec.events[0].str_len);
	LONGS_EQUAL(0x01, rec.events[0].str_buf[0]);
	LONGS_EQUAL(0x02, rec.events[0].str_buf[1]);
	LONGS_EQUAL(-1, rec.events[0].str_total);
	CHECK(rec.events[0].str_first);
	CHECK(!rec.events[0].str_last);

	/* BREAK chunk */
	LONGS_EQUAL(0, rec.events[1].str_len);
	CHECK(!rec.events[1].str_first);
	CHECK(rec.events[1].str_last);
}

TEST(StreamIndefString, ShouldEmitTextChunks_WhenIndefTextStringGiven)
{
	/* (_ "fo" "o") = 0x7f 0x62 0x66 0x6f 0x61 0x6f 0xff */
	uint8_t msg[] = { 0x7f, 0x62, 'f', 'o', 0x61, 'o', 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(3, rec.count);
	for (int i = 0; i < 3; i++) {
		LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[i].type);
	}
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[2].str_last);
	LONGS_EQUAL(0, rec.events[2].str_len); /* BREAK chunk */
}

TEST(StreamIndefString, ShouldReturnIllegal_WhenWrongMajorTypeInIndefString)
{
	/* 0x5f followed by a text string chunk instead of byte string */
	uint8_t msg[] = { 0x5f, 0x61, 0x61 };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

/* ---------- TEST_GROUP: Simple / Float / Bool ---------- */

TEST_GROUP(StreamSimple)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamSimple, ShouldEmitTrue_WhenTrueGiven)
{
	uint8_t msg[] = { 0xf5 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_BOOL, rec.events[0].type);
	CHECK(rec.events[0].bool_val);
}

TEST(StreamSimple, ShouldEmitFalse_WhenFalseGiven)
{
	uint8_t msg[] = { 0xf4 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_BOOL, rec.events[0].type);
	CHECK(!rec.events[0].bool_val);
}

TEST(StreamSimple, ShouldEmitNull_WhenNullGiven)
{
	uint8_t msg[] = { 0xf6 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_NULL, rec.events[0].type);
}

TEST(StreamSimple, ShouldEmitUndefined_WhenUndefinedGiven)
{
	uint8_t msg[] = { 0xf7 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UNDEFINED, rec.events[0].type);
}

TEST(StreamSimple, ShouldEmitSimple_WhenUnassignedSimpleValueGiven)
{
	uint8_t msg[] = { 0xe0 }; /* simple(0) */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_SIMPLE, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].simple_val);
}

TEST(StreamSimple, ShouldEmitFloat_WhenHalfPrecisionFloatGiven)
{
	/* 1.0 in half precision = 0xf9 0x3c 0x00 */
	uint8_t msg[] = { 0xf9, 0x3c, 0x00 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_FLOAT, rec.events[0].type);
	DOUBLES_EQUAL(1.0, rec.events[0].flt_val, 1e-6);
}

TEST(StreamSimple, ShouldEmitFloat_WhenSinglePrecisionFloatGiven)
{
	uint8_t msg[] = { 0xfa, 0x40, 0x48, 0xf5, 0xc3 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_FLOAT, rec.events[0].type);
	DOUBLES_EQUAL(3.14, rec.events[0].flt_val, 1e-5);
}

/* ---------- TEST_GROUP: Arrays ---------- */

TEST_GROUP(StreamArray)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamArray, ShouldEmitStartEnd_WhenEmptyArrayGiven)
{
	uint8_t msg[] = { 0x80 }; /* [] */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].container_size);
	LONGS_EQUAL(0, rec.events[0].depth);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[1].type);
	LONGS_EQUAL(0, rec.events[1].depth);
}

TEST(StreamArray, ShouldEmitItemsWithCorrectDepth_WhenDefiniteArrayGiven)
{
	uint8_t msg[] = { 0x82, 0x01, 0x02 }; /* [1, 2] */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].depth);
	LONGS_EQUAL(2, rec.events[0].container_size);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[1].type);
	LONGS_EQUAL(1, rec.events[1].depth);
	LONGS_EQUAL(1, rec.events[1].uint_val);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	LONGS_EQUAL(2, rec.events[2].uint_val);

	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[3].type);
	LONGS_EQUAL(0, rec.events[3].depth);
}

TEST(StreamArray, ShouldEmitNestedArrayEvents_WhenNestedArrayGiven)
{
	/* [[1]] */
	uint8_t msg[] = { 0x81, 0x81, 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(5, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	LONGS_EQUAL(0, rec.events[0].depth);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[1].type);
	LONGS_EQUAL(1, rec.events[1].depth);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	LONGS_EQUAL(2, rec.events[2].depth);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[3].type);
	LONGS_EQUAL(1, rec.events[3].depth);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[4].type);
	LONGS_EQUAL(0, rec.events[4].depth);
}

TEST(StreamArray, ShouldEmitIndefiniteArray_WhenIndefArrayGiven)
{
	/* [_ 1, 2] = 0x9f 0x01 0x02 0xff */
	uint8_t msg[] = { 0x9f, 0x01, 0x02, 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	LONGS_EQUAL(-1, rec.events[0].container_size);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[1].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[3].type);
}

TEST(StreamArray, ShouldEmitEvents_WhenArrayFedByteByByte)
{
	uint8_t msg[] = { 0x82, 0x01, 0x02 };
	feed_byte_by_byte(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[3].type);
}

/* ---------- TEST_GROUP: Maps ---------- */

TEST_GROUP(StreamMap)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamMap, ShouldEmitStartEnd_WhenEmptyMapGiven)
{
	uint8_t msg[] = { 0xa0 }; /* {} */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[0].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END, rec.events[1].type);
}

TEST(StreamMap, ShouldSetIsMapKey_WhenInsideMap)
{
	/* {1: 2} = 0xa1 0x01 0x02 */
	uint8_t msg[] = { 0xa1, 0x01, 0x02 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[0].type);
	CHECK(!rec.events[0].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[1].type);
	CHECK(rec.events[1].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	CHECK(!rec.events[2].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END, rec.events[3].type);
}

TEST(StreamMap, ShouldReportPairCount_WhenDefiniteMapGiven)
{
	uint8_t msg[] = { 0xa2, 0x01, 0x02, 0x03, 0x04 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[0].type);
	LONGS_EQUAL(2, rec.events[0].container_size);
}

TEST(StreamMap, ShouldEmitMultiplePairs_WhenMultiPairMapGiven)
{
	/* {1:2, 3:4} = 0xa2 0x01 0x02 0x03 0x04 */
	uint8_t msg[] = { 0xa2, 0x01, 0x02, 0x03, 0x04 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(6, rec.count);
	CHECK(rec.events[1].is_map_key);  /* key 1 */
	CHECK(!rec.events[2].is_map_key); /* val 2 */
	CHECK(rec.events[3].is_map_key);  /* key 3 */
	CHECK(!rec.events[4].is_map_key); /* val 4 */
}

TEST(StreamMap, ShouldEmitIndefiniteMap_WhenIndefMapGiven)
{
	/* {_ 1:2} = 0xbf 0x01 0x02 0xff */
	uint8_t msg[] = { 0xbf, 0x01, 0x02, 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[0].type);
	LONGS_EQUAL(-1, rec.events[0].container_size);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END, rec.events[3].type);
}

TEST(StreamMap, ShouldReturnIllegal_WhenBreakFollowsOnlyMapKey)
{
	uint8_t msg[] = { 0xbf, 0x01, 0xff };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

/* ---------- TEST_GROUP: Tags ---------- */

TEST_GROUP(StreamTag)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamTag, ShouldSetHasTag_WhenTaggedItemGiven)
{
	/* tag(1)(0) = 0xc1 0x00 */
	uint8_t msg[] = { 0xc1, 0x00 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	CHECK(rec.events[0].has_tag);
	LONGS_EQUAL(1, rec.events[0].tag);
	LONGS_EQUAL(0, rec.events[0].uint_val);
}

TEST(StreamTag, ShouldSetHasTagOnStart_WhenTaggedArrayGiven)
{
	/* tag(0)([]) = 0xc0 0x80 */
	uint8_t msg[] = { 0xc0, 0x80 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	CHECK(rec.events[0].has_tag);
	LONGS_EQUAL(0, rec.events[0].tag);

	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[1].type);
	CHECK(!rec.events[1].has_tag);
}

TEST(StreamTag, ShouldNotSetHasTag_WhenNoTagGiven)
{
	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	CHECK(!rec.events[0].has_tag);
}

TEST(StreamTag, ShouldReturnExcessive_WhenNestedTagsGiven)
{
	/* tag(1)(tag(2)(0)) = 0xc1 0xc2 0x00 */
	uint8_t msg[] = { 0xc1, 0xc2, 0x00 };
	LONGS_EQUAL(CBOR_EXCESSIVE, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

/* ---------- TEST_GROUP: Error Handling ---------- */

TEST_GROUP(StreamError)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamError, ShouldReturnIllegal_WhenStrayBreakGiven)
{
	uint8_t msg[] = { 0xff };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnIllegal_WhenBreakInsideDefiniteArray)
{
	/* [_ (break)] pretending to be definite: 0x81 0xff */
	uint8_t msg[] = { 0x81, 0xff };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnIllegal_WhenBreakFollowsPendingTagInIndefiniteArray)
{
	uint8_t msg[] = { 0x9f, 0xc0, 0xff };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
}

TEST(StreamError, ShouldReturnIllegal_WhenReservedAdditionalInfoGiven)
{
	uint8_t msg[] = { 0x1c }; /* additional_info=28, reserved */
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldStickyError_WhenSubsequentFeedAfterError)
{
	uint8_t bad[] = { 0xff };
	uint8_t good[] = { 0x01 };

	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, bad, sizeof(bad)));
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, good, sizeof(good)));
	LONGS_EQUAL(0, rec.count); /* no extra events */
}

TEST(StreamError, ShouldReturnAborted_WhenCallbackReturnsFalse)
{
	rec.abort_after_first = true;
	rec.abort_at = 1; /* abort after first event */

	uint8_t msg[] = { 0x01, 0x02 };
	LONGS_EQUAL(CBOR_ABORTED, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnInvalid_WhenCallbackIsNull)
{
	cbor_stream_init(&decoder, NULL, NULL);

	uint8_t msg[] = { 0x01 };
	LONGS_EQUAL(CBOR_INVALID, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(CBOR_INVALID, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnInvalid_WhenNegativeIntegerExceedsInt64Range)
{
	uint8_t msg[] = {
		0x3b, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};

	LONGS_EQUAL(CBOR_INVALID, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnInvalid_WhenStringLengthExceedsInt64Range)
{
	uint8_t msg[] = {
		0x7b, 0x80, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};

	LONGS_EQUAL(CBOR_INVALID, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnInvalid_WhenMapLengthOverflowsItemCount)
{
	uint8_t msg[] = {
		0xbb, 0x40, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
	};

	LONGS_EQUAL(CBOR_INVALID, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnNeedMore_WhenFinishCalledMidItem)
{
	uint8_t msg[] = { 0x18 }; /* start of uint with 1-byte length, truncated */
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnNeedMore_WhenFinishCalledMidPayload)
{
	/* 3-byte text string, only header given */
	uint8_t msg[] = { 0x63 };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnNeedMore_WhenFinishCalledInsideOpenArray)
{
	uint8_t msg[] = { 0x9f, 0x01 }; /* [_ 1, ... */
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnNeedMore_WhenFinishCalledAfterBareTag)
{
	uint8_t msg[] = { 0xc0 };
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnSuccess_WhenFinishCalledAfterCompleteLengthEncodedUint)
{
	uint8_t msg[] = { 0x18, 0x01 };
	feed_all(&decoder, msg, sizeof(msg));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldReturnSuccess_WhenFinishCalledAfterCompleteItem)
{
	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
}

TEST(StreamError, ShouldTreatZeroLengthFeedAsNoOp)
{
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, NULL, 0));

	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
}

/* ---------- TEST_GROUP: Reset ---------- */

TEST_GROUP(StreamReset)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamReset, ShouldDecodeAfterReset_WhenPreviouslyErrored)
{
	uint8_t bad[] = { 0xff };
	LONGS_EQUAL(CBOR_ILLEGAL, cbor_stream_feed(&decoder, bad, sizeof(bad)));

	cbor_stream_reset(&decoder);
	rec.count = 0;

	uint8_t good[] = { 0x05 };
	feed_all(&decoder, good, sizeof(good));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGS_EQUAL(5, rec.events[0].uint_val);
}

TEST(StreamReset, ShouldPreserveCallback_WhenResetCalled)
{
	cbor_stream_reset(&decoder);

	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
}

/* ---------- TEST_GROUP: Chunked feeds ---------- */

TEST_GROUP(StreamChunked)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamChunked, ShouldDecodeCorrectly_WhenMapFedInChunks)
{
	/* {1: "hi"} = 0xa1 0x01 0x62 0x68 0x69 */
	uint8_t msg[] = { 0xa1, 0x01, 0x62, 0x68, 0x69 };

	/* feed 2 bytes, then 3 bytes */
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg, 2));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, msg + 2, 3));

	/* MAP_START, key=1 (uint), value="hi" (text), MAP_END */
	int evt = 0;
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[evt++].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[evt].type);
	CHECK(rec.events[evt++].is_map_key);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[evt].type);
	CHECK(!rec.events[evt++].is_map_key);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END, rec.events[evt++].type);
	LONGS_EQUAL(evt, rec.count);
}

TEST(StreamChunked, ShouldDecodeNestedMap_WhenComplexMessageGiven)
{
	/* {1: [2, 3]} = 0xa1 0x01 0x82 0x02 0x03 */
	uint8_t msg[] = { 0xa1, 0x01, 0x82, 0x02, 0x03 };
	feed_byte_by_byte(&decoder, msg, sizeof(msg));

	/* MAP_START, key(uint 1), ARRAY_START, uint 2, uint 3, ARRAY_END, MAP_END */
	LONGS_EQUAL(7, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START,   rec.events[0].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[1].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[2].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[3].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[4].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END,   rec.events[5].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END,     rec.events[6].type);

	LONGS_EQUAL(1, rec.events[2].depth); /* array-start inside map value at depth 1 */
	LONGS_EQUAL(2, rec.events[3].depth); /* items inside array at depth 2 */
}

/* ---------- TEST_GROUP: is_map_key on END events ---------- */

TEST_GROUP(StreamMapKeyEnd)
{
	cbor_stream_decoder_t decoder;
	Recorder              rec;

	void setup()
	{
		memset(&rec, 0, sizeof(rec));
		cbor_stream_init(&decoder, record_cb, &rec);
	}
};

TEST(StreamMapKeyEnd, ShouldReportArrayEndAsMapKey_WhenArrayIsMapKey)
{
	/*
	 * {[1]: "v"}
	 * 0xa1         map(1 pair)
	 * 0x81 0x01    array(1): uint 1        <- map key
	 * 0x61 0x76    text "v"                <- map value
	 *
	 * Expected events and is_map_key flags:
	 *  [0] MAP_START      is_map_key=false (top-level)
	 *  [1] ARRAY_START    is_map_key=true  (array is the key)
	 *  [2] UINT(1)        is_map_key=false (inside array, not in a map)
	 *  [3] ARRAY_END      is_map_key=true  (array was the key, its end should reflect that)
	 *  [4] TEXT "v"       is_map_key=false (this is the value)
	 *  [5] MAP_END        is_map_key=false
	 */
	uint8_t msg[] = { 0xa1, 0x81, 0x01, 0x61, 0x76 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
	LONGS_EQUAL(6, rec.count);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START,   rec.events[0].type);
	CHECK(!rec.events[0].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[1].type);
	CHECK(rec.events[1].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[2].type);
	CHECK(!rec.events[2].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END,   rec.events[3].type);
	CHECK(rec.events[3].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT,        rec.events[4].type);
	CHECK(!rec.events[4].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END,     rec.events[5].type);
}

TEST(StreamMapKeyEnd, ShouldReportNestedMapEndAsMapKey_WhenNestedMapIsMapKey)
{
	/*
	 * {{1:2}: "v"}
	 * 0xa1               outer map(1 pair)
	 * 0xa1 0x01 0x02     inner map{1:2}   <- outer map key
	 * 0x61 0x76          text "v"         <- outer map value
	 *
	 * Expected events:
	 *  [0] MAP_START (outer)   is_map_key=false
	 *  [1] MAP_START (inner)   is_map_key=true  (inner map is the key)
	 *  [2] UINT(1)             is_map_key=true  (key of inner map)
	 *  [3] UINT(2)             is_map_key=false (value of inner map)
	 *  [4] MAP_END (inner)     is_map_key=true  (inner map was the key)
	 *  [5] TEXT "v"            is_map_key=false (outer map value)
	 *  [6] MAP_END (outer)     is_map_key=false
	 */
	uint8_t msg[] = { 0xa1, 0xa1, 0x01, 0x02, 0x61, 0x76 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
	LONGS_EQUAL(7, rec.count);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[0].type);
	CHECK(!rec.events[0].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START, rec.events[1].type);
	CHECK(rec.events[1].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,      rec.events[2].type);
	CHECK(rec.events[2].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,      rec.events[3].type);
	CHECK(!rec.events[3].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END,   rec.events[4].type);
	CHECK(rec.events[4].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT,      rec.events[5].type);
	CHECK(!rec.events[5].is_map_key);

	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END,   rec.events[6].type);
}
