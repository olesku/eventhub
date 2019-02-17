project (eventhub_testsuite)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_C_FLAGS "-Wall -ggdb")
set(CMAKE_CXX_FLAGS "-std=c++11")

add_executable( websocket_response_test
  ../websocket_response.cpp
  websocket_response_test.cpp
)

include_directories ("../../include")
