
include_directories ("${PROJECT_SOURCE_DIR}/tests/include" "${PROJECT_SOURCE_DIR}/include")

add_executable(eventhub_tests
  ../src/hashids.c
  src/topic_test.cpp
  src/eventloop_test.cpp
  src/redis_subscriber_test.cpp
  src/message_test.cpp
  src/main.cpp
)

target_link_libraries(eventhub_tests libeventhub)
target_link_libraries(eventhub_tests ${Glog_LIBRARIES})
target_link_libraries(eventhub_tests ${CMAKE_THREAD_LIBS_INIT})
#target_link_libraries(eventhub_tests ${OPENSSL_LIBRARIES})

add_custom_command(TARGET eventhub_tests POST_BUILD COMMAND ../eventhub_tests)
