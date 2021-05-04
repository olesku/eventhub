#include <signal.h>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <time.h>
#include <getopt.h>

#include <atomic>
#include <iostream>
#include <memory>

#include "Common.hpp"
#include "Config.hpp"
#include "Server.hpp"

using namespace std;
extern atomic<bool> stopEventhub;
extern atomic<bool> reloadEventhub;

void sighandler(int sigid) {
  switch (sigid) {
    case SIGINT:
      goto shutdown;
    case SIGQUIT:
      goto shutdown;
    case SIGTERM:
      goto shutdown;

    case SIGHUP:
      reloadEventhub = true;
      return;

    default:
      eventhub::LOG->info("No handler for signal {}, ignoring.", sigid);
      return;
  }

shutdown:
  eventhub::LOG->info("Exiting.");
  stopEventhub = true;
}

int main(int argc, char** argv) {
  struct sigaction sa;

  sa.sa_handler = sighandler;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  struct option long_options[] = {
    { "help",    no_argument,       0,  'h' },
    { "config",  required_argument, 0,  'c' },
    { 0,         0,                 0,   0  }
  };

  int option_index = 0;
  string cfgFile;

  while(1) {
    auto c = getopt_long(argc, argv, "hc:", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'c':
        cfgFile.assign(optarg);
      break;

      case 'h':
        printf("HELP\n");
      break;

      case '?':
        break;
    }
  }

  evconfig::ConfigMap cfgMap = {
      { "listen_port",              evconfig::ValueType::INT,    "8080",      evconfig::ValueSettings::REQUIRED },
      { "worker_threads",           evconfig::ValueType::INT,    "0",         evconfig::ValueSettings::REQUIRED },
      { "jwt_secret",               evconfig::ValueType::STRING, "FooBarBaz", evconfig::ValueSettings::REQUIRED },
      { "log_level",                evconfig::ValueType::STRING, "info",      evconfig::ValueSettings::REQUIRED },
      { "disable_auth",             evconfig::ValueType::BOOL,   "false",     evconfig::ValueSettings::REQUIRED },
      { "prometheus_metric_prefix", evconfig::ValueType::STRING, "eventhub",  evconfig::ValueSettings::REQUIRED },
      { "redis_host",               evconfig::ValueType::STRING, "localhost", evconfig::ValueSettings::REQUIRED },
      { "redis_port",               evconfig::ValueType::INT,    "6379",      evconfig::ValueSettings::REQUIRED },
      { "redis_password",           evconfig::ValueType::STRING, "",          evconfig::ValueSettings::OPTIONAL },
      { "redis_prefix",             evconfig::ValueType::STRING, "eventhub",  evconfig::ValueSettings::OPTIONAL },
      { "redis_pool_size",          evconfig::ValueType::INT,    "5",         evconfig::ValueSettings::REQUIRED },
      { "enable_cache",             evconfig::ValueType::BOOL,   "false",     evconfig::ValueSettings::REQUIRED },
      { "max_cache_length",         evconfig::ValueType::INT,    "1000",      evconfig::ValueSettings::REQUIRED },
      { "max_cache_request_limit",  evconfig::ValueType::INT,    "100",       evconfig::ValueSettings::REQUIRED },
      { "default_cache_ttl",        evconfig::ValueType::INT,    "60",        evconfig::ValueSettings::REQUIRED },
      { "ping_interval",            evconfig::ValueType::INT,    "30",        evconfig::ValueSettings::REQUIRED },
      { "handshake_timeout",        evconfig::ValueType::INT,    "5",         evconfig::ValueSettings::REQUIRED },
      { "enable_sse",               evconfig::ValueType::BOOL,   "false",     evconfig::ValueSettings::REQUIRED },
      { "enable_ssl",               evconfig::ValueType::BOOL,   "false",     evconfig::ValueSettings::REQUIRED },
      { "ssl_ca_certificate",       evconfig::ValueType::STRING, "",          evconfig::ValueSettings::OPTIONAL },
      { "ssl_certificate",          evconfig::ValueType::STRING, "",          evconfig::ValueSettings::OPTIONAL },
      { "ssl_private_key",          evconfig::ValueType::STRING, "",          evconfig::ValueSettings::OPTIONAL }
    };

  evconfig::Config cfg(cfgMap);

  try {
    if (!cfgFile.empty()) {
      cout << "Loading config from " << cfgFile << endl;
      cfg.setFile(cfgFile);
    }

    cfg.setLoadFromEnv(true);
    cfg.load();
    eventhub::Logger::getInstance().setLevel(cfg.get<std::string>("log_level"));
  } catch (std::exception& e) {
    eventhub::LOG->error("Error reading configuration: {}", e.what());
    return 1;
  }



  eventhub::Server server(cfg);
  server.start();

  return 0;
}
