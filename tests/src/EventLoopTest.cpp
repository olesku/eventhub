#include <chrono>
#include <thread>

#include "catch.hpp"
#include "EventLoop.hpp"

using namespace std;
using namespace eventhub;

TEST_CASE("jobs", "[eventloop]") {
  EventLoop ev;
  bool jobHasRun = false;

  SECTION("hasWork should be false before call to addJob") {
    REQUIRE(ev.hasWork() == false);
  }

  ev.addJob([&jobHasRun]() {
    jobHasRun = true;
  });

  SECTION("hasWork should be true after call to addJob") {
    REQUIRE(ev.hasWork() == true);
  }

  SECTION("job_has_run should be false before processJobs() has been run") {
    REQUIRE(jobHasRun == false);
  }

  SECTION("job_has_run should be true after processJobs() has been run") {
    ev.processJobs();
    REQUIRE(jobHasRun == true);
  }
}

TEST_CASE("timers", "[eventloop]") {
  EventLoop ev;
  bool timerHasRun = false;

  SECTION("hasWork should be false before call to addTimer") {
    REQUIRE(ev.hasWork() == false);
  }

  ev.addTimer(
      100, [&timerHasRun](EventLoop::TimerCtxT* ctx) {
        timerHasRun = true;
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
    REQUIRE(timerHasRun == false);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ev.processTimers();

  SECTION("timer should have been run after 100ms") {
    REQUIRE(timerHasRun == true);
  }

  SECTION("hasWork should be false after call to processTimers") {
    REQUIRE(ev.hasWork() == false);
  }
}
