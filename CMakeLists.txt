cmake_minimum_required(VERSION 3.10)


project(CPP_HTTP_SERVER)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

file(GLOB SOURCES

    src/*.cpp

)

add_executable(

    server

    ${SOURCES}

)



find_package(Threads REQUIRED)


target_link_libraries(

    server

    Threads::Threads

)