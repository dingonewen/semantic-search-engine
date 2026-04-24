#ifndef THREADPOOL_HPP_
#define THREADPOOL_HPP_

#include <condition_variable>
#include <cstddef>  // for size_t
#include <deque>    // for std::deque
#include <mutex>
#include <thread>  // for jthread
#include <vector>  // for std::vector

namespace searchserver {

// A ThreadPool maintains a pool of worker threads that process Tasks from a
// queue. Customers dispatch Tasks; a free thread picks up each task and runs
// it.
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads);

  // Destructor joins all threads after draining remaining tasks.
  ~ThreadPool();

  // function pointer type for task callbacks
  using thread_task_fn = void (*)(void* arg);

  struct Task {
    thread_task_fn func;
    void* arg;
  };

  // Enqueue a Task for dispatch to a worker thread.
  void Dispatch(Task t);

  auto operator=(const ThreadPool& other) -> ThreadPool& = delete;
  auto operator=(ThreadPool&& other) -> ThreadPool& = delete;
  ThreadPool(const ThreadPool& other) = delete;
  ThreadPool(ThreadPool&& other) = delete;

 private:
  std::mutex m_mtx;
  std::condition_variable m_cond;
  std::deque<Task> m_work_queue;
  bool m_killthreads;
  std::vector<std::jthread> m_thread_vec;

  void ThreadLoop();
};

}  // namespace searchserver

#endif  // THREADPOOL_HPP_
