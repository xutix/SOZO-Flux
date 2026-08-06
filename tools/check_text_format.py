#!/usr/bin/env python3
"""Check repository text files for UTF-8 and whitespace hygiene."""

from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
TEXT_SUFFIXES = {
    ".cpp",
    ".css",
    ".h",
    ".html",
    ".ini",
    ".json",
    ".md",
    ".mjs",
    ".ps1",
    ".py",
    ".txt",
    ".yaml",
    ".yml",
}
TEXT_NAMES = {".editorconfig", ".gitattributes", ".gitignore", "LICENSE", "VERSION"}


def repository_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "-co", "--exclude-standard", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [ROOT / item.decode("utf-8") for item in result.stdout.split(b"\0") if item]


def main() -> int:
    errors: list[str] = []
    checked = 0
    for path in repository_files():
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in TEXT_NAMES:
            continue
        checked += 1
        try:
            content = path.read_bytes().decode("utf-8")
        except UnicodeDecodeError as error:
            errors.append(f"{path.relative_to(ROOT)}: not valid UTF-8 ({error})")
            continue

        if content and not content.endswith(("\n", "\r")):
            errors.append(f"{path.relative_to(ROOT)}: missing final newline")
        for line_number, line in enumerate(content.splitlines(), start=1):
            if line.endswith((" ", "\t")):
                errors.append(
                    f"{path.relative_to(ROOT)}:{line_number}: trailing whitespace"
                )

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Text format check passed for {checked} files.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
