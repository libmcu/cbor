/*
 * SPDX-FileCopyrightText: 2026 권경환 Kyunghwan Kwon <k@libmcu.org>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CBOR_VERSION_H
#define CBOR_VERSION_H

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(CBOR_MAKE_VERSION)
#define CBOR_MAKE_VERSION(major, minor, patch)	\
	(((major) << 16) | ((minor) << 8) | (patch))
#endif

#define CBOR_VERSION_MAJOR	0
#define CBOR_VERSION_MINOR	6
#define CBOR_VERSION_BUILD	0
#define CBOR_VERSION		CBOR_MAKE_VERSION(CBOR_VERSION_MAJOR, \
				CBOR_VERSION_MINOR, \
				CBOR_VERSION_BUILD)

#if defined(__cplusplus)
}
#endif

#endif /* CBOR_VERSION_H */
