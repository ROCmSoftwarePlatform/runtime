"""Provides the repository macro to import LLVM."""

load("//third_party:repo.bzl", "tfrt_http_archive")

def repo(name):
    """Imports LLVM."""
    LLVM_COMMIT = "73710858b041ee129a38f6ccc740e2691d2a93c0"
    LLVM_SHA256 = "970b12fdf11fdb6868aa1e79cd6ea3f44bef17a5cc1e19303e42d4bcf70aa8ba"

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
            "//third_party/mlir:build_defs.bzl": "mlir/build_defs.bzl",
            "//third_party/mlir:linalggen.bzl": "mlir/linalggen.bzl",
            "//third_party/mlir:tblgen.bzl": "mlir/tblgen.bzl",
            "//third_party/mlir:test.BUILD": "mlir/test/BUILD",
        },
    )
