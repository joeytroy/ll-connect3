#!/usr/bin/env python3
"""Scan tracked text files for dangerous/invisible Unicode characters.

Guards against "Trojan Source" attacks (CVE-2021-42574), where bidirectional
control characters or invisible code points make source render differently from
how the compiler reads it.

Run from the repo root:  python3 .github/scripts/check-unicode.py
Exits non-zero (and prints file:line:col U+XXXX) if any forbidden code point is
found. Intentional control characters in tests should be written as escapes
(e.g. "\\u200E") rather than literal characters so the source stays clean.

Detection does NOT depend on a file decoding cleanly as UTF-8, and it does not
silently pass anything it fails to inspect -- both are bypasses a security
control must not have:

  * Genuine binaries are skipped (a NUL byte, and no UTF-16/UTF-32 BOM).
  * UTF-8 and BOM-declared UTF-16 / UTF-32 text are decoded and scanned for
    precise file:line:col reporting.
  * Any remaining undecodable *text* file is flagged (a non-UTF-8 text file is
    anomalous in a UTF-8 tree) and byte-scanned for the forbidden code points
    in every encoding that can carry them.
  * A tracked file that cannot be read at all (broken symlink, bad perms) is a
    hard finding -- never skipped silently. Directory targets of tracked
    symlinks (e.g. macOS .framework internals) are not files and are skipped.

Known accepted limitation: a UTF-16/UTF-32 text file with NO byte-order mark is
indistinguishable from binary by the NUL heuristic and is skipped. Such a file
is not valid source for this tree's toolchains (which read UTF-8), so it is not
a realistic vector here.
"""

import subprocess
import sys


# Forbidden code points keyed by integer value, so this scanner is itself pure
# ASCII and never trips over its own table. Limited to characters that are
# unambiguously suspect in a source tree: bidi controls plus zero-width /
# invisible spaces. ZWJ/ZWNJ (U+200C/U+200D) are intentionally omitted because
# they appear in legitimate emoji sequences in Markdown.
FORBIDDEN = {
    # Bidirectional formatting controls (the core Trojan Source vector)
    0x202A: "LEFT-TO-RIGHT EMBEDDING",
    0x202B: "RIGHT-TO-LEFT EMBEDDING",
    0x202C: "POP DIRECTIONAL FORMATTING",
    0x202D: "LEFT-TO-RIGHT OVERRIDE",
    0x202E: "RIGHT-TO-LEFT OVERRIDE",
    0x2066: "LEFT-TO-RIGHT ISOLATE",
    0x2067: "RIGHT-TO-LEFT ISOLATE",
    0x2068: "FIRST STRONG ISOLATE",
    0x2069: "POP DIRECTIONAL ISOLATE",
    0x200E: "LEFT-TO-RIGHT MARK",
    0x200F: "RIGHT-TO-LEFT MARK",
    0x061C: "ARABIC LETTER MARK",
    # Invisible / zero-width spaces
    0x200B: "ZERO WIDTH SPACE",
    0x2060: "WORD JOINER",
    0xFEFF: "ZERO WIDTH NO-BREAK SPACE (BOM)",
}

# Encodings a hidden character could realistically reach a toolchain through.
# Every forbidden code point is > U+00FF, so single-byte legacy encodings
# (Latin-1, Windows-1252, ...) physically cannot represent them -- the Unicode
# transformation formats are the only byte-level vectors. Both endiannesses of
# UTF-16/UTF-32 are covered.
BYTE_ENCODINGS = ("utf-8", "utf-16-le", "utf-16-be", "utf-32-le", "utf-32-be")


def _build_forbidden_bytes():
    """{byte sequence: [(codepoint, encoding), ...]} for the raw-byte backstop.

    A list of hits per sequence (not a single value) so that if two code points
    ever encode to identical bytes the table stays correct instead of silently
    dropping one. No collision exists today; this keeps it safe as FORBIDDEN
    grows. Wrapped in a function so the loop variables don't leak to module
    scope.
    """
    table = {}
    for cp in FORBIDDEN:
        for enc in BYTE_ENCODINGS:
            table.setdefault(chr(cp).encode(enc), []).append((cp, enc))
    return table


FORBIDDEN_BYTES = _build_forbidden_bytes()

# Paths under these prefixes are skipped.
#
# resources/ holds binary assets (icons, images) bundled into the Qt app via
# resources.qrc; nothing there is compiled as source. Authored source
# (kernel/, src/, scripts, docs) is always scanned. Add a prefix here only if
# an upstream import legitimately needs an exception.
EXCLUDE_PREFIXES = (
    "resources/",
)

# Byte-order marks. UTF-32 must be tested before UTF-16: the UTF-32-LE BOM
# (FF FE 00 00) starts with the UTF-16-LE BOM (FF FE), so a naive UTF-16 check
# would misdecode UTF-32-LE as UTF-16 garbage.
_UTF32_BOMS = (b"\xff\xfe\x00\x00", b"\x00\x00\xfe\xff")
_UTF16_BOMS = (b"\xff\xfe", b"\xfe\xff")


def tracked_files():
    out = subprocess.run(
        ["git", "ls-files", "-z"],
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",  # decode paths as UTF-8 regardless of runner locale
    ).stdout
    for path in out.split("\0"):
        if path and not path.startswith(EXCLUDE_PREFIXES):
            yield path


def looks_binary(data):
    """A NUL byte means binary -- unless a UTF-16/UTF-32 BOM marks it as text."""
    if not data:
        return False
    if data.startswith(_UTF32_BOMS) or data.startswith(_UTF16_BOMS):
        return False
    return b"\x00" in data


def bom_text_encoding(data):
    """Return the Python codec for a BOM-declared UTF-16/UTF-32 file, else None.

    The plain "utf-16" / "utf-32" codecs consume the BOM and auto-select the
    endianness from it. UTF-32 is checked first (see _UTF32_BOMS).
    """
    if data.startswith(_UTF32_BOMS):
        return "utf-32"
    if data.startswith(_UTF16_BOMS):
        return "utf-16"
    return None


def scan_text(path, text, findings):
    """Report forbidden code points in decoded text with 1-based line:col."""
    line = 1
    col = 0
    for ch in text:
        if ch == "\n":
            line += 1
            col = 0
            continue
        if ch == "\r":
            # CRLF: the \r is not a column -- let the following \n advance the
            # line so reported columns stay correct on Windows-style endings.
            continue
        col += 1
        name = FORBIDDEN.get(ord(ch))
        if name is not None:
            findings.append("%s:%d:%d: U+%04X %s" % (path, line, col, ord(ch), name))


def main():
    findings = []
    for path in tracked_files():
        try:
            with open(path, "rb") as fh:
                data = fh.read()
        except IsADirectoryError:
            # Tracked symlink that points at a directory (e.g. the macOS
            # .framework internals under thirdparty/) -- not a scannable file.
            continue
        except OSError as e:
            # A tracked file we cannot read (broken symlink, bad perms) is a
            # hard finding -- a security scanner must not pass files it never
            # inspected, so this fails the job rather than being skipped.
            findings.append("%s: unreadable tracked file (%s)" % (path, e))
            continue

        if looks_binary(data):
            continue

        # UTF-8 is the tree's canonical encoding; decode for precise locations.
        try:
            scan_text(path, data.decode("utf-8"), findings)
            continue
        except UnicodeDecodeError:
            pass

        # Not valid UTF-8 but not binary. Honour a declared UTF-16/UTF-32 BOM.
        enc = bom_text_encoding(data)
        if enc is not None:
            try:
                scan_text(path, data.decode(enc), findings)
                continue
            except UnicodeDecodeError:
                pass

        # Undecodable text: do NOT silently skip (that was the old bypass).
        # Flag the anomaly and byte-scan for forbidden sequences as a backstop.
        findings.append("%s: non-UTF-8 text file (cannot verify cleanly; treat as suspect)" % path)
        for seq, hits in FORBIDDEN_BYTES.items():
            if seq in data:
                for cp, enc in hits:
                    findings.append("%s: contains %s-encoded U+%04X %s" % (path, enc, cp, FORBIDDEN[cp]))

    if findings:
        print("Forbidden Unicode characters found:\n")
        for f in findings:
            print("  " + f)
        print("\n%d finding(s). See CVE-2021-42574 (Trojan Source)." % len(findings))
        return 1

    print("Unicode scan OK -- no forbidden characters.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
