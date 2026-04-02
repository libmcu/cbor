/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "CppUTest/TestHarness.h"
#include "cbor/stream.h"
#include <stdint.h>
#include <string.h>

/* ---------- test helpers ---------- */

struct RecordedEvent {
	cbor_stream_event_type_t type;
	uint8_t  depth;
	bool     is_map_key;

	/* scalar and tag payloads (uint_val also stores tag numbers) */
	uint64_t uint_val;
	int64_t  sint_val;
	double   flt_val;
	bool     bool_val;
	uint8_t  simple_val;

	/* string fields */
	uint8_t  str_buf[256];
	size_t   str_len;
	int64_t  str_total;
	bool     str_ptr_is_null;
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
	int           abort_at; /**< abort when count reaches this value */
};

struct ZeroLengthGuard {
	int    text_events;
	size_t last_len;
	bool   saw_zero_len;
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

	if (!data) {
		return true;
	}

	switch (event->type) {
	case CBOR_STREAM_EVENT_TAG:
		ev.uint_val = data->tag; /* reuse uint_val for tag number */
		break;
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
		ev.str_ptr_is_null = (data->str.ptr == NULL);
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

static bool reject_zero_length_text_cb(const cbor_stream_event_t *event,
		const cbor_stream_data_t *data, void *arg)
{
	ZeroLengthGuard *guard = (ZeroLengthGuard *)arg;

	if (event->type != CBOR_STREAM_EVENT_TEXT || data == NULL) {
		return true;
	}

	guard->text_events++;
	guard->last_len = data->str.len;
	guard->saw_zero_len = (data->str.len == 0);

	return !guard->saw_zero_len;
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
	LONGLONGS_EQUAL(5ull, rec.events[0].uint_val);
	LONGS_EQUAL(0, rec.events[0].depth);
}

TEST(StreamInteger, ShouldEmitUint_WhenOneByteLengthGiven)
{
	uint8_t msg[] = { 0x18, 0xff }; /* uint 255 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGLONGS_EQUAL(255ull, rec.events[0].uint_val);
}

TEST(StreamInteger, ShouldEmitUint_WhenTwoByteLengthGiven)
{
	uint8_t msg[] = { 0x19, 0x01, 0x00 }; /* uint 256 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
	LONGLONGS_EQUAL(256ull, rec.events[0].uint_val);
}

TEST(StreamInteger, ShouldEmitNegInt_WhenNegativeIntegerGiven)
{
	uint8_t msg[] = { 0x20 }; /* -1 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_INT, rec.events[0].type);
	LONGLONGS_EQUAL(-1ll, rec.events[0].sint_val);
}

TEST(StreamInteger, ShouldEmitNegInt_WhenNeg100Given)
{
	uint8_t msg[] = { 0x38, 0x63 }; /* -100 */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_INT, rec.events[0].type);
	LONGLONGS_EQUAL(-100ll, rec.events[0].sint_val);
}

TEST(StreamInteger, ShouldEmitMultipleUints_WhenFedByteByByte)
{
	uint8_t msg[] = { 0x01, 0x02, 0x03 };
	feed_byte_by_byte(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(3, rec.count);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);
	LONGLONGS_EQUAL(2ull, rec.events[1].uint_val);
	LONGLONGS_EQUAL(3ull, rec.events[2].uint_val);
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
	CHECK(rec.events[0].str_ptr_is_null);
	CHECK(rec.events[0].str_first);
	CHECK(rec.events[0].str_last);
}

TEST(StreamString, ShouldEmitTextString_WhenShortTextGiven)
{
	/* "a" = 0x61 0x61 */
	uint8_t msg[] = { 0x61, 0x61 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[0].type);
	LONGS_EQUAL(1, rec.events[0].str_len);
	LONGS_EQUAL('a', rec.events[0].str_buf[0]);
	LONGLONGS_EQUAL(1ll, rec.events[0].str_total);
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
	LONGLONGS_EQUAL(-1ll, rec.events[0].str_total);
	CHECK(rec.events[0].str_ptr_is_null);
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
	LONGLONGS_EQUAL(-1ll, rec.events[0].str_total);
	CHECK(rec.events[0].str_first);
	CHECK(!rec.events[0].str_last);

	/* BREAK chunk */
	LONGS_EQUAL(0, rec.events[1].str_len);
	CHECK(rec.events[1].str_ptr_is_null);
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
	CHECK(rec.events[2].str_ptr_is_null);
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
	uint8_t msg[] = { 0xfa, 0x40, 0x48, 0xf5, 0xc3 }; /* 3.14f */
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_FLOAT, rec.events[0].type);
	DOUBLES_EQUAL(3.14, rec.events[0].flt_val, 1e-5);
}

TEST(StreamSimple, ShouldEmitFloat_WhenDoublePrecisionFloatGiven)
{
	/* pi in double precision: 0xfb 0x400921fb54442d18 */
	uint8_t msg[] = { 0xfb, 0x40, 0x09, 0x21, 0xfb, 0x54, 0x44, 0x2d, 0x18 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_FLOAT, rec.events[0].type);
	DOUBLES_EQUAL(3.14159265358979, rec.events[0].flt_val, 1e-10);
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
	LONGLONGS_EQUAL(0ll, rec.events[0].container_size);
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
	LONGLONGS_EQUAL(2ll, rec.events[0].container_size);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[1].type);
	LONGS_EQUAL(1, rec.events[1].depth);
	LONGLONGS_EQUAL(1ull, rec.events[1].uint_val);

	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	LONGLONGS_EQUAL(2ull, rec.events[2].uint_val);

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
	LONGLONGS_EQUAL(-1ll, rec.events[0].container_size);
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
	LONGLONGS_EQUAL(2ll, rec.events[0].container_size);
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
	LONGLONGS_EQUAL(-1ll, rec.events[0].container_size);
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

TEST(StreamTag, ShouldEmitTagEvent_WhenTaggedItemGiven)
{
	/* tag(1)(0) = 0xc1 0x00 */
	uint8_t msg[] = { 0xc1, 0x00 };
	feed_all(&decoder, msg, sizeof(msg));

	/* TAG event, then UINT event */
	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val); /* tag number */
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[1].type);
	LONGLONGS_EQUAL(0ull, rec.events[1].uint_val);
}

TEST(StreamTag, ShouldEmitTagBeforeStart_WhenTaggedArrayGiven)
{
	/* tag(0)([]) = 0xc0 0x80 */
	uint8_t msg[] = { 0xc0, 0x80 };
	feed_all(&decoder, msg, sizeof(msg));

	/* TAG then ARRAY_START then ARRAY_END — no tag on END */
	LONGS_EQUAL(3, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(0ull, rec.events[0].uint_val);

	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[1].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[2].type);
}

TEST(StreamTag, ShouldNotEmitTag_WhenNoTagGiven)
{
	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
}

TEST(StreamTag, ShouldEmitTwoTagEvents_WhenNestedTagsGiven)
{
	/* tag(1)(tag(2)(0)) = 0xc1 0xc2 0x00 */
	uint8_t msg[] = { 0xc1, 0xc2, 0x00 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));

	/* TAG(1), TAG(2), UINT(0) */
	LONGS_EQUAL(3, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[1].type);
	LONGLONGS_EQUAL(2ull, rec.events[1].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[2].type);
	LONGLONGS_EQUAL(0ull, rec.events[2].uint_val);
}

TEST(StreamTag, ShouldEmitTwoTagsBeforeStart_WhenNestedTagsOnContainerGiven)
{
	/* tag(1)(tag(2)([])) = 0xc1 0xc2 0x80 */
	uint8_t msg[] = { 0xc1, 0xc2, 0x80 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));

	/* TAG(1), TAG(2), ARRAY_START, ARRAY_END */
	LONGS_EQUAL(4, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[1].type);
	LONGLONGS_EQUAL(2ull, rec.events[1].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[2].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END, rec.events[3].type);
}

TEST(StreamTag, ShouldEmitTagBeforeFirstChunk_WhenTaggedStringGiven)
{
	/* tag(1)("hi") = 0xc1 0x62 0x68 0x69 */
	uint8_t msg[] = { 0xc1, 0x62, 'h', 'i' };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));

	/* TAG(1), TEXT("hi") */
	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[1].type);
	CHECK(rec.events[1].str_first);
	CHECK(rec.events[1].str_last);
}

TEST(StreamTag, ShouldEmitTagBeforeFirstChunkOnly_WhenTaggedStringFedInChunks)
{
	/* tag(1)("hello") fed in separate chunks */
	uint8_t header[] = { 0xc1, 0x65 }; /* tag(1) + 5-byte text header */
	uint8_t part1[]  = { 'h', 'e', 'l' };
	uint8_t part2[]  = { 'l', 'o' };

	feed_all(&decoder, header, sizeof(header));
	feed_all(&decoder, part1, sizeof(part1));
	feed_all(&decoder, part2, sizeof(part2));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));

	/* TAG(1), then TEXT chunks — tag appears only once */
	CHECK(rec.count >= 2);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);

	/* all remaining events are TEXT */
	for (int i = 1; i < rec.count; i++) {
		LONGS_EQUAL(CBOR_STREAM_EVENT_TEXT, rec.events[i].type);
	}
	CHECK(rec.events[1].str_first);
	CHECK(rec.events[rec.count - 1].str_last);
}

TEST(StreamTag, ShouldEmitTagBeforeEmptyIndefString_WhenTaggedEmptyIndefStringGiven)
{
	/* tag(1)(_ ) = 0xc1 0x5f 0xff */
	uint8_t msg[] = { 0xc1, 0x5f, 0xff };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));

	/* TAG(1), BYTES(first=true, last=true, len=0) */
	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[0].type);
	LONGLONGS_EQUAL(1ull, rec.events[0].uint_val);
	LONGS_EQUAL(CBOR_STREAM_EVENT_BYTES, rec.events[1].type);
	LONGS_EQUAL(0, rec.events[1].str_len);
	CHECK(rec.events[1].str_first);
	CHECK(rec.events[1].str_last);
}

TEST(StreamTag, ShouldReturnExcessive_WhenTagCountExceedsMaxPendingTags)
{
	/* CBOR_STREAM_MAX_PENDING_TAGS+1 consecutive tags then a scalar */
	uint8_t msg[CBOR_STREAM_MAX_PENDING_TAGS + 2];
	/* all tag(0) bytes = 0xc0 */
	for (size_t i = 0; i <= CBOR_STREAM_MAX_PENDING_TAGS; i++) {
		msg[i] = 0xc0;
	}
	msg[CBOR_STREAM_MAX_PENDING_TAGS + 1] = 0x00; /* uint 0 */

	LONGS_EQUAL(CBOR_EXCESSIVE,
		cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamTag, ShouldReturnAborted_WhenCallbackReturnsFalseOnTagEvent)
{
	/* Abort on the TAG event itself */
	rec.abort_after_first = true;
	rec.abort_at = 0; /* zero-based: abort on first event (the TAG) */

	uint8_t msg[] = { 0xc1, 0x00 };
	LONGS_EQUAL(CBOR_ABORTED, cbor_stream_feed(&decoder, msg, sizeof(msg)));

	/* sticky: finish() returns same error */
	LONGS_EQUAL(CBOR_ABORTED, cbor_stream_finish(&decoder));
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
	/* TAG(0) was emitted before the error */
	LONGS_EQUAL(2, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[0].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_TAG, rec.events[1].type);
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
	LONGS_EQUAL(0, rec.count);
}

TEST(StreamError, ShouldReturnAborted_WhenCallbackReturnsFalse)
{
	rec.abort_after_first = true;
	rec.abort_at = 1;

	uint8_t msg[] = { 0x01, 0x02 };
	LONGS_EQUAL(CBOR_ABORTED, cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldReturnInvalid_WhenFeedCalledWithNullCallback)
{
	cbor_stream_init(&decoder, NULL, NULL);

	uint8_t msg[] = { 0x01 };
	/* feed() rejects NULL callback */
	LONGS_EQUAL(CBOR_INVALID, cbor_stream_feed(&decoder, msg, sizeof(msg)));
	/* finish() only inspects state — decoder stayed IDLE, so returns SUCCESS */
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_finish(&decoder));
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

TEST(StreamError, ShouldReturnSuccess_WhenFinishCalledAfterMultipleTopLevelItems)
{
	uint8_t msg[] = { 0x01, 0x02, 0x03 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(3, rec.count);
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

TEST(StreamError, ShouldReturnExcessive_WhenNestingExceedsMaxLevel)
{
	/* open CBOR_RECURSION_MAX_LEVEL+1 indefinite arrays */
	uint8_t msg[CBOR_RECURSION_MAX_LEVEL + 1];
	for (size_t i = 0; i <= CBOR_RECURSION_MAX_LEVEL; i++) {
		msg[i] = 0x9f; /* indefinite array */
	}
	LONGS_EQUAL(CBOR_EXCESSIVE,
		cbor_stream_feed(&decoder, msg, sizeof(msg)));
}

TEST(StreamError, ShouldSucceed_WhenNestingAtMaxLevel)
{
	/* open exactly CBOR_RECURSION_MAX_LEVEL indefinite arrays, then close all */
	uint8_t opens[CBOR_RECURSION_MAX_LEVEL];
	uint8_t closes[CBOR_RECURSION_MAX_LEVEL];
	for (size_t i = 0; i < CBOR_RECURSION_MAX_LEVEL; i++) {
		opens[i]  = 0x9f; /* indefinite array */
		closes[i] = 0xff; /* BREAK */
	}

	feed_all(&decoder, opens, sizeof(opens));
	feed_all(&decoder, closes, sizeof(closes));

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
	LONGLONGS_EQUAL(5ull, rec.events[0].uint_val);
}

TEST(StreamReset, ShouldPreserveCallback_WhenResetCalled)
{
	cbor_stream_reset(&decoder);

	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
}

TEST(StreamReset, ShouldClearPendingTags_WhenResetCalledAfterBareTag)
{
	/* feed a tag with no following item */
	uint8_t bare_tag[] = { 0xc1 };
	LONGS_EQUAL(CBOR_SUCCESS,
		cbor_stream_feed(&decoder, bare_tag, sizeof(bare_tag)));

	cbor_stream_reset(&decoder);
	rec.count = 0;

	/* after reset, normal decoding works */
	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
}

TEST(StreamReset, ShouldClearIndefStrState_WhenResetCalledMidIndefString)
{
	/* start an indefinite byte string, then reset without finishing */
	uint8_t indef_start[] = { 0x5f };
	LONGS_EQUAL(CBOR_SUCCESS,
		cbor_stream_feed(&decoder, indef_start, sizeof(indef_start)));

	cbor_stream_reset(&decoder);
	rec.count = 0;

	/* after reset, normal decoding works */
	uint8_t msg[] = { 0x01 };
	feed_all(&decoder, msg, sizeof(msg));

	LONGS_EQUAL(1, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT, rec.events[0].type);
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

	LONGS_EQUAL(7, rec.count);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_START,   rec.events[0].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[1].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_START, rec.events[2].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[3].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_UINT,        rec.events[4].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_ARRAY_END,   rec.events[5].type);
	LONGS_EQUAL(CBOR_STREAM_EVENT_MAP_END,     rec.events[6].type);

	LONGS_EQUAL(1, rec.events[2].depth);
	LONGS_EQUAL(2, rec.events[3].depth);
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
	 * Events:
	 *  [0] MAP_START      is_map_key=false
	 *  [1] ARRAY_START    is_map_key=true  (array is the key)
	 *  [2] UINT(1)        is_map_key=false (inside array)
	 *  [3] ARRAY_END      is_map_key=true  (array was the key)
	 *  [4] TEXT "v"       is_map_key=false (value)
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
	 * Events:
	 *  [0] MAP_START (outer)   is_map_key=false
	 *  [1] MAP_START (inner)   is_map_key=true
	 *  [2] UINT(1)             is_map_key=true
	 *  [3] UINT(2)             is_map_key=false
	 *  [4] MAP_END (inner)     is_map_key=true
	 *  [5] TEXT "v"            is_map_key=false
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

/* ---------- TEST_GROUP: Large payloads ---------- */

TEST_GROUP(StreamLargePayload)
{
	cbor_stream_decoder_t decoder;
	ZeroLengthGuard       guard;

	void setup()
	{
		memset(&guard, 0, sizeof(guard));
		cbor_stream_init(&decoder, reject_zero_length_text_cb, &guard);
	}
};

TEST(StreamLargePayload, ShouldMakeForwardProgress_WhenTextLengthExceeds32BitRange)
{
	uint8_t header[] = {
		0x7b,
		0x00, 0x00, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00,
	};
	uint8_t payload1[] = { 'A' };
	uint8_t payload2[] = { 'B' };

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, header, sizeof(header)));
	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, payload1, sizeof(payload1)));

	LONGS_EQUAL(1, guard.text_events);
	CHECK(!guard.saw_zero_len);
	LONGS_EQUAL(1, guard.last_len);
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));

	LONGS_EQUAL(CBOR_SUCCESS, cbor_stream_feed(&decoder, payload2, sizeof(payload2)));
	LONGS_EQUAL(2, guard.text_events);
	CHECK(!guard.saw_zero_len);
	LONGS_EQUAL(1, guard.last_len);
	LONGS_EQUAL(CBOR_NEED_MORE, cbor_stream_finish(&decoder));
}
