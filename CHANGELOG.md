## v0.6.0 - April 18, 2026
- **Breaking Change**: Replace `.key`/`.keylen` parser API with path-based API in `cbor_unmarshal`
- Add path-based dispatch to `cbor_unmarshal`
- Add wildcard path segment for CBOR parser
- Add compile-time path depth check macro
- Fix wildcard macro and matching logic clarification

## v0.5.0 - April 2, 2026
- Add streaming CBOR decoder API

## v0.4.0 - April 2, 2026
- Add CBOR tag support for encoding and parsing
- Fix text string overrun handling in encoder
- Fix encoder rejecting non-negative input for negative integer

## v0.3.2 - April 1, 2026
- Add `cbor_count_items` for dry-run item counting
- Fix iteration logic in `iterate_each`
- Fix out-of-bound access in `iterate_each` helper
- Fix out-of-bound access on unmarshal in helper
- Add int key unmarshal support in helper

## v0.3.1 - December 9, 2022
- Add CMake scaffolding
- Fix garbage data in the uninitialized buffer when decoding
- Separate base.h from cbor.h to use cbor.h as single header file with all the necessary includes
- Add unmarshal functionality for map
- Fix not working break stop code on indefinite array

## v0.3.0 - September 23, 2022
- **Breaking Change**: Revert `cbor_encode_text_string()` to its original version
- Add `cbor_encode_null_terminated_string()`
- Fix to conform to C99 standard removing `strnlen()`
- Increase the maximum recursion level from 4 to 8

## v0.2.0 - August 24, 2022
- **Breaking Change**: Change the reader and the parser initializer prototypes
  - `cbor_reader_init()` and `cbor_parse()`
  - Now a reader can be reusable to different messages

## v0.1.0 - August 24, 2022
The initial release.
