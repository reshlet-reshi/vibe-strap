#!/usr/bin/env python3
import os
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / ".ignore/vibe-strap"
EXPECTED = "vibe-strap"

if __name__ == "__main__":
    try:
        subprocess.run(
            ["sh", "vibe-strap.sh", os.fspath(OUT)],
            cwd=ROOT,
            check=True
        )
        actual = subprocess.check_output(
            [os.fspath(OUT)], 
            text=True
        ).rstrip("\n")
        if actual != EXPECTED:
            raise SystemExit(
                f"expected {EXPECTED!r}, got {actual!r}"
            )
        print("smoke test passed")
    finally:
        try:
            OUT.unlink()
        except FileNotFoundError:
            pass
