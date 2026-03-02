"""Public API for BWT transform/search."""

from __future__ import annotations

import importlib
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

_core = importlib.import_module("dnabwt._core_native")
from .exceptions import (
    NotInitializedError,
    ParameterError,
    map_core_error,
)
from .logging import logger
from .progress import make_progress_callback


@dataclass
class _SearchConfig:
    progress: bool = True
    check_signal: bool = True
    block_size: int = 512


@dataclass
class _TransformConfig:
    progress: bool = True
    check_signal: bool = True
    io_buffer_size: int = 65536


class BWTSearcher:
    def __init__(self, **kwargs: Any) -> None:
        self._initialized = False
        self._config = _SearchConfig()
        self._search_index: Any | None = None
        self._temp_encoded_path: Path | None = None
        if kwargs:
            self.update_parameter(**kwargs)

    def __del__(self) -> None:
        self._cleanup_temp_file()

    def _cleanup_temp_file(self) -> None:
        if self._temp_encoded_path is not None:
            try:
                self._temp_encoded_path.unlink(missing_ok=True)
            except Exception:  # noqa: BLE001
                pass
            self._temp_encoded_path = None

    def _reset_index_state(self) -> None:
        self._search_index = None
        self._cleanup_temp_file()

    @staticmethod
    def print_help() -> None:
        print("BWTSearcher parameters:")
        print("  progress: bool (default=True)")
        print("  check_signal: bool (default=True)")
        print("  block_size: int > 0 (default=512)")

    @staticmethod
    def validate_parameter(**kwargs: Any) -> None:
        valid_keys = {"progress", "check_signal", "block_size"}
        for key in kwargs:
            if key not in valid_keys:
                raise ParameterError(f"Unknown parameter: {key}")
        if "progress" in kwargs and not isinstance(kwargs["progress"], bool):
            raise ParameterError("progress must be bool")
        if "check_signal" in kwargs and not isinstance(kwargs["check_signal"], bool):
            raise ParameterError("check_signal must be bool")
        if "block_size" in kwargs:
            v = kwargs["block_size"]
            if not isinstance(v, int) or v <= 0:
                raise ParameterError("block_size must be int > 0")

    def update_parameter(self, **kwargs: Any) -> None:
        try:
            self.validate_parameter(**kwargs)
        except ParameterError:
            self.print_help()
            raise
        for k, v in kwargs.items():
            setattr(self._config, k, v)
        self._initialized = True

    def _core_kwargs(self, desc: str) -> tuple[dict[str, Any], Any]:
        progress_cb, reporter = make_progress_callback(self._config.progress, desc)
        kwargs: dict[str, Any] = {"progress_cb": progress_cb}
        if not self._config.check_signal:
            kwargs["signal_cb"] = lambda: 0
        return kwargs, reporter

    def _build_index_from_encoded_path(self, encoded_path: str) -> None:
        kwargs, reporter = self._core_kwargs("build-index")
        try:
            self._search_index = _core.build_search_index_from_encoded_file(
                path=encoded_path,
                block_size=self._config.block_size,
                cache_count=2,
                **kwargs,
            )
        except Exception as exc:  # noqa: BLE001
            raise map_core_error(exc, phase="build_index") from exc
        finally:
            if reporter:
                reporter.close()

    def build_index_from_file(self, filepath: str) -> None:
        if not self._initialized:
            raise NotInitializedError("BWTSearcher is not initialized")
        p = Path(filepath)
        if not p.exists():
            raise ParameterError(f"File not found: {filepath}")

        self._reset_index_state()
        self._build_index_from_encoded_path(str(p))

    def build_index_from_text(self, text: str) -> None:
        if not self._initialized:
            raise NotInitializedError("BWTSearcher is not initialized")

        self._reset_index_state()
        transform_kwargs, reporter = self._core_kwargs("build-index-transform")
        tmp_path: Path | None = None
        try:
            encoded = _core.transform_from_text(text=text, **transform_kwargs)
            with tempfile.NamedTemporaryFile(prefix="dnabwt_", suffix=".rbwt", delete=False) as tmp:
                tmp.write(encoded)
                tmp_path = Path(tmp.name)

            self._build_index_from_encoded_path(str(tmp_path))
            self._temp_encoded_path = tmp_path
        except Exception as exc:  # noqa: BLE001
            if tmp_path is not None:
                tmp_path.unlink(missing_ok=True)
            raise map_core_error(exc, phase="build_index") from exc
        finally:
            if reporter:
                reporter.close()

    def search(self, pattern: str) -> int:
        if not self._initialized:
            raise NotInitializedError("BWTSearcher is not initialized")
        if self._search_index is None:
            raise NotInitializedError("index has not been built")

        kwargs, reporter = self._core_kwargs("search")
        try:
            return int(_core.search_with_index(index=self._search_index, pattern=pattern, **kwargs))
        except Exception as exc:  # noqa: BLE001
            raise map_core_error(exc, phase="search") from exc
        finally:
            if reporter:
                reporter.close()


class BWTTransformer:
    def __init__(self, **kwargs: Any) -> None:
        self._initialized = False
        self._config = _TransformConfig()
        if kwargs:
            self.update_parameter(**kwargs)

    @staticmethod
    def print_help() -> None:
        print("BWTTransformer parameters:")
        print("  progress: bool (default=True)")
        print("  check_signal: bool (default=True)")
        print("  io_buffer_size: int > 0 (default=65536)")

    @staticmethod
    def validate_parameter(**kwargs: Any) -> None:
        valid_keys = {"progress", "check_signal", "io_buffer_size"}
        for key in kwargs:
            if key not in valid_keys:
                raise ParameterError(f"Unknown parameter: {key}")
        if "progress" in kwargs and not isinstance(kwargs["progress"], bool):
            raise ParameterError("progress must be bool")
        if "check_signal" in kwargs and not isinstance(kwargs["check_signal"], bool):
            raise ParameterError("check_signal must be bool")
        if "io_buffer_size" in kwargs:
            v = kwargs["io_buffer_size"]
            if not isinstance(v, int) or v <= 0:
                raise ParameterError("io_buffer_size must be int > 0")

    def update_parameter(self, **kwargs: Any) -> None:
        try:
            self.validate_parameter(**kwargs)
        except ParameterError:
            self.print_help()
            raise
        for k, v in kwargs.items():
            setattr(self._config, k, v)
        self._initialized = True

    def _core_kwargs(self, desc: str) -> tuple[dict[str, Any], Any]:
        progress_cb, reporter = make_progress_callback(self._config.progress, desc)
        kwargs: dict[str, Any] = {"progress_cb": progress_cb}
        if not self._config.check_signal:
            kwargs["signal_cb"] = lambda: 0
        return kwargs, reporter

    def transform_from_file(self, filepath: str) -> bytes:
        if not self._initialized:
            raise NotInitializedError("BWTTransformer is not initialized")
        kwargs, reporter = self._core_kwargs("transform")
        try:
            return _core.transform_from_file(path=filepath, **kwargs)
        except Exception as exc:  # noqa: BLE001
            logger.exception("transform_from_file failed")
            raise map_core_error(exc, phase="transform") from exc
        finally:
            if reporter:
                reporter.close()

    def transform_from_text(self, text: str) -> bytes:
        if not self._initialized:
            raise NotInitializedError("BWTTransformer is not initialized")
        kwargs, reporter = self._core_kwargs("transform")
        try:
            return _core.transform_from_text(text=text, **kwargs)
        except Exception as exc:  # noqa: BLE001
            logger.exception("transform_from_text failed")
            raise map_core_error(exc, phase="transform") from exc
        finally:
            if reporter:
                reporter.close()

    def inverse_transform_from_file(self, filepath: str) -> str:
        if not self._initialized:
            raise NotInitializedError("BWTTransformer is not initialized")
        kwargs, reporter = self._core_kwargs("inverse")
        try:
            return _core.inverse_from_file(path=filepath, **kwargs)
        except Exception as exc:  # noqa: BLE001
            logger.exception("inverse_transform_from_file failed")
            raise map_core_error(exc, phase="inverse") from exc
        finally:
            if reporter:
                reporter.close()

    def inverse_transform_from_bytes(self, bytess: bytes) -> str:
        if not self._initialized:
            raise NotInitializedError("BWTTransformer is not initialized")
        kwargs, reporter = self._core_kwargs("inverse")
        try:
            return _core.inverse_from_bytes(data=bytess, **kwargs)
        except Exception as exc:  # noqa: BLE001
            logger.exception("inverse_transform_from_bytes failed")
            raise map_core_error(exc, phase="inverse") from exc
        finally:
            if reporter:
                reporter.close()


def _build_searcher_from_kwargs(**kwargs: Any) -> BWTSearcher:
    if not kwargs:
        raise ParameterError("function mode requires non-empty kwargs")
    try:
        return BWTSearcher(**kwargs)
    except Exception as exc:  # noqa: BLE001
        raise exc


def _build_transformer_from_kwargs(**kwargs: Any) -> BWTTransformer:
    if not kwargs:
        raise ParameterError("function mode requires non-empty kwargs")
    try:
        return BWTTransformer(**kwargs)
    except Exception as exc:  # noqa: BLE001
        raise exc


def bwt_search_from_text(text: str, pattern: str, **kwargs: Any) -> int:
    searcher = _build_searcher_from_kwargs(**kwargs)
    searcher.build_index_from_text(text)
    return searcher.search(pattern)


def bwt_search_from_file(filepath: str, pattern: str, **kwargs: Any) -> int:
    searcher = _build_searcher_from_kwargs(**kwargs)
    searcher.build_index_from_file(filepath)
    return searcher.search(pattern)


def bwt_transform_from_text(text: str, **kwargs: Any) -> bytes:
    transformer = _build_transformer_from_kwargs(**kwargs)
    return transformer.transform_from_text(text)


def bwt_transform_from_file(filepath: str, **kwargs: Any) -> bytes:
    transformer = _build_transformer_from_kwargs(**kwargs)
    return transformer.transform_from_file(filepath)


def bwt_inverse_transform_from_bytes(bytess: bytes, **kwargs: Any) -> str:
    transformer = _build_transformer_from_kwargs(**kwargs)
    return transformer.inverse_transform_from_bytes(bytess)


def bwt_inverse_transform_from_file(filepath: str, **kwargs: Any) -> str:
    transformer = _build_transformer_from_kwargs(**kwargs)
    return transformer.inverse_transform_from_file(filepath)
