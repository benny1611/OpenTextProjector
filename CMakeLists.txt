# Bumped to 3.21 to unlock the magical RUNTIME_DEPENDENCIES feature
cmake_minimum_required(VERSION 3.21)
project(SimpleTextProjector CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------
# 1. DEPENDENCY MANAGEMENT
# ---------------------------------------------------------
include(FetchContent)

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

# ---------------------------------------------------------
# DEPENDENCY: Bcrypt.cpp
# ---------------------------------------------------------
FetchContent_Declare(
    bcrypt
    GIT_REPOSITORY https://github.com/hilch/Bcrypt.cpp.git
    GIT_TAG        master
)

# 1. Save your current shared library setting
set(TEMP_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})

# 2. Force Windows to build Bcrypt statically so the .lib file is generated
set(BUILD_SHARED_LIBS OFF)

# 3. Build it
FetchContent_MakeAvailable(bcrypt)

# 4. Restore your original setting so Poco/other libraries aren't affected
set(BUILD_SHARED_LIBS ${TEMP_BUILD_SHARED_LIBS})

# ---------------------------------------------------------
# 2. EXECUTABLE & LINKING
# ---------------------------------------------------------

# 1. Grab BOTH source (.cpp) and header (.h / .hpp) files
file(GLOB_RECURSE SRC_FILES "src/*.cpp" "src/*.h" "src/*.hpp")

# 2. Tell CMake to mirror your exact directory structure in Visual Studio
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${SRC_FILES})

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Poco::Foundation 
    Poco::Net 
    Poco::NetSSL
    Poco::Util 
    Poco::Data 
    Poco::DataSQLite 
    Poco::Crypto
    Poco::JWT 
    Poco::JSON
    bcrypt
)

# ---------------------------------------------------------
# DEVELOPMENT QUALITY OF LIFE (Windows Only)
# ---------------------------------------------------------
# Automatically copy required DLLs to the build folder so you can debug locally
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:${PROJECT_NAME}>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying runtime DLLs to the build directory for local debugging..."
    )
endif()

# ---------------------------------------------------------
# THE OPENSSL DLL FIX (Windows Only)
# ---------------------------------------------------------
if(WIN32)
    # 1. Force CMake to explicitly locate OpenSSL so we get its folder paths
    find_package(OpenSSL REQUIRED)

    # 2. OPENSSL_INCLUDE_DIR is usually something like "C:/Program Files/OpenSSL-Win64/include"
    # We use this command to step one directory up, giving us the root folder.
    get_filename_component(OPENSSL_ROOT_DIR "${OPENSSL_INCLUDE_DIR}" DIRECTORY)
    
    # 3. The actual .dll files live in the 'bin' folder inside that root
    set(OPENSSL_BIN_DIR "${OPENSSL_ROOT_DIR}/bin")

    # 4. Copy the contents of that bin folder directly to where your .exe is built
    if(EXISTS "${OPENSSL_BIN_DIR}")
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            # Using copy_directory grabs all the DLLs at once
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OPENSSL_BIN_DIR}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
            COMMENT "Force-copying OpenSSL DLLs to build directory..."
        )
    endif()
endif()

# Copy properties file for local debugging
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/SimpleTextProjector.properties"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/SimpleTextProjector.properties"
    COMMENT "Copying properties file to build directory..."
)

# ---------------------------------------------------------
# 3. INSTALLATION & BUNDLING DEPENDENCIES (The OpenSSL Fix)
# ---------------------------------------------------------
install(TARGETS ${PROJECT_NAME} 
    DESTINATION bin
    # This tells CMake: "Look at my executable, find any external DLLs 
    # or shared libraries it needs (like OpenSSL), and copy them to the bin folder."
    RUNTIME_DEPENDENCIES 
        DESTINATION bin
)

# Pack the properties file into the installer's bin folder
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/SimpleTextProjector.properties" 
        DESTINATION bin)

# ---------------------------------------------------------
# 4. PACKAGING (CPack / Installers)
# ---------------------------------------------------------
set(CPACK_PACKAGE_NAME "OpenTextProjector")
set(CPACK_PACKAGE_VENDOR "benny1611")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A simple program to show text on screen remotely")
set(CPACK_PACKAGE_VERSION "0.0.1")

# 4a. The License
# Point this to the MIT license file in your project directory
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# 4b. Shortcuts (Start Menu & Desktop)
# Format: "ExecutableTargetName;Display Name For Shortcut"
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};Simple Text Projector")
# Tells the installer to explicitly create a desktop icon for this target
set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}") 

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    # Optional but helpful: Adds your install folder to the Windows PATH
    set(CPACK_NSIS_MODIFY_PATH ON) 
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(UNIX)
    set(CPACK_GENERATOR "DEB;RPM")
endif()

include(CPack)

# ---------------------------------------------------------
# VISUAL STUDIO QUALITY OF LIFE
# ---------------------------------------------------------
# Force the Visual Studio "Play" button to use the folder where 
# the .exe is actually located as the working directory.
if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    )
endif()