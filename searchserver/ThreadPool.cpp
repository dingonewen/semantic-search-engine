#include <unistd.h>
#include <iostream>

#include "./ThreadPool.hpp"

namespace searchserver {

// This is the main loop that all worker threads are born into.  They
// wait for a signal on the work queue condition variable, then they
// grab work off the queue.  Threads return when they notice that
// m_killthreads is true.
void ThreadPool::thread_loop() {

}

ThreadPool::ThreadPool(size_t num_threads) : m_thread_vec(), m_mtx(), m_cond(), m_work_queue(), m_killthreads(false) {

  // Initialize our member variables.

  // TODO
  // to pass thread_loop to a thread you need to specify the function as ThreadPool::thread_loop
  // and pass the first argument as `this`
}

ThreadPool:: ~ThreadPool() {
  
}

// Enqueue a Task for dispatch.
void ThreadPool::dispatch(Task t) {
  // TODO
}

}  // namespace searchserver
