/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_HELPER_H
#define CBOR_HELPER_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "cbor/base.h"
#include <stdbool.h>

struct cbor_parser {
	void const *key;
	void (*run)(const struct cbor_parser *parser,
			const void *data, size_t datasize, void *arg);
};

bool cbor_unmarshal(cbor_reader_t *reader,
		const struct cbor_parser *parsers, size_t nr_parsers,
		const void *msg, size_t msglen, void *arg);

size_t cbor_iterate(cbor_reader_t const *reader,
		    cbor_item_t const *parent,
		    void (*callback_each)(cbor_reader_t const *reader,
			    cbor_item_t const *item, cbor_item_t const *parent,
			    void *arg),
		    void *arg);

const char *cbor_stringify_error(cbor_error_t err);
const char *cbor_stringify_item(cbor_item_t *item);

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_HELPER_H */
