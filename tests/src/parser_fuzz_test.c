/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include "cbor/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
	static cbor_item_t items[128];
	cbor_reader_t reader;
	cbor_reader_init(&reader, items, sizeof(items) / sizeof(*items));
	cbor_parse(&reader, Data, Size, NULL);

	return 0;
}
