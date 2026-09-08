#!/bin/sh
# Builds libomt.so from https://github.com/openmediatransport/libomt on Linux.
#
# Run from the root of a checkout of that repo (ExternalProject_Add's BUILD_IN_SOURCE sets that as
# the working directory automatically when this is invoked from Bootstrap_Linux.cmake).
#
# libomt is a .NET 8 Native AOT project - its own build/buildlinuxx64.sh (which this wraps) is just
# `dotnet publish ../libomt.sln -r linux-x64 -c Release`, run from inside build/ per the repo's own
# README. dotnet's publish output path follows its own conventions (bin/<Configuration>/
# <TargetFramework>/<RuntimeIdentifier>/publish/), which is why this searches for the resulting
# libomt.so with find rather than hardcoding an exact path - and fails loudly instead of silently
# succeeding with no output if that assumption turns out to be wrong.
set -e

cd build
bash buildlinuxx64.sh
cd ..

found="$(find bin -name 'libomt.so' 2>/dev/null | head -n 1)"
if [ -z "$found" ]; then
    echo "omt_build_libomt_linux.sh: libomt.so not found anywhere under bin/ after 'dotnet publish'." >&2
    echo "The .NET publish output layout may have changed - inspect the bin/ directory and update this script." >&2
    exit 1
fi

cp "$found" ./libomt.so
