## v0.3.0 - September 23, 2022
- **Breaking Change**: Revert `cbor_encode_text_string()` to its original version
- Add `cbor_encode_null_terminated_string()`
- Fix to conform to C99 standard removing `strnlen()`

## v0.2.0 - August 24, 2022
- **Breaking Change**: Change the reader and the parser initializer prototypes
  - `cbor_reader_init()` and `cbor_parse()`
  - Now a reader can be reusable to different messages

## v0.1.0 - August 24, 2022
The initial release.
