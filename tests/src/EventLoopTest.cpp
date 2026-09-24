#include <chrono>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <vector>
#include <memory>

#include "EventLoop.hpp"
#include "catch.hpp"

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

TEST_CASE("jobs added by jobs run in the next batch", "[eventloop]") {
  EventLoop ev;
  std::vector<int> order;

  ev.addJob([&]() {
    order.push_back(1);
    ev.addJob([&]() { order.push_back(3); });
    order.push_back(2);
  });

  ev.processJobs();
  REQUIRE(order == std::vector<int>{1, 2});
  REQUIRE(ev.hasWork());
  ev.processJobs();
  REQUIRE(order == std::vector<int>{1, 2, 3});
}

TEST_CASE("one throwing job does not abandon the extracted batch", "[eventloop]") {
  EventLoop ev;
  bool secondRan = false;
  ev.addJob([]() { throw std::runtime_error("expected"); });
  ev.addJob([&]() { secondRan = true; });

  REQUIRE_THROWS_AS(ev.processJobs(), std::runtime_error);
  REQUIRE(secondRan);
  REQUIRE_FALSE(ev.hasWork());
}

TEST_CASE("concurrent job producers do not lose work", "[eventloop]") {
  EventLoop ev;
  std::atomic<int> count{0};
  std::vector<std::thread> producers;
  for (int producer = 0; producer < 4; ++producer) {
    producers.emplace_back([&]() {
      for (int job = 0; job < 250; ++job) {
        ev.addJob([&]() { ++count; });
      }
    });
  }
  for (auto& producer : producers) {
    producer.join();
  }

  ev.processJobs();
  REQUIRE(count == 1000);
}

TEST_CASE("timer callbacks may add timers and change repetition", "[eventloop]") {
  EventLoop ev;
  int count = 0;
  ev.addTimer(0, [&](TimerCtx* ctx) {
    ++count;
    ctx->repeat = false;
    ev.addTimer(0, [&](TimerCtx*) { ++count; });
  }, true);

  ev.processTimers();
  REQUIRE(count == 1);
  REQUIRE(ev.hasWork());
  ev.processTimers();
  REQUIRE(count == 2);
  REQUIRE_FALSE(ev.hasWork());
}

TEST_CASE("timers", "[eventloop]") {
  EventLoop ev;
  bool timerHasRun = false;

  SECTION("hasWork should be false before call to addTimer") {
    REQUIRE(ev.hasWork() == false);
  }

  ev.addTimer(
      100, [&timerHasRun](TimerCtx* ctx) {
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
