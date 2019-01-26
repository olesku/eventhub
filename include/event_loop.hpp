#include <map>
#include <deque>
#include <chrono>
#include <functional>
#include <iostream>

namespace eventhub {
  class event_loop {
    typedef struct {
      std::chrono::milliseconds fire_time;
      std::function<void()> callback;
    } task_t;

    typedef std::map<int64_t, std::deque<task_t>> task_queue;
    public:
      inline void next() {
        auto cur_ms = now_ms();
        auto cur_sec = now_sec().count();

        if (_queue.empty()) {
          return;
        }

        for (auto it = _queue.begin(); it != _queue.end();) {
          auto sec_group = it->first;
          auto queue = it->second;

          if (sec_group <= cur_sec) {
            std::cout << "Should run second: " << sec_group << " <= " << cur_sec << std::endl;

            for(auto it2 = queue.begin(); it2 != queue.end();) {
              auto task = *it2;

              if (task.fire_time <= cur_ms) {
                std::cout << "Should run ms: " << task.fire_time.count() << " <= " << cur_ms.count() << std::endl;
                task.callback();
                queue.erase(it2);
              }
            }

            if (queue.empty()) {
              _queue.erase(it++);
            } else {
              ++it;
            }
          }
        }
      }

      inline void add(unsigned int delay, std::function<void()> callback) {
        auto fire_time = now_ms() + std::chrono::milliseconds(delay);
        auto key = now_sec().count();

        std::cout << "Key: " << key << std::endl; 
        _queue[key].push_back(task_t{fire_time, callback});
      }

    private:
      task_queue _queue;

      std::chrono::milliseconds now_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
      }

      std::chrono::milliseconds now_sec() {
        return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch());
      }
  };
}