from __future__ import annotations

import pytest

from dnabwt import BWTSearcher, BWTTransformer
from dnabwt import api as api_module
from dnabwt.exceptions import BuildIndexError, InterruptedError, ParameterError, SearchError, TransformError


def test_core_value_error_maps_to_parameter_error() -> None:
    t = BWTTransformer(progress=False, check_signal=False, io_buffer_size=512)
    with pytest.raises(ParameterError):
        t.transform_from_text("AGXT\n")


def test_build_index_runtime_error_maps_to_build_index_error(monkeypatch) -> None:
    s = BWTSearcher(progress=False, check_signal=False, block_size=64)

    def _boom(**kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(api_module._core, "build_search_index_from_encoded_file", _boom)
    with pytest.raises(BuildIndexError):
        s.build_index_from_text("AGCT\n")


def test_search_runtime_error_maps_to_search_error(monkeypatch) -> None:
    s = BWTSearcher(progress=False, check_signal=False, block_size=64)
    s.build_index_from_text("AGCTAGCT\n")

    def _boom(**kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(api_module._core, "search_with_index", _boom)
    with pytest.raises(SearchError):
        s.search("AG")


def test_transform_runtime_error_maps_to_transform_error(monkeypatch) -> None:
    t = BWTTransformer(progress=False, check_signal=False, io_buffer_size=64)

    def _boom(**kwargs):
        raise RuntimeError("boom")

    monkeypatch.setattr(api_module._core, "transform_from_text", _boom)
    with pytest.raises(TransformError):
        t.transform_from_text("AGCT\n")


def test_core_interrupted_error_maps_to_python_interrupted(monkeypatch) -> None:
    t = BWTTransformer(progress=False, check_signal=False, io_buffer_size=64)

    def _boom(**kwargs):
        raise api_module._core.InterruptedError("interrupted")

    monkeypatch.setattr(api_module._core, "transform_from_text", _boom)
    with pytest.raises(InterruptedError):
        t.transform_from_text("AGCT\n")
