"""tqdm-based progress bridge for C callbacks."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Optional

try:
    from tqdm import tqdm
except Exception:  # pragma: no cover
    tqdm = None


@dataclass
class ProgressReporter:
    enabled: bool = True
    desc: str = "dnabwt"

    def __post_init__(self) -> None:
        self._bar = None
        self._last = 0

    def callback(self, processed: int, total: int) -> int:
        if not self.enabled or tqdm is None:
            return 0
        if self._bar is None:
            self._bar = tqdm(total=total or None, desc=self.desc, unit="char", leave=False)
        delta = processed - self._last
        if delta > 0:
            self._bar.update(delta)
            self._last = processed
        return 0

    def close(self) -> None:
        if self._bar is not None:
            self._bar.close()
            self._bar = None


def make_progress_callback(enabled: bool, desc: str) -> tuple[Optional[Callable[[int, int], int]], Optional[ProgressReporter]]:
    if not enabled:
        return None, None
    reporter = ProgressReporter(enabled=True, desc=desc)
    return reporter.callback, reporter
