
include_directories ("${PROJECT_SOURCE_DIR}/tests/include" "${PROJECT_SOURCE_DIR}/include")

add_executable(eventhub_tests
  src/TopicTest.cpp
  src/EventLoopTest.cpp
  src/RedisTest.cpp
  src/AccessControllerTest.cpp
  src/ConfigTest.cpp
  src/main.cpp
)

target_link_libraries(eventhub_tests core)
target_link_libraries(eventhub_tests fmt::fmt)
#target_link_libraries(eventhub_tests spdlog)
target_link_libraries(eventhub_tests ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(eventhub_tests ${OPENSSL_LIBRARIES})

# Hiredis
find_library(HIREDIS_LIB hiredis)
target_link_libraries(eventhub_tests ${HIREDIS_LIB})

# Redis++https://github.com/sewenew/redis-plus-plus
find_library(REDIS_PLUS_PLUS_LIB redis++)
target_link_libraries(eventhub_tests ${REDIS_PLUS_PLUS_LIB})

add_custom_command(TARGET eventhub_tests POST_BUILD COMMAND ../eventhub_tests)
