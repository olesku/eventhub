#ifndef EVENTHUB_EVENT_LOOP_HPP
#define EVENTHUB_EVENT_LOOP_HPP

#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
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

class EventLoop {
public:
  EventLoop() {
    _next_timer_fire_time = std::chrono::milliseconds::zero();
  }

  inline void process() {
    processJobs();
    processTimers();
  }

  inline void processJobs() {
    std::lock_guard<std::mutex> lock(_job_queue_lock);

    if (_job_queue.empty()) {
      return;
    }

    for (auto& callback : _job_queue) {
      callback();
    }

    _job_queue.clear();
  }

  inline void processTimers() {
    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    const auto now = _now();

    if (_timer_queue.empty() || _next_timer_fire_time > now) {
      return;
    }

    _next_timer_fire_time = std::chrono::milliseconds::zero();

    for (auto timerQueueIterator = _timer_queue.begin(); timerQueueIterator != _timer_queue.end();) {
      auto& timerTask = *timerQueueIterator;

      if (timerTask.fire_time <= now) {
        timerTask.callback(&timerTask);

        if (timerTask.repeat) {
          timerTask.fire_time = _now() + timerTask.repeat_delay;
          _decreaseNextFiretimeIfLess(timerTask.fire_time);
          timerQueueIterator++;
        } else {
          timerQueueIterator = _timer_queue.erase(timerQueueIterator);
        }

        continue;
      }

      _decreaseNextFiretimeIfLess(timerTask.fire_time);
      timerQueueIterator++;
    }
  }

  inline void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false) {
    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    const auto fireTime = _now() + std::chrono::milliseconds(delay);
    TimerCtx ctx{fireTime, std::chrono::milliseconds(delay), callback, repeat};
    _decreaseNextFiretimeIfLess(fireTime);
    _timer_queue.push_back(ctx);
  }

  inline const std::chrono::milliseconds getNextTimerDelay() {
    if (!_job_queue.empty()) {
      return std::chrono::milliseconds(0);
    }

    const auto delay = _next_timer_fire_time - _now();
    return (delay < std::chrono::milliseconds(0) || delay == std::chrono::milliseconds::zero()) ? std::chrono::milliseconds(0) : delay;
  }

  inline void addJob(std::function<void()> callback) {
    std::lock_guard<std::mutex> lock(_job_queue_lock);
    _job_queue.push_back(callback);
  }

  inline bool hasWork() {
    return !_job_queue.empty() || !_timer_queue.empty();
  }

private:
  timer_queue_t _timer_queue;
  job_queue_t _job_queue;
  std::mutex _timer_queue_lock;
  std::mutex _job_queue_lock;
  std::chrono::milliseconds _next_timer_fire_time;

  void _decreaseNextFiretimeIfLess(const std::chrono::milliseconds& fireTime) {
    if (_next_timer_fire_time == std::chrono::milliseconds::zero() || _next_timer_fire_time > fireTime) {
      _next_timer_fire_time = fireTime;
    }
  }

  const std::chrono::milliseconds _now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }
};
} // namespace eventhub

#endif
