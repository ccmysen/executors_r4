#ifndef THREAD_POOL_EXECUTOR_H
#define THREAD_POOL_EXECUTOR_H

#include <iostream>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

#include "executor_helper.h"

namespace std {
namespace experimental {

// Default to the type erasing wrapper.
// Add an allocator to the template to allow custom allocation of internal objects?
template <class Wrapper=function_wrapper>
class thread_pool_executor {
 public:
  // thread pools are not copyable/default constructable
  thread_pool_executor() = delete;
  thread_pool_executor(const thread_pool_executor&) = delete;

  explicit thread_pool_executor(size_t N)
      : threads_(N), {
    atomic_init(&in_shutdown_, NOT_SHUTDOWN);
    for (size_t i = 0; i < N; ++i) {
      threads_.emplace_back(std::bind(&thread_pool_executor::run_thread, this));
    }
  }

  virtual ~thread_pool_executor() {
    set_shutdown(SOFT_SHUTDOWN);

    // Shut down the workers
    // Wait for them to join
    for (auto& t: threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  }

  virtual void shutdown_hard() {
    set_shutdown(HARD_SHUTDOWN);

    // Wait for them to join
    for (auto& t: threads_) {
      if (t.joinable()) {
        t.join();
      }
    }
  } 

  template<class Collect>
  void spawn_all(Collect& functions) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    if (in_hard_shutdown()) return;

    bool notify = work_queue_.empty();
    for (auto&& func : functions) {
      work_queue_.emplace(forward<Func>(func));
    }
    queue_lock.unlock();

    if (notify) {
      queue_empty_cv_.notify_all();
    }
  }

 public:
  // Executor interface
  template<class Func>
  void spawn(Func&& func) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    if (in_hard_shutdown()) return;

    bool notify = work_queue_.empty();
    work_queue_.emplace(forward<Func>(func));
    queue_lock.unlock();

    if (notify) {
      queue_empty_cv_.notify_all();
    }
  }

 private:
  // Shutdown immediately after current tasks complete
  static constexpr int NOT_SHUTDOWN = 0;
  // Shutdown threads as the queue empties
  static constexpr int SOFT_SHUTDOWN = 1;
  static constexpr int HARD_SHUTDOWN = 2;

  // Allow a thread to request tasks from the main queue. If there isn't much
  // contention on the main queue then this should be relatively cheap.
  bool transfer_tasks(queue<Wrapper>& dest_queue) {
    unique_lock<mutex> queue_lock(work_queue_mu_);
    queue_empty_cv_.wait(
        queue_lock, [this]{ return in_shutdown() || !work_queue_.empty(); });
    // Really nothing to do
    if ((in_shutdown() && work_queue_.empty()) || in_hard_shutdown()) {
      return false;
    }

    

  }

  bool release_tasks() {

  }

  void run_thread() {
    while (true) {
      Wrapper next_fn(move(work_queue_.front()));
      work_queue_.pop();
      queue_lock.unlock();

      // Now run it.
      next_fn();
    }
  }

  // Lock must be held by this thread before calling.
  inline bool in_shutdown() {
    return in_shutdown_.load() > NOT_SHUTDOWN;
  }

  inline bool in_hard_shutdown() {
    return in_shutdown_.load() == HARD_SHUTDOWN;
  }

  void set_shutdown(int shutdown_type) {
    int expected = NOT_SHUTDOWN;
    bool exchanged =
        in_shutdown_.compare_exchange_strong(&expected, shutdown_type);
    if (!exchanged) return;
    queue_empty_cv_.notify_all();
  }

  vector<thread> threads_;
  mutex work_queue_mu_;
  condition_variable queue_empty_cv_;
  deque<Wrapper> work_queue_;
  atomic<int> in_shutdown_;
};

}  // namespace experimental
}  // namespace std

#endif
