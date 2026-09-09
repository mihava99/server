#!/bin/sh
# Builds libomt.so from https://github.com/openmediatransport/libomt on Linux.
#
# Run from the root of a checkout of that repo (ExternalProject_Add's BUILD_IN_SOURCE sets that as
# the working directory automatically when this is invoked from Bootstrap_Linux.cmake).
#
# libomt is a .NET 8 Native AOT project - its own build/buildlinux<arch>.sh scripts (which this
# wraps) are each just `dotnet publish ../libomt.sln -r linux-<rid> -c Release`, run from inside
# build/ per the repo's own README. Native AOT compiles all the way to native code for the target
# runtime identifier, so this picks the script matching the actual build machine's architecture
# (via uname -m) rather than always building x64 - .NET's cross-architecture publish support isn't
# assumed to be set up on the build machine, only native compilation for its own architecture.
#
# dotnet's publish output path follows its own conventions (bin/<Configuration>/
# <TargetFramework>/<RuntimeIdentifier>/publish/), which is why this searches for the resulting
# libomt.so with find rather than hardcoding an exact path - and fails loudly instead of silently
# succeeding with no output if that assumption turns out to be wrong.
set -e

arch="$(uname -m)"
case "$arch" in
    x86_64|amd64)
        build_script=buildlinuxx64.sh
        ;;
    aarch64|arm64)
        build_script=buildlinuxarm64.sh
        ;;
    *)
        echo "omt_build_libomt_linux.sh: unsupported architecture '$arch' - libomt only publishes" >&2
        echo "Linux build scripts for x86_64 (buildlinuxx64.sh) and aarch64 (buildlinuxarm64.sh)." >&2
        exit 1
        ;;
esac

cd build
bash "$build_script"
cd ..

found="$(find bin -name 'libomt.so' 2>/dev/null | head -n 1)"
if [ -z "$found" ]; then
    echo "omt_build_libomt_linux.sh: libomt.so not found anywhere under bin/ after 'dotnet publish'." >&2
    echo "The .NET publish output layout may have changed - inspect the bin/ directory and update this script." >&2
    exit 1
fi

cp "$found" ./libomt.so
