project (eventhub)
cmake_minimum_required (VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_C_FLAGS "-Wall -ggdb")
set(CMAKE_CXX_FLAGS "-std=c++11")

add_executable( eventhub 
  src/picohttpparser.c
  src/ws_parser.c
  src/http_request.cpp
  src/http_response.cpp
  src/websocket_handler.cpp
  src/server.cpp
  src/connection.cpp
  src/connection_worker.cpp
  src/main.cpp
)

# Glog
find_package( Glog REQUIRED )
include_directories( ${Glog_INCLUDE_PATH} )

# Threads
find_package( Threads REQUIRED )
include_directories( ${Threads_INCLUDE_DIR} )

# OpenSSL
find_package( OpenSSL REQUIRED )
include_directories( ${OPENSSL_INCLUDE_DIR} )


# Include local header files
include_directories ("${PROJECT_SOURCE_DIR}/include" "${PROJECT_SOURCE_DIR}/lib")

target_link_libraries( eventhub ${CMAKE_THREAD_LIBS_INIT} )
target_link_libraries( eventhub ${Glog_LIBRARIES} )
target_link_libraries( eventhub ${OPENSSL_LIBRARIES} )
