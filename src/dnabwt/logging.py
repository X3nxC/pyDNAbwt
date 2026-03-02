"""Package logger setup."""

from __future__ import annotations

import logging
from pathlib import Path
from typing import Optional

_LOGGER_NAME = "dnabwt"
logger = logging.getLogger(_LOGGER_NAME)


_DEF_FMT = "%(asctime)s | %(levelname)s | %(name)s | %(message)s"


def _ensure_default_handler() -> None:
    if logger.handlers:
        return
    handler = logging.StreamHandler()
    handler.setFormatter(logging.Formatter(_DEF_FMT))
    logger.addHandler(handler)


_ensure_default_handler()
logger.setLevel(logging.WARNING)
logger.propagate = False


def configure_logger(level: int = logging.WARNING, log_path: Optional[str] = None) -> logging.Logger:
    """Configure package logger output level and optional file sink."""
    logger.setLevel(level)

    if log_path:
        p = Path(log_path)
        p.parent.mkdir(parents=True, exist_ok=True)
        file_handler = logging.FileHandler(p, encoding="utf-8")
        file_handler.setFormatter(logging.Formatter(_DEF_FMT))
        logger.addHandler(file_handler)

    return logger
