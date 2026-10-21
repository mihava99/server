# CasparCG Server for Fedora.
#
# Build it with tools/linux/rpm/build-rpm, which also installs the build dependencies, creates
# the source tarball from the current commit and passes the version macros defined below.
#
# The HTML producer needs CEF, which is not available as a Fedora package that matches what
# CasparCG is tested with, so the build downloads it (see CEF.cmake in src/CMakeModules). It
# therefore needs network access while it runs, which rules out Koji and a default Copr, but
# works with rpmbuild and with mock --enable-network.
#
# OMT is not part of this package. The server loads libomt when OMT is used, and it comes in the
# libomt package (libomt.spec), which is optional: without it the server works and OMT is not
# available.
#
# Use "rpmbuild --without html" to leave out the HTML producer.

%bcond html 1

# Before the macros below, which are expanded as they are defined and use the name.
Name:           casparcg-server

# build-rpm defines these from src/CMakeLists.txt and git. The defaults let the spec be read
# without it, for example by "dnf builddep".
%{!?casparcg_version: %global casparcg_version 2.6.0}
%{!?casparcg_commit: %global casparcg_commit unknown}

# CEF is installed here, away from the libraries of the system.
%global private_libdir %{_libdir}/%{name}

# There is no useful debuginfo for the bundled binaries, and dwz cannot read the DWARF 5 sections
# of the prebuilt CEF. The packaged files are still stripped.
%global debug_package %{nil}

# Link time optimization makes the link of the server very slow and gains nothing here.
%global _lto_cflags %{nil}

# The CMake project is in the src directory.
%global _vpath_srcdir src

# The bundled CEF libraries are private to this package. They must not become capabilities of
# the system (CEF brings its own libvulkan), and the server must not require them from another
# package.
%global __provides_exclude_from ^%{private_libdir}/.*$
%global __requires_exclude ^(libcef|libvk_swiftshader|libvulkan)[.]so

Version:        %{casparcg_version}
Release:        0.1.git%{casparcg_commit}%{?dist}
Summary:        Professional video and graphics playout server

# CEF is BSD-3-Clause (plus the licenses of Chromium's own dependencies).
License:        GPL-3.0-or-later AND BSD-3-Clause
URL:            https://github.com/CasparCG/server
Source0:        %{name}-%{version}.tar.gz

# CEF.cmake only knows the x86_64 build of CEF.
ExclusiveArch:  x86_64

BuildRequires:  gcc-c++
BuildRequires:  cmake
BuildRequires:  ninja-build
BuildRequires:  pkgconf-pkg-config
BuildRequires:  git-core
BuildRequires:  boost-devel
BuildRequires:  libicu-devel
BuildRequires:  zlib-devel
BuildRequires:  pkgconfig(egl)
BuildRequires:  pkgconfig(gl)
BuildRequires:  pkgconfig(glew)
BuildRequires:  pkgconfig(libavcodec)
BuildRequires:  pkgconfig(libavdevice)
BuildRequires:  pkgconfig(libavfilter)
BuildRequires:  pkgconfig(libavformat)
BuildRequires:  pkgconfig(libavutil)
BuildRequires:  pkgconfig(libswresample)
BuildRequires:  pkgconfig(libswscale)
BuildRequires:  pkgconfig(openal)
BuildRequires:  pkgconfig(sfml-graphics)
BuildRequires:  pkgconfig(tbb)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xrandr)
BuildRequires:  liberation-mono-fonts
%if %{with html}
# The server links against libcef.so, and the linker wants the libraries that it needs itself.
BuildRequires:  cups-devel
BuildRequires:  nss-devel
BuildRequires:  pkgconfig(atk)
BuildRequires:  pkgconfig(atk-bridge-2.0)
BuildRequires:  pkgconfig(atspi-2)
BuildRequires:  pkgconfig(xcomposite)
BuildRequires:  pkgconfig(xdamage)
%endif

Requires:       liberation-mono-fonts
%if %{with html}
# CEF loads the NSS libraries (libsoftokn3.so, libnssckbi.so) when it starts, so rpm cannot see them.
Requires:       nss
%endif

# The server loads libomt when OMT is used. It is not needed to run the server.
Suggests:       libomt

%description
CasparCG Server plays out and records professional graphics, audio and video to
multiple outputs. It is controlled over the network with the AMCP protocol and has
been used in 24/7 broadcast production since 2006.

The server needs a graphics card that supports OpenGL 4.5 and a running X server.
Start it with a configuration file, for example a copy of
%{_datadir}/%{name}/casparcg.config.

OMT (Open Media Transport) is enabled by installing the libomt package.

The FFmpeg of Fedora (ffmpeg-free) cannot decode H.264 and HEVC. To play those, replace
it with the FFmpeg of RPM Fusion, which has the same library versions.

%prep
%autosetup

%build
export GIT_HASH=%{casparcg_commit}
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_NOLOGO=1

# The font is loaded by its path at runtime.
diag_font=$(rpm -ql liberation-mono-fonts | grep -m1 '/LiberationMono-Regular[.]ttf$')
test -n "$diag_font"

# %%cmake builds shared libraries by default, and does not know the rest.
%cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCASPARCG_BINARY_NAME=%{name} \
    -DDIAG_FONT_PATH="$diag_font" \
    -DENABLE_WERROR=OFF \
    -DUSE_SYSTEM_CEF=OFF \
    -DOMT_BUILD_FROM_SOURCE=OFF \
    %{!?with_html:-DENABLE_HTML=OFF}
%cmake_build

%install
%cmake_install

# CMake installs the bundled libraries without the executable bit, and rpm only looks for the
# dependencies of executable files.
find %{buildroot}%{private_libdir} -type f \( -name '*.so' -o -name '*.so.*' \) -exec chmod 0755 {} +

# CasparCG runs CEF without its sandbox, and a copy that is not setuid root does nothing.
rm -f %{buildroot}%{private_libdir}/chrome-sandbox

%check
# The server finds the bundled libraries through its rpath, so they have to be where it points to.
readelf -d %{buildroot}%{_bindir}/%{name} | grep -F '%{private_libdir}'
%if %{with html}
test -f %{buildroot}%{private_libdir}/libcef.so
%endif

%files
%license LICENSE
%doc README.md CHANGELOG.md
%{_bindir}/%{name}
%{_datadir}/%{name}/
%{private_libdir}/

%changelog
* Sun Oct 11 2026 Mihai <insider.vasiloiu5@gmail.com> - 2.6.0-0.1
- First Fedora package
