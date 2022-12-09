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
