#ifndef THREAD_PER_TASK_EXECUTOR_H
#define THREAD_PER_TASK_EXECUTOR_H

#include <thread>
#include <utility>
#include <vector>

namespace std {
namespace experimental {

class thread_per_task_executor {
 public:
  typedef function_wrapper wrapper_type;

 public:
  thread_per_task_executor(thread_per_task_executor&) = delete;

  static thread_per_task_executor& get_executor() {
    static thread_per_task_executor instance;
    return instance;
  }

  virtual ~thread_per_task_executor() {
    for (thread& t : thread_vec) {
      if (t.joinable())
        t.join();
    }
  }

 public:
  template<class Func>
  inline void spawn(Func&& closure) {
    thread_vec.emplace_back(forward<Func>(closure));
  }

 private:
  vector<thread> thread_vec;
  thread_per_task_executor() {}
};

}  // namespace experimental
}  // namespace std

#endif  // THREAD_PER_TASK_EXECUTOR_H

