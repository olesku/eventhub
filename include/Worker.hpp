#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <thread>

template <class T>
using worker_list_t = std::list<std::unique_ptr<T>>;

class WorkerBase {
public:
  WorkerBase() = default;

  virtual ~WorkerBase() {}

  void run() {
    if (!_thread.joinable()) {
      _thread = std::thread(&WorkerBase::_workerMain, this);
    }
  }

  std::thread& thread() {
    return _thread;
  }

  std::thread::id threadId() {
    return _thread.get_id();
  }

  bool stopRequested() {
    return _stop_requested.load(std::memory_order_acquire);
  }

  void stop() {
    if (!_stop_requested.exchange(true, std::memory_order_acq_rel)) {
      _wakeForStop();
    }
  }

private:
  std::thread _thread;
  std::atomic<bool> _stop_requested{false};

protected:
  virtual void _workerMain() {}
  virtual void _wakeForStop() {}
};

template <class T>
class WorkerGroup {
public:
  WorkerGroup()  = default;
  ~WorkerGroup() = default;
  using iterator = typename worker_list_t<T>::iterator;

  void addWorker(std::unique_ptr<T> worker) {
    _workers.push_back(std::move(worker));
    _workers.back()->run();
  }

  void killAndDeleteAll() {
    stopAll();
    joinAll();
    clear();
  }

  void stopAll() {
    for (auto& wrk : _workers) {
      if (wrk->thread().joinable()) {
        wrk->stop();
      }
    }
  }

  void joinAll() {
    for (auto& wrk : _workers) {
      if (wrk->thread().joinable()) {
        wrk->thread().join();
      }
    }
  }

  void clear() {
    _workers.clear();
  }

  worker_list_t<T>& getWorkerList() {
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
