set (SOURCES
  http/picohttpparser.c
  http/Parser.cpp
  http/Handler.cpp
  http/Response.cpp
  websocket/ws_parser.cpp
  websocket/Parser.cpp
  websocket/Handler.cpp
  websocket/Response.cpp
  sse/Handler.cpp
  sse/Response.cpp
  metrics/PrometheusRenderer.cpp
  metrics/JsonRenderer.cpp
  Config.cpp
  RPCHandler.cpp
  Redis.cpp
  KVStore.cpp
  Util.cpp
  Topic.cpp
  TopicManager.cpp
  Server.cpp
  Connection.cpp
  SSLConnection.cpp
  ConnectionWorker.cpp
  AccessController.cpp
  EpollWrapper.cpp
)

add_library(eventhub_core ${SOURCES})

add_executable(eventhub main.cpp)

find_library(HIREDIS_LIB hiredis)
find_library(REDIS_PLUS_PLUS_LIB redis++)

target_link_libraries(eventhub eventhub_core)
target_link_libraries(eventhub ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub ${OPENSSL_CRYPTO_LIBRARIES})
target_link_libraries(eventhub ${OPENSSL_LIBRARIES})
target_link_libraries(eventhub ${HIREDIS_LIB})
target_link_libraries(eventhub ${REDIS_PLUS_PLUS_LIB})
target_link_libraries(eventhub fmt::fmt)