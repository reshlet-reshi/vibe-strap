#!/usr/bin/env python3
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent
EXPECTED = "vibe-strap\n"
SHELL_FILES = [
    ROOT / "vibe-strap.sh",
    ROOT / "emit.sh",
    ROOT / "inlines/elf/begin.sh",
    ROOT / "inlines/str/begin.sh",
    ROOT / "inlines/str/end.sh",
]


def test_shellcheck():
    subprocess.run(
        [
            "shellcheck",
            "--exclude", "SC1090",
            *map(os.fspath, SHELL_FILES)
        ],
        cwd=ROOT,
        check=True
    )


def test_smoke():
    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        out = tmp_path / "vibe-strap"
        subprocess.run(
            [
                "sh",
                os.fspath(ROOT / "vibe-strap.sh"),
                os.fspath(ROOT / "emit.sh"),
                os.fspath(ROOT / "inlines"),
                os.fspath(out),
            ],
            cwd=tmp_path,
            check=True
        )
        actual = subprocess.check_output(
            [os.fspath(out)],
            text=True
        )
        if actual != EXPECTED:
            raise SystemExit(
                f"expected {EXPECTED!r}, got {actual!r}"
            )
        print("smoke test passed")

if __name__ == "__main__":
    test_shellcheck()
    test_smoke()
