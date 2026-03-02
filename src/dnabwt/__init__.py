"""dnabwt package."""

from .api import (
    BWTSearcher,
    BWTTransformer,
    bwt_inverse_transform_from_bytes,
    bwt_inverse_transform_from_file,
    bwt_search_from_file,
    bwt_search_from_text,
    bwt_transform_from_file,
    bwt_transform_from_text,
)
from .exceptions import (
    BuildIndexError,
    DNABWTError,
    InterruptedError,
    NotInitializedError,
    ParameterError,
    SearchError,
    TransformError,
)
from .logging import configure_logger, logger

__all__ = [
    "BWTSearcher",
    "BWTTransformer",
    "bwt_search_from_text",
    "bwt_search_from_file",
    "bwt_transform_from_text",
    "bwt_transform_from_file",
    "bwt_inverse_transform_from_bytes",
    "bwt_inverse_transform_from_file",
    "DNABWTError",
    "ParameterError",
    "NotInitializedError",
    "BuildIndexError",
    "TransformError",
    "SearchError",
    "InterruptedError",
    "configure_logger",
    "logger",
]
