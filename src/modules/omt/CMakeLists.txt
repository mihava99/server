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

# The libomt/libvmx runtime is loaded dynamically at process runtime (see util/omt_util.cpp),
# so it isn't required at build/link time on any platform. On a Windows dev machine that has a
# local checkout of https://github.com/OpenMediaTransport/OpenMediaTransport, optionally copy its
# prebuilt DLLs next to the server executable purely for local testing convenience.
if (MSVC)
    if (NOT DEFINED OMT_SDK_DIR)
        set(OMT_SDK_DIR "D:/Github/OpenMediaTransport")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        set(OMT_LIB_DIR "${OMT_SDK_DIR}/Libraries/Winarm64")
    else()
        set(OMT_LIB_DIR "${OMT_SDK_DIR}/Libraries/Winx64")
    endif()

    if (EXISTS "${OMT_LIB_DIR}/libomt.dll")
        casparcg_add_runtime_dependency("${OMT_LIB_DIR}/libomt.dll")
    endif()
    if (EXISTS "${OMT_LIB_DIR}/libvmx.dll")
        casparcg_add_runtime_dependency("${OMT_LIB_DIR}/libvmx.dll")
    endif()
endif()
