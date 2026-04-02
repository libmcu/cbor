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
| 6: tag                                             | 32    |
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

Each `cbor_item_t` represents one parsed CBOR node (container, tag, and leaf
all count as one item). `cbor_get_item_size()` unit depends on the item type:

* Integer / float / simple value: encoded payload size in bytes
* Byte/text string (definite-length): string length in bytes
* Array (definite-length): number of child items
* Map (definite-length): number of key-value pairs
* Tag: tag number (e.g. `1` for epoch-based date/time)

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

When a value is tagged, the tag occupies its own item slot immediately before
the wrapped item. For `{"a": tag(1, 1234567890)}`:

* 1 map item
* 1 key item (`"a"`)
* 1 tag item (tag number = `1`, `cbor_get_item_size()` returns `1`)
* 1 integer item (`1234567890`)
* total parsed item count (`n`) = 4

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

The streaming decoder processes CBOR byte-by-byte (or in any chunk size) without
requiring the full message up front.  It issues events via a callback as each
item is decoded, so it works on constrained systems where the complete payload
may not fit in RAM at once.

```c
#include "cbor/stream.h"

static bool on_event(const cbor_stream_event_t *event,
        const cbor_stream_data_t *data, void *arg)
{
    switch (event->type) {
    case CBOR_STREAM_EVENT_UINT:
        printf("uint: %llu\n", (unsigned long long)data->uint);
        break;
    case CBOR_STREAM_EVENT_INT:
        printf("int: %lld\n", (long long)data->sint);
        break;
    case CBOR_STREAM_EVENT_TEXT:
        printf("text[%zu/%lld]: %.*s\n",
               data->str.len, (long long)data->str.total,
               (int)data->str.len,
               data->str.ptr ? (const char *)data->str.ptr : "");
        break;
    case CBOR_STREAM_EVENT_BYTES:
        /* data->str.ptr points into the caller's buffer, or is NULL when len == 0 */
        break;
    case CBOR_STREAM_EVENT_ARRAY_START:
        printf("array(%lld) depth=%u\n",
               (long long)data->container.size, event->depth);
        break;
    case CBOR_STREAM_EVENT_ARRAY_END:
        break;
    case CBOR_STREAM_EVENT_MAP_START:
        printf("map(%lld) depth=%u key=%d\n",
               (long long)data->container.size,
               event->depth, event->is_map_key);
        break;
    case CBOR_STREAM_EVENT_MAP_END:
        break;
    case CBOR_STREAM_EVENT_TAG:
        printf("tag(%llu)\n", (unsigned long long)data->tag);
        break;
    case CBOR_STREAM_EVENT_FLOAT:
        printf("float: %g\n", data->flt);
        break;
    case CBOR_STREAM_EVENT_BOOL:
        printf("bool: %s\n", data->boolean ? "true" : "false");
        break;
    case CBOR_STREAM_EVENT_NULL:
        printf("null\n");
        break;
    default:
        break;
    }
    return true; /* return false to abort decoding */
}

cbor_stream_decoder_t decoder;
cbor_stream_init(&decoder, on_event, NULL);

/* Feed any number of chunks — partial items are held in decoder state */
cbor_error_t err = cbor_stream_feed(&decoder, chunk, chunk_len);
if (err != CBOR_SUCCESS) {
    /* handle error */
}

/* After the last chunk, verify the stream ended on a clean boundary */
err = cbor_stream_finish(&decoder);
```

Feeding can be split across as many calls as needed:

```c
/* byte-at-a-time — fully supported */
for (size_t i = 0; i < msg_len; i++) {
    cbor_stream_feed(&decoder, &msg[i], 1);
}
cbor_stream_finish(&decoder);
```

To reuse the decoder after an error, call `cbor_stream_reset()`:

```c
if (cbor_stream_feed(&decoder, bad_data, len) != CBOR_SUCCESS) {
    cbor_stream_reset(&decoder); /* clears error, preserves callback */
}
```

#### Events

| Event | `data` field | Notes |
| --- | --- | --- |
| `CBOR_STREAM_EVENT_UINT` | `data->uint` | unsigned integer |
| `CBOR_STREAM_EVENT_INT` | `data->sint` | negative integer |
| `CBOR_STREAM_EVENT_BYTES` / `_TEXT` | `data->str` | `ptr` into caller's buffer, or `NULL` when `len == 0`; indefinite strings end with a final zero-length BREAK chunk carrying `last=true` |
| `CBOR_STREAM_EVENT_ARRAY_START` / `_END` | `data->container.size` (`-1` if indefinite) / NULL | container open/close |
| `CBOR_STREAM_EVENT_MAP_START` / `_END` | `data->container.size` (`-1` if indefinite) / NULL | `event->is_map_key` tracks key/value position |
| `CBOR_STREAM_EVENT_TAG` | `data->tag` | emitted before the wrapped item's event(s) |
| `CBOR_STREAM_EVENT_FLOAT` | `data->flt` | half/single/double all promoted to `double` |
| `CBOR_STREAM_EVENT_BOOL` | `data->boolean` | |
| `CBOR_STREAM_EVENT_NULL` / `_UNDEFINED` | (unused) | callback receives a non-NULL `data` pointer, but no payload field is used |
| `CBOR_STREAM_EVENT_SIMPLE` | `data->simple` | unassigned simple value |

#### Error codes

| Code | Meaning |
| --- | --- |
| `CBOR_SUCCESS` | all bytes consumed cleanly |
| `CBOR_NEED_MORE` | `finish()` called with an incomplete item or open container |
| `CBOR_ILLEGAL` | reserved additional-info byte; malformed encoding |
| `CBOR_INVALID` | well-formed but semantically invalid (e.g. negative integer overflows `int64`) |
| `CBOR_EXCESSIVE` | nesting deeper than `CBOR_RECURSION_MAX_LEVEL` or tag nesting beyond `CBOR_STREAM_MAX_PENDING_TAGS` |
| `CBOR_ABORTED` | callback returned `false` |

After any non-`CBOR_SUCCESS` return produced while operating on a valid decoder
instance, the decoder is in a sticky error state. Subsequent `feed()` /
`finish()` calls return the same error immediately until you call
`cbor_stream_reset()`.

API-level argument validation failures (for example `NULL` decoder/callback, or
`NULL` data with non-zero length) may also return `CBOR_INVALID`, but they do
not modify decoder state and therefore are not sticky.

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

cbor_writer_init(&writer, buf, sizeof(buf));

cbor_encode_map(&writer, 2);
  /* 1st */
  cbor_encode_text_string(&writer, "key", 3);
  cbor_encode_text_string(&writer, "value", 5);
  /* 2nd */
  cbor_encode_text_string(&writer, "age", 3);
  cbor_encode_negative_integer(&writer, -1);
```

#### Tags (RFC 8949 §3.4)

Call `cbor_encode_tag()` immediately before the item it wraps. The caller is
responsible for ensuring the tag and its wrapped item are written in order — the
same convention used by `cbor_encode_array()` and `cbor_encode_map()`.

```c
/* tag(1, 1234567890) — epoch-based date/time */
cbor_encode_tag(&writer, 1);
cbor_encode_unsigned_integer(&writer, 1234567890);

/* tag(0, "2006-01-13T20:11:12Z") — date/time string */
cbor_encode_tag(&writer, 0);
cbor_encode_text_string(&writer, "2006-01-13T20:11:12Z", 20);

/* Nested tags: tag(1, tag(2, 0)) */
cbor_encode_tag(&writer, 1);
cbor_encode_tag(&writer, 2);
cbor_encode_unsigned_integer(&writer, 0);
```

#### Decoding tagged values

The parser records each tag as a `CBOR_ITEM_TAG` item. Use
`cbor_get_tag_number()` to read the tag number; the item immediately following
it in the `items[]` array is the wrapped value.

```c
/* tag(1, 1234567890) — epoch-based date/time (RFC 8949 §3.4.2)
 * Encoded: 0xC1 0x1A 0x49 0x96 0x02 0xD2 */
uint8_t msg[] = { 0xC1, 0x1A, 0x49, 0x96, 0x02, 0xD2 };

cbor_reader_t reader;
cbor_item_t items[4];
size_t n;

cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
cbor_parse(&reader, msg, sizeof(msg), &n);  /* n == 2 */

/* items[0]: CBOR_ITEM_TAG,    cbor_get_tag_number() == 1   */
/* items[1]: CBOR_ITEM_INTEGER, value == 1234567890          */
if (items[0].type == CBOR_ITEM_TAG) {
    cbor_tag_t tag = cbor_get_tag_number(&items[0]);

    uint32_t timestamp = 0;
    cbor_decode(&reader, &items[1], &timestamp, sizeof(timestamp));

    /* tag == 1, timestamp == 1234567890 */
}
```

With `cbor_iterate()`, `CBOR_ITEM_TAG` items are delivered to the callback
before the wrapped item, so the tag number can be inspected in context:

```c
static void on_item(const cbor_reader_t *reader,
        const cbor_item_t *item, const cbor_item_t *parent, void *arg)
{
    if (item->type == CBOR_ITEM_TAG) {
        printf("tag %zu\n", cbor_get_tag_number(item));
        return;
    }

    int64_t val = 0;
    cbor_decode(reader, item, &val, sizeof(val));
    printf("value %lld\n", (long long)val);
}
```

`cbor_unmarshal()` treats tags as transparent: the parser callback registered
for a map key receives the wrapped value item, not the tag item, regardless of
how many tags are stacked.

## Limitation

* The maximum item length is `size_t` because the interface return type is `size_t`. The argument's value in the specification can go up to `uint64_t` though
* A negative integer ranges down to -2^63-1 other than -2^64 in the specification
* Sorting of encoded map keys is not supported
* On 32-bit targets where `size_t` is 32 bits, tag numbers above `2^32 - 1`
  cannot be represented in `cbor_item_t.size`; `cbor_parse()` returns
  `CBOR_INVALID` for such values
* `cbor_unmarshal()` only works on the major type 5: map with string key
