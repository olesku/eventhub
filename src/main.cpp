#include "Common.hpp"
#include "Config.hpp"
#include "Server.hpp"
#include <iostream>
#include <memory>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <stdexcept>

using namespace std;
extern int stopEventhub;

void shutdown(int sigid) {
  LOG(INFO) << "Exiting.";
  stopEventhub = 1;
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

  try {
    eventhub::Config.add<int>("LISTEN_PORT", 8080);
    eventhub::Config.add<int>("WORKER_THREADS", 0);
    eventhub::Config.add<string>("JWT_SECRET", "eventhub_secret");
    eventhub::Config.add<string>("REDIS_HOST", "127.0.0.1");
    eventhub::Config.add<int>("REDIS_PORT", 6379);
    eventhub::Config.add<string>("REDIS_PASSWORD", "");
    eventhub::Config.add<string>("REDIS_PREFIX", "eventhub");
    eventhub::Config.add<int>("REDIS_POOL_SIZE", 5);
    eventhub::Config.add<int>("MAX_CACHE_LENGTH", 1000);
    eventhub::Config.add<int>("PING_INTERVAL", 30);
    eventhub::Config.add<int>("WEBSOCKET_HANDSHAKE_TIMEOUT", 15);
    eventhub::Config.add<bool>("DISABLE_AUTH", false);
  } catch(std::exception &e) {
    LOG(ERROR) << "Error reading configuration: " << e.what();
    return 1;
  }


  eventhub::Server server(
    eventhub::Config.get<std::string>("REDIS_HOST"),
    eventhub::Config.get<int>("REDIS_PORT"),
    eventhub::Config.get<std::string>("REDIS_PASSWORD"),
    eventhub::Config.get<int>("REDIS_POOL_SIZE")
  );

  server.start();

  return 0;
}
