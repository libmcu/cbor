work in progress

Simplicity and code size are considered first. No dynamic allocations.

## Usage
### Decoder

will look like:

```c
struct {
	uint8_t u8_1;
	char txt[3];
	uint8_t u8_2;
} fmt;

uint8_t msg[] = { 0x83,0x01,0x63,0x61,0x62,0x63,0x03 };
cbor_decode(&fmt, sizeof(fmt), msg, sizeof(msg));
```
