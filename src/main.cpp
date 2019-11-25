#include <signal.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <time.h>

#include <iostream>
#include <memory>
#include <atomic>

#include "Common.hpp"
#include "Config.hpp"
#include "Server.hpp"

using namespace std;
extern atomic<bool> stopEventhub;

void shutdown(int sigid) {
  LOG(INFO) << "Exiting.";
  stopEventhub = 1;
}

int main(int argc, char** argv) {
  struct sigaction sa;

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  sa.sa_handler = shutdown;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  try {
    eventhub::Config.addInt("LISTEN_PORT", 8080);
    eventhub::Config.addInt("WORKER_THREADS", 0);
    eventhub::Config.addString("JWT_SECRET", "eventhub_secret");
    eventhub::Config.addString("REDIS_HOST", "127.0.0.1");
    eventhub::Config.addInt("REDIS_PORT", 6379);
    eventhub::Config.addString("REDIS_PASSWORD", "");
    eventhub::Config.addString("REDIS_PREFIX", "eventhub");
    eventhub::Config.addInt("REDIS_POOL_SIZE", 5);
    eventhub::Config.addInt("MAX_CACHE_LENGTH", 1000);
    eventhub::Config.addInt("PING_INTERVAL", 30);
    eventhub::Config.addInt("WEBSOCKET_HANDSHAKE_TIMEOUT", 15);
    eventhub::Config.addBool("DISABLE_AUTH", false);
    eventhub::Config.addString("PROMETHEUS_METRIC_PREFIX", "eventhub");
  } catch (std::exception& e) {
    LOG(ERROR) << "Error reading configuration: " << e.what();
    return 1;
  }

  eventhub::Server server(
      eventhub::Config.getString("REDIS_HOST"),
      eventhub::Config.getInt("REDIS_PORT"),
      eventhub::Config.getString("REDIS_PASSWORD"),
      eventhub::Config.getInt("REDIS_POOL_SIZE"));

  server.start();

  return 0;
}
