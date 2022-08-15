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
set(CBOR_ROOT <THIRD_PARTY_DIR>/cbor)
include(${CBOR_ROOT}/cbor.cmake)
```

## Usage
Please see the [examples](examples).

* `CBOR_BIG_ENDIAN`
  - Define the macro for big endian machine. The default is little endian.
* `CBOR_RECURSION_MAX_LEVEL`
  - This is set to avoid stack overflow from recursion. The default maximum
    depth is 4.

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

cbor_reader_init(&reader, cbor_message, sizeof(cbor_message));
cbor_parse(&reader, items, MAX_ITEMS, &n);

for (i = 0; i < n; i++) {
	printf("item: %s, size: %zu\n",
			cbor_stringify_item(&items[i]),
			cbor_get_item_size(&items[i]);
}
```

### Decoder

```c
union {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64
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

cbor_encode_map_indefinite(&writer);
  /* 1st */
  cbor_encode_text_string(&writer, "key");
  cbor_encode_text_string(&writer, "value");
  /* 2nd */
  cbor_encode_text_string(&writer, "age");
  cbor_encode_negative_integer(&writer, -1);
cbor_encode_break(&writer);
```

## Limitation

* The maximum item length is `size_t` because the interface return type is `size_t`. The argument's value in the specification can go up to `uint64_t` though
* A negative integer ranges down to -2^63-1 other than -2^64 in the specification
* Sorting of encoded map keys is not supported
* Tag item is not implemented yet
