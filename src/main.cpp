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
  eventhub::LOG->info("Exiting.");
  stopEventhub = 1;
}

int main(int argc, char** argv) {
  struct sigaction sa;

  sa.sa_handler = shutdown;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  try {
    eventhub::Config.addString("LOG_LEVEL", "info");
    eventhub::Logger::getInstance().setLevel(eventhub::Config.getString("LOG_LEVEL"));

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
    eventhub::Config.addInt("HANDSHAKE_TIMEOUT", 15);
    eventhub::Config.addInt("DEFAULT_CACHE_TTL", 60);
    eventhub::Config.addInt("MAX_CACHE_REQUEST_LIMIT", 1000);
    eventhub::Config.addBool("DISABLE_AUTH", false);
    eventhub::Config.addBool("ENABLE_SSE", false);
    eventhub::Config.addBool("ENABLE_CACHE", true);
    eventhub::Config.addString("PROMETHEUS_METRIC_PREFIX", "eventhub");
  } catch (std::exception& e) {
    eventhub::LOG->error("Error reading configuration: {}", e.what());
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
