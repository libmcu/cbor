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

        err = CBOR_SUCCESS
        i = 0
        while (maxitems == CBOR_INDEFINITE_VALUE or i < maxitems) and self.msgidx < self.msgsize:
            val = self.data[self.msgidx]
            major_type = val >> 5
            additional_info = val & 0x1F
            following_bytes = get_following_bytes(additional_info)

            ok, err = self.has_valid_following_bytes(following_bytes)
            if not ok:
                break

            if major_type in (0, 1):
                err = self.do_integer(following_bytes)
            elif major_type in (2, 3):
                err = self.do_string(additional_info, following_bytes)
            elif major_type in (4, 5):
                err = self.do_recursive(additional_info, following_bytes)
            elif major_type == 6:
                err = CBOR_INVALID
            elif major_type == 7:
                err = self.do_float_and_other(following_bytes)
            else:
                err = CBOR_ILLEGAL

            if err == CBOR_BREAK:
                if maxitems == CBOR_INDEFINITE_VALUE or self.msgidx == self.msgsize:
                    break
            elif err != CBOR_SUCCESS:
                break

            err = CBOR_SUCCESS
            i += 1

        self.recursion_depth -= 1
        return err

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
            return self.parse(CBOR_INDEFINITE_VALUE)
        if length > (self.msgsize - self.msgidx):
            return CBOR_ILLEGAL

        self.msgidx += length
        return CBOR_SUCCESS

    def do_recursive(self, additional_info: int, following_bytes: int):
        length = self.go_get_item_length(additional_info, following_bytes)
        self.itemidx += 1

        if length != CBOR_INDEFINITE_VALUE and length > (self.msgsize - self.msgidx):
            return CBOR_ILLEGAL

        return self.parse(length)

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
