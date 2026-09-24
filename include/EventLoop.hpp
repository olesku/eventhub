#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <exception>
#include <mutex>

namespace eventhub {

struct TimerCtx {
  std::chrono::milliseconds fire_time;
  std::chrono::milliseconds repeat_delay;
  std::function<void(TimerCtx* ctx)> callback;
  bool repeat;
};

using timer_queue_t = std::deque<TimerCtx>;
using job_queue_t   = std::deque<std::function<void()>>;

class EventLoop final {
public:
  EventLoop() {
    _next_timer_fire_time = std::chrono::milliseconds::zero();
  }

  void process() {
    processJobs();
    processTimers();
  }

  void processJobs() {
    job_queue_t jobs;
    {
      std::lock_guard<std::mutex> lock(_job_queue_lock);
      jobs.swap(_job_queue);
    }

    std::exception_ptr firstException;
    for (auto& callback : jobs) {
      try {
        callback();
      } catch (...) {
        if (!firstException) {
          firstException = std::current_exception();
        }
      }
    }

    if (firstException) {
      std::rethrow_exception(firstException);
    }
  }

  void processTimers() {
    const auto now = _now();
    timer_queue_t due;
    {
      std::lock_guard<std::mutex> lock(_timer_queue_lock);
      if (_timer_queue.empty() || _next_timer_fire_time > now) {
        return;
      }
      _next_timer_fire_time = std::chrono::milliseconds::zero();
      for (auto iterator = _timer_queue.begin(); iterator != _timer_queue.end();) {
        if (iterator->fire_time <= now) {
          due.push_back(std::move(*iterator));
          iterator = _timer_queue.erase(iterator);
        } else {
          _decreaseNextFiretimeIfLessLocked(iterator->fire_time);
          ++iterator;
        }
      }
    }

    std::exception_ptr firstException;
    for (auto& timer : due) {
      bool callbackCompleted = false;
      try {
        timer.callback(&timer);
        callbackCompleted = true;
      } catch (...) {
        if (!firstException) {
          firstException = std::current_exception();
        }
      }

      if (callbackCompleted && timer.repeat) {
        timer.fire_time = _now() + timer.repeat_delay;
        std::lock_guard<std::mutex> lock(_timer_queue_lock);
        _decreaseNextFiretimeIfLessLocked(timer.fire_time);
        _timer_queue.push_back(std::move(timer));
      }
    }

    if (firstException) {
      std::rethrow_exception(firstException);
    }
  }

  void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false) {
    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    const auto fireTime = _now() + std::chrono::milliseconds(delay);
    TimerCtx ctx{fireTime, std::chrono::milliseconds(delay), callback, repeat};
    _decreaseNextFiretimeIfLessLocked(fireTime);
    _timer_queue.push_back(ctx);
  }

  const std::chrono::milliseconds getNextTimerDelay() {
    {
      std::lock_guard<std::mutex> lock(_job_queue_lock);
      if (!_job_queue.empty()) {
        return std::chrono::milliseconds(0);
      }
    }

    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    const auto nextFire = _next_timer_fire_time;

    const auto delay = nextFire - _now();
    return (delay < std::chrono::milliseconds(0) || delay == std::chrono::milliseconds::zero()) ? std::chrono::milliseconds(0) : delay;
  }

  const std::chrono::milliseconds getNextTimerFireTime() {
    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    return _next_timer_fire_time;
  }

  void addJob(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(_job_queue_lock);
    _job_queue.push_back(callback);
  }

  bool hasWork() {
    {
      std::lock_guard<std::mutex> lock(_job_queue_lock);
      if (!_job_queue.empty()) {
        return true;
      }
    }

    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    return !_timer_queue.empty();
  }

private:
  timer_queue_t _timer_queue;
  job_queue_t _job_queue;
  std::mutex _timer_queue_lock;
  std::mutex _job_queue_lock;
  std::chrono::milliseconds _next_timer_fire_time;

  void _decreaseNextFiretimeIfLessLocked(const std::chrono::milliseconds& fireTime) {
    if (_next_timer_fire_time == std::chrono::milliseconds::zero() || _next_timer_fire_time > fireTime) {
      _next_timer_fire_time = fireTime;
    }
  }

  const std::chrono::milliseconds _now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }
};

} // namespace eventhub
