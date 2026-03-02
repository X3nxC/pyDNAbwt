from __future__ import annotations

import pytest

import dnabwt._core_native as core


def test_signal_callback_interrupts_transform() -> None:
    text = "AGCT" * 1024 + "\n"

    with pytest.raises(core.InterruptedError):
        core.transform_from_text(
            text=text,
            progress_cb=None,
            signal_cb=lambda: 1,
        )


def test_signal_callback_interrupts_inverse() -> None:
    text = "AGCTAGCTAGCT\n"
    encoded = core.transform_from_text(text=text, progress_cb=None, signal_cb=lambda: 0)

    with pytest.raises(core.InterruptedError):
        core.inverse_from_bytes(
            data=encoded,
            progress_cb=None,
            signal_cb=lambda: 1,
        )


def test_signal_callback_interrupts_search_bytes_mode() -> None:
    text = "AGCT" * 2048 + "\n"
    encoded = core.transform_from_text(text=text, progress_cb=None, signal_cb=lambda: 0)

    with pytest.raises(core.InterruptedError):
        core.search_count(
            data=encoded,
            pattern="AGC",
            progress_cb=None,
            signal_cb=lambda: 1,
        )


def test_signal_callback_interrupts_search_index_mode(tmp_path) -> None:
    text = "AGCT" * 1024 + "\n"
    encoded = core.transform_from_text(text=text, progress_cb=None, signal_cb=lambda: 0)
    p = tmp_path / "idx.rbwt"
    p.write_bytes(encoded)

    index = core.build_search_index_from_encoded_file(path=str(p), block_size=128, cache_count=2)
    with pytest.raises(core.InterruptedError):
        core.search_with_index(
            index=index,
            pattern="AGC",
            progress_cb=None,
            signal_cb=lambda: 1,
        )
