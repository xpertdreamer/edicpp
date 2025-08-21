cmake_minimum_required(VERSION 3.10)
project(edi LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(SOURCES
    src/edi.cpp
    src/term.cpp
)

set(VERSION_HEADER ${CMAKE_BINARY_DIR}/generated/version.hpp)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.hpp.in
    ${VERSION_HEADER}
    @ONLY
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_BINARY_DIR}/generated
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
