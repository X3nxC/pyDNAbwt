from __future__ import annotations

from dnabwt import (
    BWTSearcher,
    BWTTransformer,
    bwt_inverse_transform_from_bytes,
    bwt_search_from_file,
    bwt_search_from_text,
    bwt_transform_from_file,
    bwt_transform_from_text,
)


def test_transform_function_mode_roundtrip() -> None:
    text = "AGCTAGCTA\n"
    encoded = bwt_transform_from_text(text, progress=False, check_signal=False, io_buffer_size=4096)
    decoded = bwt_inverse_transform_from_bytes(encoded, progress=False, check_signal=False, io_buffer_size=4096)
    assert decoded == text


def test_transform_oop_mode_roundtrip() -> None:
    text = "TTTTAGCA\n"
    transformer = BWTTransformer(progress=False, check_signal=False, io_buffer_size=8192)
    encoded = transformer.transform_from_text(text)
    assert transformer.inverse_transform_from_bytes(encoded) == text


def test_search_function_mode_text_and_file(tmp_path) -> None:
    text = "AGCTAGCTA\n"
    encoded = bwt_transform_from_text(text, progress=False, check_signal=False, io_buffer_size=1024)
    encoded_path = tmp_path / "small_dna.rbwt"
    encoded_path.write_bytes(encoded)

    assert bwt_search_from_text(text, "AG", progress=False, check_signal=False, block_size=128) == 2
    assert bwt_search_from_file(str(encoded_path), "CTA", progress=False, check_signal=False, block_size=128) == 2


def test_search_oop_mode_persistent_index() -> None:
    searcher = BWTSearcher(progress=False, check_signal=False, block_size=128)
    searcher.build_index_from_text("AGCTAGCTA\n")
    assert searcher.search("AG") == 2
    assert searcher.search("CTA") == 2


def test_transform_function_mode_file(tmp_path) -> None:
    text = "AACCGGTT\n"
    p = tmp_path / "dna.txt"
    p.write_text(text, encoding="ascii")

    encoded = bwt_transform_from_file(str(p), progress=False, check_signal=False, io_buffer_size=2048)
    decoded = bwt_inverse_transform_from_bytes(encoded, progress=False, check_signal=False, io_buffer_size=2048)
    assert decoded == text


def test_search_build_index_from_encoded_file(tmp_path) -> None:
    text = "AACCGGTTAACCGGTT\n"
    encoded = bwt_transform_from_text(text, progress=False, check_signal=False, io_buffer_size=1024)
    encoded_path = tmp_path / "idx.rbwt"
    encoded_path.write_bytes(encoded)

    s = BWTSearcher(progress=False, check_signal=False, block_size=64)
    s.build_index_from_file(str(encoded_path))
    assert s.search("AAC") == 2
