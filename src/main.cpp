#include <iostream>
#include <memory>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "common.hpp"
#include "server.hpp"
#include "redis_subscriber.hpp"


using namespace std;

int stop_eventhub = 0;

void shutdown(int sigid) {
  LOG(INFO) << "Exiting.";
  stop_eventhub = 1;
}

int main(int argc, char **argv) {
  struct sigaction sa;
  time_t rawtime;
   struct tm *info;
   char buffer[80];

  FLAGS_logtostderr = 1;
  google::InitGoogleLogging(argv[0]);

  sa.sa_handler = shutdown;
  sa.sa_flags   = 0;

  sigemptyset(&(sa.sa_mask));
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);

  eventhub::server server_inst;
  eventhub::redis_subscriber redis_inst;

  redis_inst.connect("localhost", "6379");

  return 0;

  server_inst.start();

  while(!stop_eventhub) {
   time( &rawtime );
   info = localtime( &rawtime );

   strftime(buffer,80,"%x - %H:%M:%S", info);
    server_inst.publish("system/clock", buffer);
    server_inst.publish("ole/fredrik", "Er kul");
    server_inst.publish("boring/channel", "This is boring");
    server_inst.publish("eventhub", "rocks!");
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  }

  server_inst.stop();

  return 0;
}
