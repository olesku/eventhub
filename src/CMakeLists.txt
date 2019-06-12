
add_library( libeventhub
  picohttpparser.c
  ws_parser.cpp
  util.cpp
  http_request.cpp
  http_response.cpp
  websocket_request.cpp
  websocket_response.cpp
  http_handler.cpp
  websocket_handler.cpp
  topic.cpp
  topic_manager.cpp
  server.cpp
  connection.cpp
  connection_worker.cpp
  redis_subscriber.cpp
)

add_executable(eventhub main.cpp)

target_link_libraries(eventhub libeventhub)
target_link_libraries(eventhub ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub ${Glog_LIBRARIES})
target_link_libraries(eventhub ${OPENSSL_LIBRARIES})
