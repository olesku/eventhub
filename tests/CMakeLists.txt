
include_directories ("${PROJECT_SOURCE_DIR}/tests/include" "${PROJECT_SOURCE_DIR}/include")

set(SOURCES
  src/ConfigTest.cpp
  src/TopicTest.cpp
  src/EventLoopTest.cpp
  src/RedisTest.cpp
  src/AccessControllerTest.cpp
  src/UtilTest.cpp
  src/KVStoreTest.cpp
  src/main.cpp
)

add_executable(eventhub_tests ${SOURCES})

target_link_libraries(eventhub_tests eventhub_core)
target_link_libraries(eventhub_tests fmt::fmt)
target_link_libraries(eventhub_tests ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub_tests ${OPENSSL_LIBRARIES})

# Hiredis
find_library(HIREDIS_LIB hiredis)
target_link_libraries(eventhub_tests ${HIREDIS_LIB})

# Redis++https://github.com/sewenew/redis-plus-plus
find_library(REDIS_PLUS_PLUS_LIB redis++)
target_link_libraries(eventhub_tests ${REDIS_PLUS_PLUS_LIB})