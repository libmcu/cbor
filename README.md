# CBOR
This is a minimalistic implementation for CBOR, the Concise Binary Object
Representation. CBOR is defined by
[IETF RFC 8949](https://datatracker.ietf.org/doc/html/rfc8949), and
Wikipedia has [a good description](https://en.wikipedia.org/wiki/CBOR).

## Features

* C99
* No dynamic memory allocation
* Small code footprint

The parser takes 634 bytes on ARM Cortex-M0 compiling with arm-none-eabi-gcc
10-2020-q4-major, optimizing for code size `-Os`.

Stack usage per the major type functions:

| Major type                                         | Bytes |
| -------------------------------------------------- | ----- |
| 0: unsigned integer                                | 12    |
| 1: negative integer                                | 12    |
| 2: byte string                                     | 56    |
| 3: text string                                     | 56    |
| 4: array                                           | 56    |
| 5: map                                             | 56    |
| 6: tag(not implemented yet)                        | 0     |
| 7: floating-point numbers, simple values and break | 20    |

And the call stack for each recursion is 32 bytes.

## Usage

* `CBOR_BIG_ENDIAN`
  - Define the macro for big endian machine. The default is little endian.
* `CBOR_RECURSION_MAX_LEVEL`
  - This is set to avoid stack overflow from recursion. The default maximum
    depth is 4.

Please see the [examples](examples).

### Parser

```c
cbor_reader_t reader;
cbor_item_t items[MAX_ITEMS];
size_t n;

cbor_reader_init(&reader, cbor_encoded_message, sizeof(cbor_encoded_message));
cbor_parse(&reader, items, sizeof(items) / sizeof(items[0]), &n);

for (i = 0; i < n; i++) {
	printf("item: %s, size: %zu\n",
			cbor_stringify_item(&items[i]), items[i].size);
}
```

### Decoder

```c
union {
	int8_t i8;
	int16_t i16;
	int32_t i32;
	int64_t i64
	uint8_t s[64];
} val;

cbor_decode(&reader, &items[i], &val, sizeof(val));
```

### Encoder

```c
cbor_writer_t writer;

cbor_writer_init(&reader, buf, sizeof(buf));

cbor_encode_map(&writer, 2);
  /* 1st */
  cbor_encode_text_string(&writer, "key", 3);
  cbor_encode_text_string(&writer, "value", 5);
  /* 2nd */
  cbor_encode_text_string(&writer, "age", 1);
  cbor_encode_negative_integer(&writer, -1);
```

## Limitation

* The maximum item length is `size_t` because the interface return type is `size_t`. The argument's value in the specification can go up to `uint64_t` though
* A negative integer ranges down to -2^63-1 other than -2^64 in the specification
* Tag item is not implemented yet
* Float and simple value for encoder are not implemented yet
