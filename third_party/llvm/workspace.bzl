"""Provides the repository macro to import LLVM."""

load("//third_party:repo.bzl", "tfrt_http_archive")

def repo(name):
    """Imports LLVM."""
    LLVM_COMMIT = "d82f2a123f9c443911fc40009d2017915b850758"
    LLVM_SHA256 = "9140b422c7355f6defd73f99e436afd6bc278684a2697dfce63631768a014106"

    tfrt_http_archive(
        name = name,
        sha256 = LLVM_SHA256,
        strip_prefix = "llvm-project-" + LLVM_COMMIT,
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
            "https://github.com/llvm/llvm-project/archive/{commit}.tar.gz".format(commit = LLVM_COMMIT),
        ],
        link_files = {
            "//third_party/llvm:llvm.autogenerated.BUILD": "llvm/BUILD",
            "//third_party/mlir:BUILD": "mlir/BUILD",
            "//third_party/mlir:test.BUILD": "mlir/test/BUILD",
        },
    )
