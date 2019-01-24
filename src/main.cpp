#include <iostream>
#include <memory>
#include <signal.h>
#include "common.hpp"
#include "server.hpp"


using namespace std;

int stop_eventhub = 0;

void shutdown(int sigid) {
  LOG(INFO) << "Exiting.";
  stop_eventhub = 1;
}

int main(int argc, char **argv) {
  struct sigaction sa;

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  sa.sa_handler = shutdown;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  auto server = make_shared<eventhub::server>();
  server->start();

  while(!stop_eventhub) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  server->stop();

  return 0;
}
