#include <unistd.h>

#include "./ThreadPool.hpp"
#include "./catch.hpp"

// if you're just incrementing/reading a single integer, use atomic. 
// If you're protecting a larger data structure (like a std::string or std::deque), use mutex.
#include <mutex>
#include <atomic> // avoid data race for counter

using searchserver::ThreadPool;


uint32_t workcount = 0;
static std::mutex mtx;

// This is the function that each dispatched thread from the thread
// pool is sent to execute.
void TestTaskFn([[maybe_unused]] void* arg) {
  uint32_t local = 0;
  {
    std::scoped_lock sl(mtx);
    workcount++;
    local = workcount;
  }

  if (local % 5 == 1) {
    usleep(250000);  // 0.25s
  }
}

TEST_CASE("Basic", "[Test_ThreadPool]") {
  ThreadPool *tp = new ThreadPool(10);

  // Try dispatching some work.  Make sure we dispatch enough that
  // there will be a queue of pending tasks in the threadpool, so
  // that we can test the "delete before all tasks are done" case.
  for (int i = 0; i < 300; i++) {
    ThreadPool::Task next_t = {TestTaskFn, nullptr};

    tp->Dispatch(next_t);
  }
  usleep(1250000);  // 1.25s

  // Make sure that there are still tasks pending.
  mtx.lock();
  REQUIRE(static_cast<uint32_t>(300) > workcount);
  mtx.unlock();

  // Kill off the threadpool, which should force the rest of the
  // pending tasks to be finished serially.
  delete tp;

  // Make sure all 300 tasks finished successfully.
  REQUIRE(static_cast<uint32_t>(300) == workcount);
}

static std::string str;
static std::mutex lock;

struct ConcurrentTaskArg {
  char message;
  int secs;
};

void TestConcurrentTaskFn(void* arg) {
  auto taskarg = reinterpret_cast<ConcurrentTaskArg*>(arg);
  if (taskarg->secs > 0) {
    sleep(taskarg->secs);
  }

  lock.lock();
  str += taskarg->message;
  lock.unlock();

  delete taskarg;
}


TEST_CASE("Concurrent", "[Test_ThreadPool]") {
  ThreadPool *tp = new ThreadPool(2);

  ConcurrentTaskArg* arg0 = new ConcurrentTaskArg();
  arg0->message = 'A';
  arg0->secs    =  0;

  ConcurrentTaskArg* arg1 = new ConcurrentTaskArg();
  arg1->message = 'i';
  arg1->secs    =  4;

  ConcurrentTaskArg* arg2 = new ConcurrentTaskArg();
  arg2->message = 'o';
  arg2->secs    =  2;

  ThreadPool::Task task0 = {TestConcurrentTaskFn, arg0};
  ThreadPool::Task task1 = {TestConcurrentTaskFn, arg1};
  ThreadPool::Task task2 = {TestConcurrentTaskFn, arg2};

  tp->Dispatch(task0);
  tp->Dispatch(task1);
  tp->Dispatch(task2);
  usleep(4250000);  // 4.25s

  int tries = 0;
  for ( ; tries < 5; ++tries) {
    size_t len;
    lock.lock();
    len = str.size();
    lock.unlock();

    if (len == 3) {
      break;
    }
    sleep(1);
  }

  // The threads should have finished
  // but they did not
  REQUIRE(tries < 5);

  delete tp;

  // Make sure characters were added in expected order.
  REQUIRE(str == "Aoi");

  // added to the threadpool in order of Aio
  // but task that ads i should have slept long enough
  // that the o task finishes first
}

// add dispatch test case
TEST_CASE("Dispatch", "[Test_ThreadPool]") {
  using searchserver::ThreadPool;
  std::atomic<int> counter{0};

  ThreadPool tp(4);

  for (int i = 0; i < 10; ++i) {
    ThreadPool::Task t = {[](void* arg) {
      reinterpret_cast<std::atomic<int>*>(arg)->fetch_add(1);
    }, &counter};
    tp.Dispatch(t);
  }

  usleep(500000);  // 0.5s — give threads time to finish
  REQUIRE(counter == 10);
}