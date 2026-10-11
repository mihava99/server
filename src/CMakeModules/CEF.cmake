# The CEF (Chromium Embedded Framework) build that is bundled with CasparCG for the HTML
# producer. Windows always uses it, Linux uses it with -DUSE_SYSTEM_CEF=OFF.
#
# The archives come from CEF's own build CDN. To move to a newer CEF:
#  - Pick a build on https://cef-builds.spotifycdn.com/index.html and change CEF_VERSION.
#  - Download the "minimal" Linux and Windows archives and put their SHA256 below. That page
#    only lists SHA1, so hash the downloads yourself.
#  - Compare the files that Bootstrap_Linux.cmake and Bootstrap_Windows.cmake bundle with the
#    Release and Resources folders of the new archives. CEF adds and removes files between
#    versions.
set(CEF_VERSION "154.0.34+g14c5a08+chromium-154.0.8037.98")
set(CEF_LINUX64_SHA256 "e87e2c76304372dd28b899ceb6efa4b353f52700594ddc358a8237ec07ec6492")
set(CEF_WINDOWS64_SHA256 "c81c34d048d87276f4a18f0b000206710a1709bb67dd658948b87100e1a538da")

set(CEF_DOWNLOAD_BASE_URL "https://cef-builds.spotifycdn.com" CACHE STRING
    "Where to download CEF from, this can be a mirror with the same file names")

# The CDN needs the '+' in the file names to be encoded.
string(REPLACE "+" "%2B" CEF_URL_VERSION "${CEF_VERSION}")
set(CEF_LINUX64_URL "${CEF_DOWNLOAD_BASE_URL}/cef_binary_${CEF_URL_VERSION}_linux64_minimal.tar.bz2")
set(CEF_WINDOWS64_URL "${CEF_DOWNLOAD_BASE_URL}/cef_binary_${CEF_URL_VERSION}_windows64_minimal.tar.bz2")
