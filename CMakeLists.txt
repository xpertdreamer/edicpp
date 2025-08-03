cmake_minimum_required(VERSION 3.10)

# Project name
project(edi)

# Cpp Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Exec
add_executable(${PROJECT_NAME}
    src/edi.cpp
)

# Include dirs
target_include_directories(${PROJECT_NAME} PRIVATE
    include/
)

set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/build)