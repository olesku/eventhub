add_library( libeventhub
  http/picohttpparser.c
  http/Parser.cpp
  http/Handler.cpp
  http/Response.cpp
  websocket/ws_parser.cpp
  websocket/Parser.cpp
  websocket/Handler.cpp
  websocket/Response.cpp
  metrics/PrometheusRenderer.cpp
  metrics/JsonRenderer.cpp
  Config.cpp
  RPCHandler.cpp
  Redis.cpp
  Util.cpp
  Topic.cpp
  TopicManager.cpp
  Server.cpp
  Connection.cpp
  ConnectionWorker.cpp
  AccessController.cpp
  EpollWrapper.cpp
)

add_executable(eventhub main.cpp)

target_link_libraries(eventhub libeventhub)
target_link_libraries(eventhub ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub ${OPENSSL_CRYPTO_LIBRARIES})
target_link_libraries(eventhub fmt::fmt)
#target_link_libraries(eventhub spdlog)

# Hiredis
find_library(HIREDIS_LIB hiredis)
target_link_libraries(eventhub ${HIREDIS_LIB})

# Redis++https://github.com/sewenew/redis-plus-plus
find_library(REDIS_PLUS_PLUS_LIB redis++)
target_link_libraries(eventhub ${REDIS_PLUS_PLUS_LIB})
