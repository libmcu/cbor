# CBOR
Simplicity and code size are considered first.

The decoder takes 702 bytes for ARM Cortex-M0, using arm-none-eabi-gcc
10-2020-q4-major with optimization for code size `-Os`.

Stack usage per the major type function:

| Major type                                         | Bytes |
| -------------------------------------------------- | ----- |
| 0: unsigned integer                                | 16    |
| 1: negative integer                                | 40    |
| 2: byte string                                     | 24    |
| 3: text string                                     | 24    |
| 4: array                                           | 8     |
| 5: map                                             | 8     |
| 6: tag(not implemented yet)                        | 0     |
| 7: floating-point numbers, simple values and break | 8     |

## Features

* C99
* No dynamic allocations
* Small memory footprint

## Usage

* `CBOR_BIG_ENDIAN`
* `CBOR_RECURSION_MAX_LEVEL`

### Decoder

```c
struct {
	uint8_t u8_1;
	char txt[3];
	uint8_t u8_2;
} fmt;

uint8_t msg[] = { 0x83,0x01,0x63,0x61,0x62,0x63,0x03 };
cbor_decode(&fmt, sizeof(fmt), msg, sizeof(msg));
```

### Encoder

## Limitation

* The maximum item length is `size_t` because the interface return type is `size_t`. The argument's value in the specification can go up to `uint64_t` though
* A negative integer ranges down to -2^63-1 other than -2^64 in the specification
* Tag item and simple value are not implemented yet
* Encoder is not implemented yet
