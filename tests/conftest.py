from __future__ import annotations

import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
if str(SRC) not in sys.path:
    sys.path.insert(0, str(SRC))

for build_lib in ROOT.glob("build/lib.*"):
    if build_lib.is_dir() and str(build_lib) not in sys.path:
        sys.path.insert(0, str(build_lib))

pytest.importorskip("dnabwt._core_native")
