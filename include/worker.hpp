#ifndef EVENTHUB_WORKER_HPP
#define EVENTHUB_WORKER_HPP

#include <memory>
#include <thread>
#include <vector>
#include <future>
#include <chrono>


template <class T>
using worker_list_t = std::vector<std::unique_ptr<T>>;

class worker_base {
  public:
    worker_base() {
      _stop_requested_future = _stop_requested.get_future();
    }

    ~worker_base() {}

    void run() {
      if (!_thread.joinable()) {
        _thread = std::thread(&worker_base::worker_main, this);
      }
    }

    std::thread& thread() {
      return _thread;
    }

    std::thread::id thread_id() {
      return _thread.get_id();
    }

    bool stop_requested() {
      if (_stop_requested_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout) {
        return false;
      }

      return true;
    }

    void stop() {
      _stop_requested.set_value();
    }

  private:
    std::thread _thread;
    std::promise<void> _stop_requested;
    std::future<void> _stop_requested_future;

  protected:
    virtual void worker_main() {}
};

template <class T>
class worker_group {
  public:
    worker_group<T>() {};
    ~worker_group<T>(){};
    using iterator = typename worker_list_t<T>::iterator;

    void add_worker(T* worker) {
       _workers.push_back(std::unique_ptr<T>(worker));
       worker->run();
    }

    void kill_and_delete_all() {
      for (auto &wrk : _workers) {
        if (wrk->thread().joinable()) {
          wrk->stop();
          wrk->thread().join();
        }
      }

       _workers.clear();
    }

    worker_list_t<T>& get_worker_list() {
      return _workers;
    }

    typename worker_list_t<T>::iterator begin() {
      return _workers.begin();
    }

    typename worker_list_t<T>::iterator end() {
      return _workers.end();
    }

  private:
    worker_list_t<T> _workers;
};

#endif
