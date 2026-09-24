cmake_minimum_required(VERSION 3.13)
project (eventhub)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

OPTION(SKIP_TESTS "Skip building tests" 0)
set(EVENTHUB_SANITIZER "" CACHE STRING "Sanitizer: address or thread")
set_property(CACHE EVENTHUB_SANITIZER PROPERTY STRINGS "" address thread)

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
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})

# Hiredis
find_path(HIREDIS_HEADER hiredis)
include_directories(${HIREDIS_HEADER})

# Redis++ https://github.com/sewenew/redis-plus-plus
find_path(REDIS_PLUS_PLUS_HEADER sw)
include_directories(${REDIS_PLUS_PLUS_HEADER})

add_subdirectory(src)

if (NOT ${SKIP_TESTS})
        add_subdirectory(tests)
        include(CTest)
        add_test(eventhub_tests eventhub_tests)
endif()

set(EVENTHUB_TARGETS eventhub_core eventhub)
if (NOT ${SKIP_TESTS})
        list(APPEND EVENTHUB_TARGETS eventhub_tests)
endif()

foreach(EVENTHUB_TARGET IN LISTS EVENTHUB_TARGETS)
        target_compile_features(${EVENTHUB_TARGET} PRIVATE cxx_std_17)
        target_compile_options(${EVENTHUB_TARGET} PRIVATE
                -Wall
                $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>)
        target_compile_definitions(${EVENTHUB_TARGET} PRIVATE SPDLOG_FMT_EXTERNAL=ON)

        if (EVENTHUB_SANITIZER STREQUAL "address")
                target_compile_options(${EVENTHUB_TARGET} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options(${EVENTHUB_TARGET} PRIVATE -fsanitize=address,undefined)
        elseif (EVENTHUB_SANITIZER STREQUAL "thread")
                target_compile_options(${EVENTHUB_TARGET} PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
                target_link_options(${EVENTHUB_TARGET} PRIVATE -fsanitize=thread)
        elseif (NOT EVENTHUB_SANITIZER STREQUAL "")
                message(FATAL_ERROR "EVENTHUB_SANITIZER must be empty, address, or thread")
        endif()
endforeach()

file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.hpp *.h)

add_custom_target(
        clangformat
        COMMAND /usr/bin/clang-format
        -style=file
        -i
        ${ALL_SOURCE_FILES}
)
