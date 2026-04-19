/*
 * SPDX-FileCopyrightText: 2021 Kyunghwan Kwon <k@mononn.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef HELPER_INLINE_DECL_TEST_H
#define HELPER_INLINE_DECL_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

struct helper_inline_decl_counts {
	size_t certificate;
	size_t private_key;
	size_t dev_url;
	size_t prod_url;
};

bool helper_inline_decl_dispatch_depth1(const uint8_t *msg, size_t msglen,
		struct helper_inline_decl_counts *counts);
bool helper_inline_decl_dispatch_depth2(const uint8_t *msg, size_t msglen,
		struct helper_inline_decl_counts *counts);

#if defined(__cplusplus)
}
#endif

#endif /* HELPER_INLINE_DECL_TEST_H */
