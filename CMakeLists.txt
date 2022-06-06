cmake_minimum_required(VERSION 3.21)
project(OpenTextProjector)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)
include_directories(includes/groupsock/include)
include_directories(includes/UsageEnvironment/include)
include_directories(includes)
include_directories(libraries)
include_directories(tools)

add_link_options(-m64
        -static-libstdc++
        -static-libgcc
        -ggdb)

add_executable(OpenTextProjector
        main.cpp
        main.h
        tools/tinythread.cpp
        tools/tinythread.h
        tools/base64.cpp
        tools/base64.h
        tools/concurrent_queue.hpp
        tools/encoder.hpp
        tools/ScreenSource.hpp
        tools/RTSPServer.hpp)

target_include_directories(${PROJECT_NAME} PUBLIC includes)

target_link_libraries(${PROJECT_NAME}
        ${CMAKE_SOURCE_DIR}/libraries/libglfw3.a
        ${CMAKE_SOURCE_DIR}/libraries/libglfw3dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libopengl32.a
        ${CMAKE_SOURCE_DIR}/libraries/libglew32.a
        ${CMAKE_SOURCE_DIR}/libraries/libglew32.dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libswscale.dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libavutil.dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libavcodec.dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libfreetype.dll.a
        ${CMAKE_SOURCE_DIR}/libraries/libfreetype.a
        ${CMAKE_SOURCE_DIR}/libraries/libliveMedia.a
        ${CMAKE_SOURCE_DIR}/libraries/libgroupsock.a
        ${CMAKE_SOURCE_DIR}/libraries/libBasicUsageEnvironment.a
        ${CMAKE_SOURCE_DIR}/libraries/libUsageEnvironment.a
        ${CMAKE_SOURCE_DIR}/libraries/libx264.a
        ${CMAKE_SOURCE_DIR}/libraries/libx264.dll.a
        opengl32
        glu32
        winmm
        gdi32
        ws2_32
        ssl
        crypto)
