cmake_minimum_required(VERSION 3.10)
project(edi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(SOURCES
    src/edi.cpp
    src/term.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(edi PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)
