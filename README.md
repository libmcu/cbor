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
* Byte/text string: string length in bytes
* Array: number of child items
* Map: number of key-value pairs

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
if (err == CBOR_SUCCESS || err == CBOR_BREAK) {
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
