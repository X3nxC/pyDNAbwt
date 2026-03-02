from __future__ import annotations

import pytest

from dnabwt import BWTSearcher, BWTTransformer, bwt_search_from_text, bwt_transform_from_text
from dnabwt.exceptions import NotInitializedError, ParameterError


def test_searcher_update_parameter_unknown_prints_help(capsys) -> None:
    s = BWTSearcher(progress=False, check_signal=False, block_size=128)
    with pytest.raises(ParameterError):
        s.update_parameter(unknown_key=1)
    out = capsys.readouterr().out
    assert "BWTSearcher parameters" in out


def test_transformer_update_parameter_invalid_type_prints_help(capsys) -> None:
    t = BWTTransformer(progress=False, check_signal=False, io_buffer_size=128)
    with pytest.raises(ParameterError):
        t.update_parameter(io_buffer_size=0)
    out = capsys.readouterr().out
    assert "BWTTransformer parameters" in out


def test_not_initialized_objects_reject_operations() -> None:
    s = BWTSearcher()
    t = BWTTransformer()

    with pytest.raises(NotInitializedError):
        s.build_index_from_text("AGCT\n")
    with pytest.raises(NotInitializedError):
        s.search("AG")
    with pytest.raises(NotInitializedError):
        t.transform_from_text("AGCT\n")


def test_function_mode_requires_non_empty_kwargs() -> None:
    with pytest.raises(ParameterError):
        bwt_transform_from_text("AGCT\n")
    with pytest.raises(ParameterError):
        bwt_search_from_text("AGCT\n", "AG")


def test_static_validate_parameter() -> None:
    with pytest.raises(ParameterError):
        BWTSearcher.validate_parameter(block_size=-1)
    with pytest.raises(ParameterError):
        BWTTransformer.validate_parameter(progress="yes")
