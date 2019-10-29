project (eventhub)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_C_FLAGS "-Wall -ggdb")
set(CMAKE_CXX_FLAGS "-std=c++14")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# Projects headers
include_directories ("${PROJECT_SOURCE_DIR}/include")

# Glog
find_package(Glog REQUIRED)
include_directories(${Glog_INCLUDE_PATH})

# Threads
find_package(Threads REQUIRED)
include_directories(${Threads_INCLUDE_DIR})

# OpenSSL
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Hiredis
find_path(HIREDIS_HEADER hiredis)
include_directories(${HIREDIS_HEADER})

# Redis++ https://github.com/sewenew/redis-plus-plus
find_path(REDIS_PLUS_PLUS_HEADER sw)
include_directories(${REDIS_PLUS_PLUS_HEADER})

add_subdirectory(src)
add_subdirectory(tests)


file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.hpp *.h)

add_custom_target(
        clangformat
        COMMAND /usr/bin/clang-format
        -style=file
        -i
        ${ALL_SOURCE_FILES}
)
