#ifndef INCLUDE_WORKER_HPP_
#define INCLUDE_WORKER_HPP_

#include <chrono>
#include <future>
#include <list>
#include <memory>
#include <thread>

template <class T>
using worker_list_t = std::list<std::unique_ptr<T>>;

class WorkerBase {
public:
  WorkerBase() {
    _stop_requested_future = _stop_requested.get_future();
  }

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
  virtual void _workerMain() {}
};

template <class T>
class WorkerGroup {
public:
  WorkerGroup<T>() {}
  ~WorkerGroup<T>() {}
  using iterator = typename worker_list_t<T>::iterator;

  void addWorker(T* worker) {
    _workers.push_back(std::unique_ptr<T>(worker));
    worker->run();
  }

  void killAndDeleteAll() {
    for (auto& wrk : _workers) {
      if (wrk->thread().joinable()) {
        wrk->stop();
        wrk->thread().join();
      }
    }

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

#endif // INCLUDE_WORKER_HPP_
