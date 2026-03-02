from setuptools import Extension, setup


core_sources = [
    "src/dnabwt/_core/binding.c",
    "src/dnabwt/_core/core_api.c",
    "src/c/bwt/bwt_transform.c",
    "src/c/bwt/bwt_inverse.c",
    "src/c/bwt/bwt_search.c",
    "src/c/code/dna_rle.c",
    "src/c/code/sentinel.c",
    "src/c/io/file_reader.c",
    "src/c/io/file_writer.c",
    "src/c/util/alloc.c",
    "src/c/util/log.c",
    "src/c/util/progress.c",
    "src/c/util/signal.c",
    "src/c/util/status.c",
]

core_extension = Extension(
    "dnabwt._core_native",
    sources=core_sources,
    include_dirs=["src"],
    extra_compile_args=["-std=c99", "-O2"],
)

setup(
    name="pyDNAbwt",
    version="0.1.0",
    ext_modules=[core_extension],
)
