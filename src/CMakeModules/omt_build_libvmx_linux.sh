#!/bin/sh
# Builds libvmx.so from https://github.com/openmediatransport/libvmx on Linux.
#
# Run from the root of a checkout of that repo (ExternalProject_Add's BUILD_IN_SOURCE sets that as
# the working directory automatically when this is invoked from Bootstrap_Linux.cmake).
#
# libvmx's own build/buildlinux<arch>.sh scripts (which this wraps) are each a single clang++
# invocation with a fixed output path (build/libvmx.so) - this picks the one matching the actual
# build machine's architecture (via uname -m) rather than always building x86_64. This isn't just a
# different set of compiler flags: the x64 script uses x86-specific instructions (-mbmi/-mavx2) and
# compiles vmxcodec_x86.cpp/vmxcodec_avx2.cpp, while the arm64 script compiles vmxcodec_arm.cpp (a
# NEON-based implementation) instead - genuinely different source files per architecture.
set -e

cd build

arch="$(uname -m)"
case "$arch" in
    x86_64|amd64)
        bash buildlinuxx64.sh
        ;;
    aarch64|arm64)
        bash buildlinuxarm64.sh
        ;;
    *)
        echo "omt_build_libvmx_linux.sh: unsupported architecture '$arch' - libvmx only publishes" >&2
        echo "Linux build scripts for x86_64 (buildlinuxx64.sh) and aarch64 (buildlinuxarm64.sh)." >&2
        exit 1
        ;;
esac
