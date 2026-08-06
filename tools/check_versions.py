#!/usr/bin/env python3
"""Verify that the release manifest matches firmware and protocol constants."""

from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]


def read_manifest() -> dict[str, str]:
    values: dict[str, str] = {}
    for line in (ROOT / "VERSION").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        if not separator or not key or not value:
            raise ValueError(f"invalid VERSION line: {line!r}")
        values[key] = value
    return values


def extract(pattern: str, text: str, label: str) -> str:
    match = re.search(pattern, text)
    if match is None:
        raise ValueError(f"could not find {label}")
    return match.group(1)


def main() -> int:
    expected_keys = {"platform", "gateway_s3", "node_c3", "protocol"}
    manifest = read_manifest()
    if set(manifest) != expected_keys:
        print(
            "VERSION keys must be exactly: " + ", ".join(sorted(expected_keys)),
            file=sys.stderr,
        )
        return 1

    version_header = (
        ROOT / "SOZO-Common/lib/SozoVersion/src/SozoVersion.h"
    ).read_text(encoding="utf-8")
    protocol_header = (
        ROOT / "SOZO-Common/lib/SozoNodeProtocol/src/SozoNodeProtocol.h"
    ).read_text(encoding="utf-8")

    actual = {
        "platform": extract(r'kPlatform\[\] = "([^"]+)"', version_header, "platform version"),
        "gateway_s3": extract(r'kGatewayS3\[\] = "([^"]+)"', version_header, "gateway version"),
        "node_c3": extract(r'kNodeC3\[\] = "([^"]+)"', version_header, "node version"),
        "protocol": extract(r"kProtocolVersion = (\d+)", protocol_header, "protocol version"),
    }

    mismatches = [
        f"{key}: VERSION={manifest[key]!r}, source={actual[key]!r}"
        for key in sorted(expected_keys)
        if manifest[key] != actual[key]
    ]
    if mismatches:
        print("Version mismatch:\n" + "\n".join(mismatches), file=sys.stderr)
        return 1

    print(
        "Versions aligned: "
        + ", ".join(f"{key}={manifest[key]}" for key in sorted(expected_keys))
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
