#include "ThreadPool.hpp"

#include <iostream>
#include <thread>

namespace searchserver {

// This is the main loop that all worker threads are born into.  They
// wait for a signal on the work queue condition variable, then they
// grab work off the queue.  Threads return when they notice that
// m_killthreads is true.
void ThreadPool::thread_loop() {
  for (;;) {
    Task task;
    {
      std::unique_lock<std::mutex> lk(m_mtx);
      m_cond.wait(lk,
                  [this] { return m_killthreads || !m_work_queue.empty(); });
      if (m_killthreads && m_work_queue.empty())
        return;
      task = std::move(m_work_queue.front());
      m_work_queue.pop_front();
    }
    if (task.func) {
      try {
        task.func(task.arg);
      } catch (...) { /* swallow errors from tasks */
      }
    }
  }
}

ThreadPool::ThreadPool(size_t num_threads)
    : m_thread_vec(), m_mtx(), m_cond(), m_work_queue(), m_killthreads(false) {
  if (num_threads == 0)
    num_threads = std::thread::hardware_concurrency()
                      ? std::thread::hardware_concurrency()
                      : 4;
  m_thread_vec.reserve(num_threads);
  for (size_t i = 0; i < num_threads; ++i) {
    m_thread_vec.emplace_back([this] { thread_loop(); });
  }
}

ThreadPool::~ThreadPool() {
  {
    std::unique_lock<std::mutex> lk(m_mtx);
    m_killthreads = true;
  }
  m_cond.notify_all();
  // jthread will join on its destructor; clear vector to join now
  m_thread_vec.clear();
}

// Enqueue a Task for dispatch.
void ThreadPool::dispatch(Task t) {
  {
    std::unique_lock<std::mutex> lk(m_mtx);
    m_work_queue.push_back(std::move(t));
  }
  m_cond.notify_one();
}

}  // namespace searchserver
