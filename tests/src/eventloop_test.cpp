#include <chrono>
#include <thread>

#include "catch.hpp"
#include "event_loop.hpp"

using namespace std;
using namespace eventhub;

TEST_CASE("jobs", "[eventloop]") {
  event_loop ev;
  bool job_has_run = false;

  SECTION("has_work should be false before call to add_job") {
    REQUIRE(ev.has_work() == false);
  }

  ev.add_job([&job_has_run]() {
    job_has_run = true;
  });

  SECTION("has_work should be true after call to add_job") {
    REQUIRE(ev.has_work() == true);
  }

  SECTION("job_has_run should be false before process_jobs() has been run") {
    REQUIRE(job_has_run == false);
  }

  SECTION("job_has_run should be true after process_jobs() has been run") {
    ev.process_jobs();
    REQUIRE(job_has_run == true);
  }
}

TEST_CASE("timers", "[eventloop]") {
  event_loop ev;
  bool timer_has_run = false;

  SECTION("has_work should be false before call to add_timer") {
    REQUIRE(ev.has_work() == false);
  }

  ev.add_timer(100, [&timer_has_run](event_loop::timer_ctx_t *ctx) {
    timer_has_run = true;
  }, false);

  SECTION("has_work should be true after call to add_timer") {
    REQUIRE(ev.has_work() == true);
  }

  SECTION("get_next_timer_delay should be > 90ms") {
    REQUIRE(ev.get_next_timer_delay() > std::chrono::milliseconds(90));
  }

  ev.process_timers();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  SECTION("timer should not have been run before after 100ms") {
    REQUIRE(timer_has_run == false);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  ev.process_timers();

  SECTION("timer should have been run after 100ms") {
    REQUIRE(timer_has_run == true);
  }

  SECTION("has_work should be false after call to process_timers") {
    REQUIRE(ev.has_work() == false);
  }
}
