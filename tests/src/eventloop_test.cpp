#include <chrono>
#include <thread>

#include "catch.hpp"
#include "event_loop.hpp"

using namespace std;
using namespace eventhub;

TEST_CASE("jobs", "[eventloop]") {
  EventLoop ev;
  bool job_has_run = false;

  SECTION("hasWork should be false before call to addJob") {
    REQUIRE(ev.hasWork() == false);
  }

  ev.addJob([&job_has_run]() {
    job_has_run = true;
  });

  SECTION("hasWork should be true after call to addJob") {
    REQUIRE(ev.hasWork() == true);
  }

  SECTION("job_has_run should be false before processJobs() has been run") {
    REQUIRE(job_has_run == false);
  }

  SECTION("job_has_run should be true after processJobs() has been run") {
    ev.processJobs();
    REQUIRE(job_has_run == true);
  }
}

TEST_CASE("timers", "[eventloop]") {
  EventLoop ev;
  bool timer_has_run = false;

  SECTION("hasWork should be false before call to addTimer") {
    REQUIRE(ev.hasWork() == false);
  }

  ev.addTimer(
      100, [&timer_has_run](EventLoop::timer_ctx_t* ctx) {
        timer_has_run = true;
      },
      false);

  SECTION("hasWork should be true after call to addTimer") {
    REQUIRE(ev.hasWork() == true);
  }

  SECTION("getNextTimerDelay should be > 90ms") {
    REQUIRE(ev.getNextTimerDelay() > std::chrono::milliseconds(90));
  }

  ev.processTimers();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  SECTION("timer should not have been run before after 100ms") {
    REQUIRE(timer_has_run == false);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ev.processTimers();

  SECTION("timer should have been run after 100ms") {
    REQUIRE(timer_has_run == true);
  }

  SECTION("hasWork should be false after call to processTimers") {
    REQUIRE(ev.hasWork() == false);
  }
}
