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
set(CEF_VERSION "156.0.14+g4fa4ab5+chromium-156.0.8078.48")
set(CEF_LINUX64_SHA256 "4191ac920a91a052eba04afb6ed830d47aed46a3e246c434a2f062c23588dc8a")
set(CEF_WINDOWS64_SHA256 "99f8a6dffd47417e0caee7845a98c549b41f1d4246cd715dec843ca4b699ee57")



set(CEF_DOWNLOAD_BASE_URL "https://cef-builds.spotifycdn.com" CACHE STRING
    "Where to download CEF from, this can be a mirror with the same file names")

# The CDN needs the '+' in the file names to be encoded.
string(REPLACE "+" "%2B" CEF_URL_VERSION "${CEF_VERSION}")
set(CEF_LINUX64_URL "${CEF_DOWNLOAD_BASE_URL}/cef_binary_${CEF_URL_VERSION}_linux64_minimal.tar.bz2")
set(CEF_WINDOWS64_URL "${CEF_DOWNLOAD_BASE_URL}/cef_binary_${CEF_URL_VERSION}_windows64_minimal.tar.bz2")
