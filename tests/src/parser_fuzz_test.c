#include "cbor/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	static cbor_item_t items[128];
	cbor_reader_t reader;
	cbor_reader_init(&reader, Data, Size);
	cbor_parse(&reader, items, sizeof(items) / sizeof(items[0]), NULL);

	return 0;
}
