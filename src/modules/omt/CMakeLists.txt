cmake_minimum_required(VERSION 3.28)
project(omt)

set(SOURCES
    omt.cpp
    omt.h

    consumer/omt_consumer.cpp
    consumer/omt_consumer.h

    producer/omt_producer.cpp
    producer/omt_producer.h

    util/omt_util.cpp
    util/omt_util.h

    interop/libomt.h
)

casparcg_add_module_project(omt
    SOURCES ${SOURCES}
    INIT_FUNCTION "omt::init"
)

target_include_directories(omt PRIVATE
    ..
    ${FFMPEG_INCLUDE_PATH}
)

target_link_libraries(omt PRIVATE
    core
    common
    ffmpeg
)

set_target_properties(omt PROPERTIES FOLDER modules)
source_group(sources\\consumer consumer/*)
source_group(sources\\producer producer/*)
source_group(sources\\util util/*)
source_group(sources\\interop interop/*)
source_group(sources ./*)

# The libomt/libvmx runtime is loaded dynamically at process runtime (see util/omt_util.cpp), so
# it isn't required at build/link time on any platform. On Windows, OMT_SDK_DIR is fetched
# automatically from GitHub by Bootstrap_Windows.cmake (or points at a developer-supplied local
# checkout, if OMT_SDK_DIR was set before configuring) - copy its prebuilt DLLs next to the server
# executable so a normal build can send/receive OMT out of the box.
if (MSVC AND DEFINED OMT_SDK_DIR)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        set(OMT_LIB_DIR "${OMT_SDK_DIR}/Libraries/Winarm64")
    else()
        set(OMT_LIB_DIR "${OMT_SDK_DIR}/Libraries/Winx64")
    endif()

    # OMT_SDK_FETCHED means Bootstrap_Windows.cmake just declared this as an ExternalProject: its
    # files only appear on disk once that project's build step actually runs, so EXISTS here (a
    # configure-time check) would wrongly skip bundling on a fresh checkout's first configure -
    # trust the build-order dependency instead. A developer-supplied OMT_SDK_DIR has no such
    # project backing it, so it keeps the defensive EXISTS check.
    if (OMT_SDK_FETCHED OR EXISTS "${OMT_LIB_DIR}/libomt.dll")
        casparcg_add_runtime_dependency("${OMT_LIB_DIR}/libomt.dll")
    endif()
    if (OMT_SDK_FETCHED OR EXISTS "${OMT_LIB_DIR}/libvmx.dll")
        casparcg_add_runtime_dependency("${OMT_LIB_DIR}/libvmx.dll")
    endif()
endif()
