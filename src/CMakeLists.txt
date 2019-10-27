
add_library( libeventhub
  picohttpparser.c
  redis.cpp
  util.cpp
  http_request.cpp
  http_response.cpp
  http_handler.cpp
  websocket/ws_parser.cpp
  websocket/StateMachine.cpp
  websocket/Handler.cpp
  websocket/Response.cpp
  topic.cpp
  topic_manager.cpp
  server.cpp
  connection.cpp
  connection_worker.cpp
  access_controller.cpp
)

add_executable(eventhub main.cpp)

target_link_libraries(eventhub libeventhub)
target_link_libraries(eventhub ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub ${Glog_LIBRARIES})
target_link_libraries(eventhub ${OPENSSL_LIBRARIES})

# Hiredis
find_library(HIREDIS_LIB hiredis)
target_link_libraries(eventhub ${HIREDIS_LIB})

# Redis++https://github.com/sewenew/redis-plus-plus
find_library(REDIS_PLUS_PLUS_LIB redis++)
target_link_libraries(eventhub ${REDIS_PLUS_PLUS_LIB})