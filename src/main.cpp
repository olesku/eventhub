#include <signal.h>
#include <stdio.h>
#include <getopt.h>
#include <bits/getopt_core.h>
#include <spdlog/logger.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <string>
#include <atomic>
#include <iostream>
#include <exception>

#include "Config.hpp"
#include "Server.hpp"
#include "Logger.hpp"

namespace eventhub {
extern std::atomic<bool> stopEventhub;
extern std::atomic<bool> reloadEventhub;
}

using namespace eventhub;

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
      LOG->info("No handler for signal {}, ignoring.", sigid);
      return;
  }

shutdown:
  LOG->info("Exiting.");
  stopEventhub = true;
}

/**
 * Raise the open file descriptor soft limit (RLIMIT_NOFILE) up to the hard
 * limit. Each connection consumes a file descriptor, so a low soft limit
 * (commonly 65536 in containers) caps the number of concurrent connections
 * and makes accept() fail with EMFILE once reached. Raising the soft limit
 * up to the hard limit needs no privileges and removes the dependency on the
 * runtime/container default.
 */
void raiseFileDescriptorLimit() {
  struct rlimit rl;

  if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
    LOG->warn("Could not read RLIMIT_NOFILE: {}. Leaving fd limit unchanged.", strerror(errno));
    return;
  }

  if (rl.rlim_cur >= rl.rlim_max) {
    LOG->info("Open file descriptor limit already at maximum ({}).", rl.rlim_cur);
    return;
  }

  const rlim_t previous = rl.rlim_cur;
  rl.rlim_cur           = rl.rlim_max;

  if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
    LOG->warn("Could not raise RLIMIT_NOFILE from {} to {}: {}.", previous, rl.rlim_max, strerror(errno));
    return;
  }

  LOG->info("Raised open file descriptor limit from {} to {}.", previous, rl.rlim_max);
}

void printUsage(char** argv) {
  std::cerr << "Usage:" << std::endl
            << "\t" << argv[0] << " [--config=<file>]" << std::endl;
  exit(0);
}

int main(int argc, char** argv) {
  struct sigaction sa;

  sa.sa_handler = sighandler;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGQUIT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGHUP, &sa, nullptr);

  struct option long_options[] = {
    { "config",  required_argument, 0,  'c' },
    { "help",  required_argument,   0,  'h' },
    { 0,         0,                 0,   0  }
  };

  int option_index = 0;
  std::string cfgFile;

  while(1) {
    auto c = getopt_long(argc, argv, "c:h", long_options, &option_index);

    if (c == -1) {
      break;
    }

    switch (c) {
      case 'c':
        cfgFile.assign(optarg);
      break;

      default:
        printUsage(argv);
    }
  }

  ConfigMap cfgMap = {
      { "listen_port",               ConfigValueType::INT,    "8080",      ConfigValueSettings::REQUIRED },
      { "worker_threads",            ConfigValueType::INT,    "0",         ConfigValueSettings::REQUIRED },
      { "jwt_secret",                ConfigValueType::STRING, "FooBarBaz", ConfigValueSettings::REQUIRED },
      { "log_level",                 ConfigValueType::STRING, "info",      ConfigValueSettings::REQUIRED },
      { "disable_auth",              ConfigValueType::BOOL,   "false",     ConfigValueSettings::REQUIRED },
      { "prometheus_metric_prefix",  ConfigValueType::STRING, "eventhub",  ConfigValueSettings::REQUIRED },
      { "redis_host",                ConfigValueType::STRING, "localhost", ConfigValueSettings::REQUIRED },
      { "redis_port",                ConfigValueType::INT,    "6379",      ConfigValueSettings::REQUIRED },
      { "redis_password",            ConfigValueType::STRING, "",          ConfigValueSettings::OPTIONAL },
      { "redis_prefix",              ConfigValueType::STRING, "eventhub",  ConfigValueSettings::OPTIONAL },
      { "redis_pool_size",           ConfigValueType::INT,    "5",         ConfigValueSettings::REQUIRED },
      { "enable_cache",              ConfigValueType::BOOL,   "false",     ConfigValueSettings::REQUIRED },
      { "max_cache_length",          ConfigValueType::INT,    "1000",      ConfigValueSettings::REQUIRED },
      { "max_cache_request_limit",   ConfigValueType::INT,    "100",       ConfigValueSettings::REQUIRED },
      { "default_cache_ttl",         ConfigValueType::INT,    "60",        ConfigValueSettings::REQUIRED },
      { "ping_interval",             ConfigValueType::INT,    "30",        ConfigValueSettings::REQUIRED },
      { "handshake_timeout",         ConfigValueType::INT,    "5",         ConfigValueSettings::REQUIRED },
      { "enable_sse",                ConfigValueType::BOOL,   "false",     ConfigValueSettings::REQUIRED },
      { "enable_ssl",                ConfigValueType::BOOL,   "false",     ConfigValueSettings::REQUIRED },
      { "ssl_listen_port",           ConfigValueType::INT,    "8443",      ConfigValueSettings::REQUIRED },
      { "ssl_ca_certificate",        ConfigValueType::STRING, "",          ConfigValueSettings::OPTIONAL },
      { "ssl_certificate",           ConfigValueType::STRING, "",          ConfigValueSettings::OPTIONAL },
      { "ssl_private_key",           ConfigValueType::STRING, "",          ConfigValueSettings::OPTIONAL },
      { "ssl_cert_auto_reload",      ConfigValueType::BOOL,   "false",     ConfigValueSettings::OPTIONAL },
      { "ssl_cert_check_interval",   ConfigValueType::INT,    "300",       ConfigValueSettings::OPTIONAL },
      { "disable_unsecure_listener", ConfigValueType::BOOL,   "false",     ConfigValueSettings::OPTIONAL },
      { "enable_kvstore",            ConfigValueType::BOOL,   "true",      ConfigValueSettings::REQUIRED }
    };

  Config cfg(cfgMap);

  try {
    if (!cfgFile.empty()) {
      std::cout << "Loading config from " << cfgFile << std::endl;
      cfg.setFile(cfgFile);
    }

    cfg.setLoadFromEnv(true);
    cfg.load();
    Logger::getInstance().setLevel(cfg.get<std::string>("log_level"));
  } catch (std::exception& e) {
    LOG->error("Error reading configuration: {}", e.what());
    return 1;
  }

  raiseFileDescriptorLimit();

  Server server(cfg);
  server.start();

  return 0;
}
