project (eventhub)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_C_FLAGS "-Wall -ggdb")
set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")
set(CMAKE_CXX_FLAGS "-Wall -std=c++14 -DSPDLOG_FMT_EXTERNAL")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


# To detect data race with thread sanitizer, uncomment those 2 lines
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
# set(CMAKE_LD_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")

# Projects headers
include_directories ("${PROJECT_SOURCE_DIR}/include")

# Fmt
find_package(fmt REQUIRED)

# Spdlog
find_package(spdlog REQUIRED)

# Threads
find_package(Threads REQUIRED)
include_directories(${Threads_INCLUDE_DIR})

# OpenSSL
#set(OPENSSL_USE_STATIC_LIBS TRUE)
#set(OPENSSL_ROOT_DIR "/usr/lib")
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Hiredis
find_path(HIREDIS_HEADER hiredis)
include_directories(${HIREDIS_HEADER})

# Redis++ https://github.com/sewenew/redis-plus-plus
find_path(REDIS_PLUS_PLUS_HEADER sw)
include_directories(${REDIS_PLUS_PLUS_HEADER})

add_subdirectory(src)

if (NOT DEFINED SKIP_TESTS)
  add_subdirectory(tests)
endif()

file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.hpp *.h)

add_custom_target(
        clangformat
        COMMAND /usr/bin/clang-format
        -style=file
        -i
        ${ALL_SOURCE_FILES}
)
