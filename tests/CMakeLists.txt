
include_directories ("${PROJECT_SOURCE_DIR}/tests/include")

add_executable(eventhub_tests
  src/topic_test.cpp
)

target_link_libraries(eventhub_tests libeventhub)
target_link_libraries(eventhub_tests ${Glog_LIBRARIES})
#target_link_libraries(eventhub_tests ${CMAKE_THREAD_LIBS_INIT})
#target_link_libraries(eventhub_tests ${OPENSSL_LIBRARIES})
