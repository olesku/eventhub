#include <iostream>
#include <chrono>
#include <thread>
#include "../include/event_loop.hpp"

using namespace eventhub;
using namespace std;

int main(int argc, char* argv[]) {
  event_loop e;


for (int i = 0; i < 2; i++) {
  e.add_timer(1000*i, [i](struct event_loop::timer_ctx_t *ctx) {
    std::cout << "Callback " << i << std::endl;
  }, false);
}

e.add_job([]{
  std::cout << "Job 1" << std::endl;
});

e.add_job([]{
  std::cout << "Job 2" << std::endl;
});

  e.add_timer(2000, [](struct event_loop::timer_ctx_t *ctx) {
    std::cout << "Callback repeat" << std::endl;
  }, true);

  while(1) {
    e.process();
    cout << "Sleeping for " << e.get_next_timer_delay().count() << " ms" << endl;
    std::this_thread::sleep_for(e.get_next_timer_delay());
  }
  return 0;
}
