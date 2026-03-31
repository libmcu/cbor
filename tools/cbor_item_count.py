#!/usr/bin/env python3

import argparse
import re
import sys

CBOR_SUCCESS = "CBOR_SUCCESS"
CBOR_ILLEGAL = "CBOR_ILLEGAL"
CBOR_INVALID = "CBOR_INVALID"
CBOR_OVERRUN = "CBOR_OVERRUN"
CBOR_BREAK = "CBOR_BREAK"
CBOR_EXCESSIVE = "CBOR_EXCESSIVE"

CBOR_INDEFINITE_VALUE = -1
CBOR_RESERVED_VALUE = -2
DEFAULT_CBOR_RECURSION_MAX_LEVEL = 8


def get_following_bytes(additional_info: int) -> int:
    if additional_info < 24:
        return 0
    if additional_info == 31:
        return CBOR_INDEFINITE_VALUE
    if additional_info >= 28:
        return CBOR_RESERVED_VALUE
    return 1 << (additional_info - 24)


class Counter:
    def __init__(self, data: bytes, max_depth: int = DEFAULT_CBOR_RECURSION_MAX_LEVEL):
        self.data = data
        self.msgsize = len(data)
        self.msgidx = 0
        self.itemidx = 0
        self.recursion_depth = 0
        self.max_depth = max_depth

    def has_valid_following_bytes(self, following_bytes: int):
        if following_bytes == CBOR_RESERVED_VALUE:
            return False, CBOR_ILLEGAL
        if following_bytes == CBOR_INDEFINITE_VALUE:
            return True, CBOR_SUCCESS
        if (following_bytes + 1) > (self.msgsize - self.msgidx):
            return False, CBOR_ILLEGAL
        return True, CBOR_SUCCESS

    def go_get_item_length(self, additional_info: int, following_bytes: int):
        if following_bytes == CBOR_INDEFINITE_VALUE:
            length = CBOR_INDEFINITE_VALUE
            offset = 0
        elif following_bytes == 0:
            length = additional_info
            offset = 0
        else:
            begin = self.msgidx + 1
            end = begin + following_bytes
            length = int.from_bytes(self.data[begin:end], byteorder="big", signed=False)
            offset = following_bytes
        self.msgidx += offset + 1
        return length

    def parse(self, maxitems: int):
        self.recursion_depth += 1
        if self.recursion_depth > self.max_depth:
            self.recursion_depth -= 1
            return CBOR_EXCESSIVE

        try:
            err = CBOR_SUCCESS
            i = 0
            while self._can_parse_next(maxitems, i):
                err = self._parse_one_item()
                if self._should_stop_after_item(err, maxitems):
                    break

                err = CBOR_SUCCESS
                i += 1

            return err
        finally:
            self.recursion_depth -= 1

    def _can_parse_next(self, maxitems: int, parsed_items: int) -> bool:
        limit_ok = maxitems == CBOR_INDEFINITE_VALUE or parsed_items < maxitems
        return limit_ok and self.msgidx < self.msgsize

    def _parse_one_item(self):
        val = self.data[self.msgidx]
        major_type = val >> 5
        additional_info = val & 0x1F
        following_bytes = get_following_bytes(additional_info)

        ok, err = self.has_valid_following_bytes(following_bytes)
        if not ok:
            return err

        return self._dispatch_parser(major_type, additional_info, following_bytes)

    def _dispatch_parser(self, major_type: int, additional_info: int,
                         following_bytes: int):
        if major_type in (0, 1):
            return self.do_integer(following_bytes)
        if major_type in (2, 3):
            return self.do_string(additional_info, following_bytes)
        if major_type in (4, 5):
            return self.do_recursive(major_type, additional_info, following_bytes)
        if major_type == 6:
            return CBOR_INVALID
        if major_type == 7:
            return self.do_float_and_other(following_bytes)
        return CBOR_ILLEGAL

    def _should_stop_after_item(self, err: str, maxitems: int) -> bool:
        if err == CBOR_BREAK:
            return maxitems == CBOR_INDEFINITE_VALUE or self.msgidx == self.msgsize
        return err != CBOR_SUCCESS

    def do_integer(self, following_bytes: int):
        if following_bytes == CBOR_INDEFINITE_VALUE:
            return CBOR_ILLEGAL
        self.msgidx += following_bytes + 1
        self.itemidx += 1
        return CBOR_SUCCESS

    def do_string(self, additional_info: int, following_bytes: int):
        length = self.go_get_item_length(additional_info, following_bytes)
        self.itemidx += 1

        if length == CBOR_INDEFINITE_VALUE:
            err = self.parse(CBOR_INDEFINITE_VALUE)
            return err
        if length > (self.msgsize - self.msgidx):
            return CBOR_ILLEGAL

        self.msgidx += length
        return CBOR_SUCCESS

    def do_recursive(self, major_type: int, additional_info: int,
                     following_bytes: int):
        length = self.go_get_item_length(additional_info, following_bytes)
        expected_items = length
        self.itemidx += 1

        if length != CBOR_INDEFINITE_VALUE and major_type == 5:
            expected_items = length * 2

        if length == CBOR_INDEFINITE_VALUE:
            err = self.parse(CBOR_INDEFINITE_VALUE)
            return err

        for _ in range(expected_items):
            itemidx_before = self.itemidx
            msgidx_before = self.msgidx
            err = self.parse(1)
            if err != CBOR_SUCCESS:
                return err
            if self.itemidx == itemidx_before and self.msgidx == msgidx_before:
                return CBOR_ILLEGAL

        return CBOR_SUCCESS

    def do_float_and_other(self, following_bytes: int):
        if following_bytes == CBOR_INDEFINITE_VALUE:
            self.msgidx += 1
            self.itemidx += 1
            return CBOR_BREAK

        ok, err = self.has_valid_following_bytes(following_bytes)
        if not ok:
            return err

        self.msgidx += following_bytes + 1
        self.itemidx += 1
        return CBOR_SUCCESS


def count_items(data: bytes, max_depth: int = DEFAULT_CBOR_RECURSION_MAX_LEVEL):
    counter = Counter(data, max_depth=max_depth)
    err = counter.parse(counter.msgsize)
    if err == CBOR_SUCCESS and counter.msgidx < counter.msgsize:
        err = CBOR_OVERRUN
    return err, counter.itemidx


def parse_hex_input(hex_input: str) -> bytes:
    s = hex_input.strip()
    if not s:
        return b""

    s = s.replace(",", " ")
    if any(ch.isspace() for ch in s):
        tokens = [t for t in re.split(r"\s+", s) if t]
        parts = []
        for t in tokens:
            if t.lower().startswith("0x"):
                t = t[2:]
            if len(t) == 0:
                continue
            if len(t) % 2 != 0:
                t = "0" + t
            parts.append(t)
        return bytes.fromhex("".join(parts))

    if s.lower().startswith("0x"):
        s = s[2:]
    if len(s) % 2 != 0:
        s = "0" + s
    return bytes.fromhex(s)


def load_input(args) -> bytes:
    if args.hex is not None:
        return parse_hex_input(args.hex)
    if args.file is not None:
        with open(args.file, "rb") as f:
            return f.read()

    data = sys.stdin.buffer.read()
    if not data:
        raise ValueError("no input given")
    return data


def main() -> int:
    parser = argparse.ArgumentParser(description="Count CBOR items from input message")
    parser.add_argument("--hex", help="CBOR bytes as hex string")
    parser.add_argument("--file", help="path to CBOR binary file")
    parser.add_argument(
        "--max-depth",
        type=int,
        default=DEFAULT_CBOR_RECURSION_MAX_LEVEL,
        help=(
            "maximum recursion depth to allow while parsing "
            f"(default: {DEFAULT_CBOR_RECURSION_MAX_LEVEL})"
        ),
    )
    args = parser.parse_args()

    if args.max_depth < 1:
        print("input error: --max-depth must be >= 1", file=sys.stderr)
        return 2

    try:
        data = load_input(args)
    except Exception as e:
        print(f"input error: {e}", file=sys.stderr)
        return 2

    err, nitems = count_items(data, max_depth=args.max_depth)
    print(f"error: {err}")
    print(f"items: {nitems}")

    if err in (CBOR_SUCCESS, CBOR_BREAK):
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
