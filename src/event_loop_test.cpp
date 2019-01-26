#include <iostream>
#include "../include/event_loop.hpp"

using namespace eventhub;
using namespace std;

int main(int argc, char* argv[]) {
  event_loop e;

  e.add(1000, []{

  });

  while(1) {
    e.next();
  }
  return 0;
}