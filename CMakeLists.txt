
cmake_minimum_required(VERSION 3.16)

project(CPP_HTTP_SERVER
    VERSION 1.0
    LANGUAGES CXX
)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header files
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
file(GLOB SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

# Server executable
add_executable(server ${SOURCES})

# Threading support
find_package(Threads REQUIRED)

# libcurl for HTTP requests to the local AI inference server
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBCURL REQUIRED IMPORTED_TARGET libcurl)

# Link dependencies
target_link_libraries(server PRIVATE
    Threads::Threads
    PkgConfig::LIBCURL
)
