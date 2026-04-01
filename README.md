# CBOR
This is a minimalistic implementation for CBOR, the Concise Binary Object
Representation. CBOR is defined by
[IETF RFC 8949](https://datatracker.ietf.org/doc/html/rfc8949), and
Wikipedia has [a good description](https://en.wikipedia.org/wiki/CBOR).

## Features

* C99
* No dynamic memory allocation
* Small code footprint

## Build
### Make

```make
CBOR_ROOT ?= <THIRD_PARTY_DIR>/cbor
include $(CBOR_ROOT)/cbor.mk
```

### CMake

```cmake
include(FetchContent)
FetchContent_Declare(cbor
                      GIT_REPOSITORY https://github.com/libmcu/cbor.git
                      GIT_TAG main
)
FetchContent_MakeAvailable(cbor)
```

or

```cmake
set(CBOR_ROOT <THIRD_PARTY_DIR>/cbor)
include(${CBOR_ROOT}/cbor.cmake)
```

## Usage

```c
static void parse_cert(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg) {
	struct your_data_type *out = arg;
	cbor_decode(reader, item, out->cert, sizeof(out->cert));
}
static void parse_key(const cbor_reader_t *reader,
		const struct cbor_parser *parser,
		const cbor_item_t *item, void *arg) {
	struct your_data_type *out = arg;
	cbor_decode(reader, item, out->key, sizeof(out->key));
}

static const struct cbor_parser parsers[] = {
	{ .key = "certificate", .run = parse_cert },
	{ .key = "privateKey",  .run = parse_key },
};

cbor_reader_t reader;
cbor_item_t items[MAX_ITEMS];

cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
cbor_unmarshal(&reader, parsers, sizeof(parsers) / sizeof(*parsers),
		msg, msglen, &your_data_type);

...
```

Please refer to [examples](examples).

### Option

* `CBOR_BIG_ENDIAN`
  - Define the macro for big endian machine. The default is little endian.
* `CBOR_RECURSION_MAX_LEVEL`
  - This is set to avoid stack overflow from recursion. The default maximum
    depth is 8.

### Parser

The parser takes 626 bytes on ARM Cortex-M0 optimizing for code size `-Os`.
[arm-none-eabi-gcc
10-2020-q4-major](https://developer.arm.com/-/media/Files/downloads/gnu-rm/10-2020q4/gcc-arm-none-eabi-10-2020-q4-major-src.tar.bz2?revision=8f69a18b-dbe3-45ec-b896-3ba56844938d&hash=946C702B1C99A84CD0C441357D578E80B2A56EF9)
was used for the check.

Stack usage per the major type functions:

| Major type                                         | Bytes |
| -------------------------------------------------- | ----- |
| 0: unsigned integer                                | 12    |
| 1: negative integer                                | 12    |
| 2: byte string                                     | 32    |
| 3: text string                                     | 32    |
| 4: array                                           | 32    |
| 5: map                                             | 32    |
| 6: tag(not implemented yet)                        | 0     |
| 7: floating-point numbers, simple values and break | 32    |

And the call stack for each recursion is 24 bytes.

```c
cbor_reader_t reader;
cbor_item_t items[MAX_ITEMS];
size_t n;

cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
cbor_parse(&reader, cbor_message, cbor_message_len, &n);

for (size_t i = 0; i < n; i++) {
	printf("item: %s, size: %zu\n",
			cbor_stringify_item(&items[i]),
			cbor_get_item_size(&items[i]));
}
```

`MAX_ITEMS` is a capacity in **number of parsed items**, not bytes. The memory
footprint of this array is:

```c
size_t item_buffer_bytes = MAX_ITEMS * sizeof(cbor_item_t);
```

`sizeof(cbor_item_t)` is platform dependent (e.g. typically 12 bytes on many
32-bit MCUs, and often 24 bytes on 64-bit hosts).

Each `cbor_item_t` represents one parsed CBOR node (container and leaf both
count as one item). `cbor_get_item_size()` unit depends on the item type:

* Integer / float / simple value: encoded payload size in bytes
* Byte/text string (definite-length): string length in bytes
* Array (definite-length): number of child items
* Map (definite-length): number of key-value pairs

For **indefinite-length** strings, arrays, and maps, `cbor_get_item_size()`
returns the raw CBOR length field, which is the sentinel
`(size_t)CBOR_INDEFINITE_VALUE`. In that case, the number of child items / key-
value pairs is not known up front; iterate until the CBOR BREAK marker is
encountered instead of using `cbor_get_item_size()` as a count.

Example (`0xA2 0x61 0x61 0x01 0x61 0x62 0x02`, equivalent to
`{"a": 1, "b": 2}`):

* 1 map item
* 2 key items (`"a"`, `"b"`)
* 2 value items (`1`, `2`)
* total parsed item count (`n`) = 5

### Sizing `MAX_ITEMS` (pre-estimation)

Use `cbor_count_items()` for dry-run counting without allocating `cbor_item_t`
buffer:

```c
size_t needed_items = 0;
cbor_error_t err = cbor_count_items(cbor_message, cbor_message_len,
		&needed_items);
if (err == CBOR_SUCCESS) {
	/* allocate items[needed_items + margin] */
}
```

In practice, use one of the following:

1. Reserve a conservative static capacity based on your schema, or
2. Call `cbor_count_items()` first, then set production `MAX_ITEMS` with margin.

### Error when item capacity is not enough

If `items[]` is too small to hold all parsed nodes, `cbor_parse()` returns
`CBOR_OVERRUN` (stringified as `"out of memory"`).

```c
cbor_error_t err = cbor_parse(&reader, msg, msglen, &n);
if (err == CBOR_OVERRUN) {
	/* items[] capacity is insufficient: increase maxitems */
}
```

In this case, `n` contains only the number of items actually stored before the
overrun.

### Streaming Decoder

The streaming decoder processes CBOR bytes incrementally without requiring
the full message to be in memory first. It is push-based: feed any number of
bytes at a time and receive events via a callback as items are decoded.

```c
#include <stdio.h>
#include <inttypes.h>
#include "cbor/stream.h"

static bool on_event(const cbor_stream_event_t *event,
        const cbor_stream_data_t *data, void *arg)
{
    switch (event->type) {
    case CBOR_STREAM_EVENT_UINT:
        printf("uint: %" PRIu64 " (depth=%u)\n", data->uint, event->depth);
        break;
    case CBOR_STREAM_EVENT_TEXT:
        printf("text: ");
        if (data->str.ptr) {
            fwrite(data->str.ptr, 1, data->str.len, stdout);
        }
        printf(" (first=%d last=%d)\n",
                data->str.first, data->str.last);
        break;
    case CBOR_STREAM_EVENT_MAP_START:
        printf("map start (size=%" PRId64 ")\n", data->container.size);
        break;
    case CBOR_STREAM_EVENT_MAP_END:
        printf("map end\n");
        break;
    default:
        break;
    }
    return true; /* return false to abort decoding */
}

cbor_stream_decoder_t decoder;
cbor_stream_init(&decoder, on_event, NULL);

/* Feed in any chunk size */
cbor_stream_feed(&decoder, chunk1, chunk1_len);
cbor_stream_feed(&decoder, chunk2, chunk2_len);

/* Verify all items were complete */
if (cbor_stream_finish(&decoder) != CBOR_SUCCESS) {
    /* truncated input */
}
```

**Event callback** receives three arguments on every call:

| Field | Description |
| --- | --- |
| `event->type` | Item type (`CBOR_STREAM_EVENT_UINT`, `_TEXT`, `_ARRAY_START`, …) |
| `event->depth` | Nesting depth (0 = top level) |
| `event->is_map_key` | `true` when the item is a map key |
| `event->has_tag` | `true` when the item carries a CBOR tag |
| `event->tag` | Tag number (valid only when `has_tag` is `true`) |
| `data` | Decoded value; `NULL` for `_END` events |
| `arg` | User-supplied context pointer passed to `cbor_stream_init()` |

Note: The streaming decoder supports only one pending tag. A second tag
before the tagged item resolves causes `cbor_stream_feed()` to return
`CBOR_EXCESSIVE`.

Returning `false` from the callback stops decoding and causes
`cbor_stream_feed()` to return `CBOR_ABORTED`.

**String events** (`CBOR_STREAM_EVENT_BYTES` / `CBOR_STREAM_EVENT_TEXT`)
carry chunk metadata in `data->str`:

| Field | Description |
| --- | --- |
| `ptr` | Pointer into the caller's feed buffer (valid during the callback) |
| `len` | Bytes in this chunk |
| `total` | Total string length; `-1` for indefinite-length strings |
| `first` | `true` on the first chunk |
| `last` | `true` on the last chunk |

A definite-length string may arrive in multiple chunks when the payload
spans several `cbor_stream_feed()` calls. An indefinite-length string may emit
one or more chunks per CBOR sub-chunk, followed by a zero-length chunk with
`last = true` for the BREAK terminator.

**Container events** (`_ARRAY_START` / `_MAP_START`) carry
`data->container.size`: the number of items (array) or key-value pairs (map),
or `-1` for indefinite-length.

**API summary:**

```c
/* Initialize (NULL callback is allowed, but feed/finish will return CBOR_INVALID) */
void cbor_stream_init(cbor_stream_decoder_t *decoder,
        cbor_stream_callback_t callback, void *arg);

/* Push bytes; may be called repeatedly with any chunk size */
cbor_error_t cbor_stream_feed(cbor_stream_decoder_t *decoder,
        const void *data, size_t len);

/* Signal end-of-input; returns CBOR_NEED_MORE if input was truncated */
cbor_error_t cbor_stream_finish(cbor_stream_decoder_t *decoder);

/* Reset to initial state (preserves callback and arg) */
void cbor_stream_reset(cbor_stream_decoder_t *decoder);
```

After any error `cbor_stream_feed()` enters a sticky error state and returns
the same error on subsequent calls. Use `cbor_stream_reset()` to reuse the
decoder.

## Tools

### Item counter script

You can estimate item count from CBOR bytes with:

```bash
python3 tools/cbor_item_count.py --hex "A2 61 61 01 61 62 02"
```

or from file:

```bash
python3 tools/cbor_item_count.py --file ./message.cbor
```

If your build overrides `CBOR_RECURSION_MAX_LEVEL`, pass the same depth to the
script so it matches parser behavior:

```bash
python3 tools/cbor_item_count.py --file ./message.cbor --max-depth 16
```

The script prints parser-compatible status (`CBOR_SUCCESS`, `CBOR_BREAK`,
`CBOR_ILLEGAL`, etc.) and counted item number.

### Decoder

```c
union {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64;
	float f32;
	double f64;
	uint8_t s[MTU];
} val;

cbor_decode(&reader, &items[i], &val, sizeof(val));
```

### Encoder

```c
cbor_writer_t writer;

cbor_writer_init(&reader, buf, sizeof(buf));

cbor_encode_map(&writer, 2);
  /* 1st */
  cbor_encode_text_string(&writer, "key");
  cbor_encode_text_string(&writer, "value");
  /* 2nd */
  cbor_encode_text_string(&writer, "age");
  cbor_encode_negative_integer(&writer, -1);
```

## Limitation

* The maximum item length is `size_t` because the interface return type is `size_t`. The argument's value in the specification can go up to `uint64_t` though
* A negative integer ranges down to -2^63-1 other than -2^64 in the specification
* Sorting of encoded map keys is not supported
* Tag item is not implemented yet
* `cbor_unmarshal()` only works on the major type 5: map with string key
