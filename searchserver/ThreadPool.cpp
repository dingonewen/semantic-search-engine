#include "./ThreadPool.hpp"

namespace searchserver {

// Worker thread main loop — waits for tasks, runs them, exits when killed.
void ThreadPool::ThreadLoop() {
  while (true) {
    Task t;
    {
      std::unique_lock<std::mutex> lock(m_mtx);
      m_cond.wait(lock,
                  [this] { return m_killthreads || !m_work_queue.empty(); });
      if (m_killthreads && m_work_queue.empty()) {
        return;  // drain remaining tasks before dying
      }
      t = std::move(m_work_queue.front());
      m_work_queue.pop_front();
    }  // lock released before running task
    t.func(t.arg);
  }
}

ThreadPool::ThreadPool(size_t num_threads)
    : m_mtx(), m_cond(), m_work_queue(), m_killthreads(false), m_thread_vec() {
  for (size_t i = 0; i < num_threads; ++i) {
    m_thread_vec.emplace_back(&ThreadPool::ThreadLoop, this);
  }
}

ThreadPool::~ThreadPool() {
  {
    std::scoped_lock<std::mutex> lock(m_mtx);
    m_killthreads = true;
  }
  m_cond.notify_all();
  // jthreads auto-join when m_thread_vec is destroyed
}

// Enqueue a Task for dispatch.
void ThreadPool::Dispatch(Task t) {
  {
    std::scoped_lock<std::mutex> lock(m_mtx);
    m_work_queue.push_back(std::move(t));
  }
  m_cond.notify_one();
}

}  // namespace searchserver
