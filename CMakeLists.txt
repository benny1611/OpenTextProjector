cmake_minimum_required(VERSION 3.21)

# Automatically bootstrap vcpkg if the toolchain file is provided
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

project(OpenTextProjector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# 1. DEPENDENCY MANAGEMENT (FetchContent & System Packages)
# ---------------------------------------------------------
include(FetchContent)

# --- Graphics & Windowing ---
find_package(OpenGL REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# Fetch FreeType (Great for text rendering)
FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        VER-2-13-2
)
FetchContent_MakeAvailable(freetype)

# --- Original Dependencies (Poco & Bcrypt) ---
FetchContent_Declare(
    poco
    GIT_REPOSITORY https://github.com/pocoproject/poco.git
    GIT_TAG        poco-1.13.3-release
)
set(ENABLE_TESTS OFF CACHE BOOL "Disable Poco tests" FORCE)
set(ENABLE_DATA_MYSQL OFF CACHE BOOL "Disable MySQL" FORCE)
set(ENABLE_DATA_ODBC OFF CACHE BOOL "Disable ODBC" FORCE)
set(ENABLE_DATA_POSTGRESQL OFF CACHE BOOL "Disable PostgreSQL" FORCE)
FetchContent_MakeAvailable(poco)

# Fetch Bcrypt.cpp sources but define it manually to avoid name collision with Windows bcrypt.dll
FetchContent_Declare(
    bcrypt_external
    GIT_REPOSITORY https://github.com/hilch/Bcrypt.cpp.git
    GIT_TAG        master
)

FetchContent_GetProperties(bcrypt_external)
if(NOT bcrypt_external_POPULATED)
    FetchContent_Populate(bcrypt_external)
    
    # Collect the source files manually
    file(GLOB BCRYPT_SOURCES 
        "${bcrypt_external_SOURCE_DIR}/src/*.cpp"
        "${bcrypt_external_SOURCE_DIR}/src/*.c"
    )
    
    # Create a target with a safe, non-conflicting name
    add_library(bcrypt_cpp STATIC ${BCRYPT_SOURCES})
    target_include_directories(bcrypt_cpp PUBLIC "${bcrypt_external_SOURCE_DIR}/include")
endif()

# --- WebRTC Streaming & Media ---
# Vcpkg provides a custom module wrapper, allowing a direct native package look up
find_package(FFMPEG REQUIRED)

# Fetch libdatachannel
FetchContent_Declare(
    datachannel
    GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
    GIT_TAG        v0.24.3
)
set(NO_WEBSOCKETS OFF CACHE BOOL "" FORCE)
set(NO_TESTS ON CACHE BOOL "" FORCE)
set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(datachannel)

# =========================================================
# 1.5. LOCAL IMGUI TARGET DEFINITION (OpenGL 2)
# =========================================================
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/imgui")

add_library(imgui STATIC
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp"
)

# PUBLIC includes make sure your main executable automatically 
# finds headers like <imgui.h> when it links against 'imgui'
target_include_directories(imgui PUBLIC 
    "${IMGUI_DIR}"
    "${IMGUI_DIR}/backends"
)

# ImGui backends rely on GLFW and OpenGL functionalities
target_link_libraries(imgui PRIVATE glfw OpenGL::GL)


# ---------------------------------------------------------
# 2. EXECUTABLE & LINKING
# ---------------------------------------------------------
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h" "src/*.hpp")

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SRC_FILES})
add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    # Core backend tools
    Poco::Foundation Poco::Net Poco::NetSSL Poco::Util Poco::Data Poco::DataSQLite Poco::Crypto Poco::JWT Poco::JSON
    bcrypt_cpp
    
    # Graphics & Windowing & Fonts
    OpenGL::GL
    glfw
    freetype
    glad::glad
    imgui
    
    # Media Streaming & WebRTC
    datachannel
    ${FFMPEG_LIBRARIES}
)

# Unified include target for all underlying FFmpeg component headers
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${FFMPEG_INCLUDE_DIRS}
)

# ---------------------------------------------------------
# DEVELOPMENT QUALITY OF LIFE & PACKAGING
# ---------------------------------------------------------
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs to the build directory for local debugging..."
    )
    
    find_package(OpenSSL REQUIRED)
    get_filename_component(OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    set(OPENSSL_BIN_DIR "${OPENSSL_ROOT_DIR}/bin")
    if(EXISTS "${OPENSSL_BIN_DIR}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OPENSSL_BIN_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Force-copying OpenSSL DLLs to build directory..."
        )
    endif()
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/OpenTextProjector.properties"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/OpenTextProjector.properties"
    COMMENT "Copying properties file to build directory..."
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/resources"
    COMMENT "Copying resources folder to build directory..."
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin RUNTIME_DEPENDENCIES DESTINATION bin)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/OpenTextProjector.properties" DESTINATION bin)
install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources" DESTINATION bin)

# CPack Configurations
set(CPACK_PACKAGE_NAME "OpenTextProjector")
set(CPACK_PACKAGE_VENDOR "benny1611")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple program to show text on screen remotely")
set(CPACK_PACKAGE_VERSION "0.0.1")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};Simple Text Projector")
set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}") 

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_MODIFY_PATH ON) 
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM")
endif()

include(CPack)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()