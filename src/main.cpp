#include "common.hpp"
#include "redis_subscriber.hpp"
#include "server.hpp"
#include <iostream>
#include <memory>
#include <signal.h>
#include <stdio.h>
#include <time.h>

using namespace std;

int stop_eventhub = 0;

void shutdown(int sigid) {
  LOG(INFO) << "Exiting.";
  stop_eventhub = 1;
}

int main(int argc, char** argv) {
  struct sigaction sa;
  time_t rawtime;
  struct tm* info;
  char buffer[80];

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  sa.sa_handler = shutdown;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  eventhub::Server server("127.0.0.1", 6379);
  server.start();

  return 0;
}
