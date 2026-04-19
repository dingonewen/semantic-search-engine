#include <unistd.h>
#include <iostream>

#include "./ThreadPool.hpp"

namespace searchserver {

// This is the main loop that all worker threads are born into.  They
// wait for a signal on the work queue condition variable, then they
// grab work off the queue.  Threads return when they notice that
// m_killthreads is true.
// Every worker thread runs forever until being killed
void ThreadPool::thread_loop() {
  while (true) {
    Task t;
    {
      std::unique_lock<std::mutex> lock(m_mtx);
      m_cond.wait(lock, [this]{
        return m_killthreads || !m_work_queue.empty();
      });  
      // wake up wh m_work_queue is NOT empty or ~ThreadPool() is called
      if (m_killthreads && m_work_queue.empty()) {
        return; // ensure threads finish any remaining tasks before dying 
      }
      t = m_work_queue.front();
      m_work_queue.pop_front();
    } // lock released
    t.func(t.arg);
  } 
}

ThreadPool::ThreadPool(size_t num_threads) : m_mtx(), m_cond(), m_work_queue(), m_killthreads(false), m_thread_vec() {
  // Initialize our member variables.
  // to pass thread_loop to a thread you need to specify the function as ThreadPool::thread_loop
  // and pass the first argument of thread_loop as `this`
  for (size_t i = 0; i < num_threads; ++i) {
    m_thread_vec.emplace_back(&ThreadPool::thread_loop, this);
  }
}

ThreadPool:: ~ThreadPool() {
  {
    std::scoped_lock<std::mutex> lock(m_mtx);
    m_killthreads = true;
  }
  // wake ALL threads so they can see m_killthreads = true
  m_cond.notify_all();  
  // all jthreads auto-join when m_thread_vec is destroyed
}

// Enqueue a Task for dispatch.
void ThreadPool::dispatch(Task t) {
    {
      std::scoped_lock<std::mutex> lock(m_mtx);
      m_work_queue.push_back(t);
    }
    m_cond.notify_one();
  }

}  // namespace searchserver
