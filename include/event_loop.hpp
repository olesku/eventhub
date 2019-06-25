#ifndef EVENTHUB_EVENT_LOOP_HPP
#define EVENTHUB_EVENT_LOOP_HPP

#include <chrono>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>

namespace eventhub {
class EventLoop {
public:
  typedef struct time_ctx_t time_ctx_t;

  struct timer_ctx_t {
    std::chrono::milliseconds fire_time;
    std::chrono::milliseconds repeat_delay;
    std::function<void(timer_ctx_t* ctx)> callback;
    bool repeat;
  };

  typedef std::deque<timer_ctx_t> timer_queue_t;
  typedef std::deque<std::function<void()>> job_queue_t;

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

    for (auto timer_queue_iterator = _timer_queue.begin(); timer_queue_iterator != _timer_queue.end();) {
      auto& timer_task = *timer_queue_iterator;

      if (timer_task.fire_time <= now) {
        timer_task.callback(&timer_task);

        if (timer_task.repeat) {
          timer_task.fire_time = _now() + timer_task.repeat_delay;
          _decreaseNextFiretimeIfLess(timer_task.fire_time);
          timer_queue_iterator++;
        } else {
          timer_queue_iterator = _timer_queue.erase(timer_queue_iterator);
        }

        continue;
      }

      _decreaseNextFiretimeIfLess(timer_task.fire_time);
      timer_queue_iterator++;
    }
  }

  inline void addTimer(int64_t delay, std::function<void(timer_ctx_t* ctx)> callback, bool repeat = false) {
    std::lock_guard<std::mutex> lock(_timer_queue_lock);
    const auto fire_time = _now() + std::chrono::milliseconds(delay);
    timer_ctx_t ctx{fire_time, std::chrono::milliseconds(delay), callback, repeat};
    _decreaseNextFiretimeIfLess(fire_time);
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

  void _decreaseNextFiretimeIfLess(const std::chrono::milliseconds& fire_time) {
    if (_next_timer_fire_time == std::chrono::milliseconds::zero() || _next_timer_fire_time > fire_time) {
      _next_timer_fire_time = fire_time;
    }
  }

  const std::chrono::milliseconds _now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }
};
} // namespace eventhub

#endif
