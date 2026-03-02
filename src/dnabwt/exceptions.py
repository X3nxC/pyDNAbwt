"""dnabwt public exceptions."""

from __future__ import annotations


class DNABWTError(Exception):
    """Base exception for dnabwt."""


class ParameterError(DNABWTError, ValueError):
    """Invalid user parameter."""


class NotInitializedError(DNABWTError):
    """Operation requires initialized object parameters."""


class BuildIndexError(DNABWTError):
    """Index building failed."""


class TransformError(DNABWTError):
    """BWT transform/inverse failed."""


class SearchError(DNABWTError):
    """BWT search failed."""


class InterruptedError(DNABWTError, KeyboardInterrupt):
    """Operation interrupted by signal or callback."""


def map_core_error(exc: BaseException, *, phase: str) -> DNABWTError:
    """Map extension errors into stable Python-level exceptions."""
    name = exc.__class__.__name__
    msg = str(exc)

    if name == "InterruptedError":
        return InterruptedError(msg)
    if isinstance(exc, ValueError):
        return ParameterError(msg)
    if phase == "search":
        return SearchError(msg)
    if phase == "build_index":
        return BuildIndexError(msg)
    return TransformError(msg)
