# libomt for CasparCG Server: the Open Media Transport runtime.
#
# CasparCG Server uses libomt for its OMT consumer and producer. It loads the library when OMT is
# first used, so the server works without it and OMT is simply not available until this package
# is installed.
#
# libomt (a .NET Native AOT library) and libvmx (a native video codec) are built from the upstream
# sources at the revisions that are pinned in src/CMakeModules/OMT.cmake. The build clones them
# from GitHub and restores NuGet packages, so it needs network access while it runs. That rules
# out Koji and a default Copr, but works with rpmbuild and with mock --enable-network.
#
# Build it with tools/linux/rpm/build-rpm --package libomt, which also installs the build
# dependencies and creates the source tarball from the current commit.

Name:           libomt
# The Version of libomt.csproj at the pinned revision. The build stops if they differ, so that
# changing the revision in OMT.cmake cannot be forgotten here.
Version:        1.0.0.19
Release:        1%{?dist}
Summary:        Open Media Transport runtime library, for CasparCG Server

# libomt, libomtnet (compiled into libomt) and libvmx are all MIT licensed.
License:        MIT
URL:            https://github.com/openmediatransport
Source0:        %{name}-%{version}.tar.gz

# OMT.cmake builds for these two.
ExclusiveArch:  x86_64 aarch64

BuildRequires:  cmake
BuildRequires:  make
BuildRequires:  git-core
BuildRequires:  clang
# Any .NET SDK from 8.0 on can build it. Which one a distribution has differs.
BuildRequires:  (dotnet-sdk-8.0 or dotnet-sdk-9.0 or dotnet-sdk-10.0)
# Native AOT links against these when the SDK is the one of the distribution.
BuildRequires:  zlib-devel
BuildRequires:  openssl-devel
BuildRequires:  brotli-devel

# libomt loads ICU and Avahi when it starts, so rpm cannot see them.
Requires:       libicu
Requires:       avahi-libs
Provides:       bundled(libomtnet)
Provides:       bundled(libvmx)

# There is no useful debuginfo for a Native AOT library.
%global debug_package %{nil}

# The CMake project is in tools/linux/libomt.
%global _vpath_srcdir tools/linux/libomt

%description
libomt is the C library of Open Media Transport (OMT), a protocol for sending video and
audio with low latency over a local network, and libvmx is the video codec that it uses.

CasparCG Server loads libomt for its OMT consumer and producer. Install this package to
enable OMT in the server, no rebuild of the server is needed.

%prep
%autosetup

%build
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1

%cmake -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
%cmake_build

# The revision of libomt that OMT.cmake pins must be the version that this package is named after.
grep -F '<Version>%{version}</Version>' %{_vpath_builddir}/omt-src/libomt/libomt.csproj

%install
%cmake_install

# CMake installs the libraries without the executable bit, and rpm only looks for the
# dependencies of executable files.
chmod 0755 %{buildroot}%{_libdir}/libomt.so %{buildroot}%{_libdir}/libvmx.so

# The MIT license of the three projects asks for its notice to come with the libraries.
for library in libvmx libomtnet libomt; do
    install -Dpm 0644 %{_vpath_builddir}/omt-src/$library/LICENSE.txt \
        %{buildroot}%{_licensedir}/%{name}/$library.txt
done

%check
# These are the functions that the server looks up when it loads the library.
for function in omt_discovery_getaddresses omt_receive_create omt_receive_destroy omt_receive \
                omt_send_create omt_send_destroy omt_send omt_send_getaddress omt_send_connections; do
    nm -D --defined-only %{buildroot}%{_libdir}/libomt.so | grep -qw "$function"
done

%files
%license %{_licensedir}/%{name}/libvmx.txt
%license %{_licensedir}/%{name}/libomtnet.txt
%license %{_licensedir}/%{name}/libomt.txt
%{_libdir}/libomt.so
%{_libdir}/libvmx.so

%changelog
* Tue Oct 20 2026 Mihai <insider.vasiloiu5@gmail.com> - 1.0.0.19-1
- First package, for CasparCG Server
