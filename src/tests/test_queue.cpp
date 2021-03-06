#include <iostream>
#include <gtest/gtest.h>
#include "ell.hpp"
#include "queue.hpp"

// Call googletest assert macro.
// This is need because in some lambda we cannot use
// googletest macros. Dont' really know why, but this worked
// fine as a workaround.
// Usage is : GASSERT(EQUAL(42, 42));
#define GASSERT(msg)                                                                     \
  [&]()                                                                                  \
  {                                                                                      \
    ASSERT_##msg;                                                                        \
  }();


TEST(test_queue, simple_push_pop)
{
  using namespace std::chrono;

  ell::EventLoop loop;
  ell::Queue<int> queue;
  auto start = system_clock::now();

  auto pusher = [&]() -> void
  {
    ell::sleep(std::chrono::milliseconds(1500));
    queue.push(42);
    queue.push(21);
  };

  auto popper = [&]() -> int
  {
    int v1 = queue.pop();
    GASSERT(EQ(42, v1));

    // We should have waited for 2.5 second.
    auto end     = system_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    GASSERT(GE(elapsed.count(), 1500));

    // Second pop should be instant.
    int v2 = queue.pop();
    GASSERT(EQ(21, v2));

    auto end2 = system_clock::now();
    elapsed = duration_cast<milliseconds>(end2 - end);
    GASSERT(LE(elapsed.count(), 5)); // We allow ~5ms if CPU is slow.

    return v1;
  };

  auto pusher_task = loop.call_soon(pusher);
  auto pop_task    = loop.call_soon(popper);

  // The coroutine popper will wait until an `int` becomes
  // available in the queue.
  loop.run_until_complete(pop_task);

  ASSERT_EQ(42, pop_task->get_result());
}

TEST(test_queue, try_pop)
{
  using namespace std::chrono;

  ell::EventLoop loop;
  ell::Queue<int> queue;
  auto start = system_clock::now();

  auto pusher = [&]() -> void
  {
    ell::sleep(std::chrono::milliseconds(1500));
    queue.push(42);
    queue.push(21);
  };

  auto popper = [&]() -> int
  {
    // Since the pusher will wait for 2.5 seconds, the first
    // try_pop shall fail.
    int ret = -1;
    GASSERT(FALSE(queue.try_pop(ret)));

    // This one shall pause until an item because available.
    int v1 = queue.pop();
    GASSERT(EQ(42, v1));

    // We should have waited for 2.5 second.
    auto end     = system_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    GASSERT(GE(elapsed.count(), 1500));

    // try_pop will succeed for the second item.
    GASSERT(TRUE(queue.try_pop(ret)));
    GASSERT(EQ(21, ret));

    auto end2 = system_clock::now();
    elapsed = duration_cast<milliseconds>(end2 - end);
    GASSERT(LE(elapsed.count(), 5)); // We allow ~5ms if CPU is slow.

    return v1;
  };

  auto pusher_task = loop.call_soon(pusher);
  auto pop_task    = loop.call_soon(popper);

  loop.run_until_complete(pop_task);
}

TEST(test_queue, fized_size_queue)
{
  using namespace std::chrono;

  ell::EventLoop loop;
  ell::Queue<int> queue(10); // maxsize
  auto start = system_clock::now();

  // Fully populate the queue.
  for (int i = 0; i < 10; i++)
    queue.push(i);

  auto pusher = [&]()
  {
    // The queue is full. We shall wait.
    queue.push(42);

    // We should have waited for 1.5 second.
    auto end     = system_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    GASSERT(GE(elapsed.count(), 1500));
  };

  auto popper = [&]()
  {
    ell::sleep(std::chrono::milliseconds(1500));
    for (int i = 0; i < 10; ++i)
      queue.pop();

    int last = queue.pop();
    GASSERT(EQ(42, last));
  };

  auto pusher_task = loop.call_soon(pusher);
  auto pop_task    = loop.call_soon(popper);

  loop.run_until_complete(pop_task);
}

TEST(test_queue, test_try_push)
{
  using namespace std::chrono;

  ell::EventLoop loop;
  ell::Queue<int> queue(10); // maxsize
  auto start = system_clock::now();

  // Fully populate the queue.
  for (int i = 0; i < 10; i++)
    queue.push(i);

  auto pusher = [&]()
  {
    // The queue is full.
    ASSERT_FALSE(queue.try_push(42));
    queue.push(1337);

    // coroutine `popper` popped 11 items. We now have space.
    ASSERT_TRUE(queue.try_push(42));

    // We should have waited for 1.5 second.
    auto end     = system_clock::now();
    auto elapsed = duration_cast<milliseconds>(end - start);
    GASSERT(GE(elapsed.count(), 1500));
  };

  auto popper = [&]()
  {
    ell::sleep(std::chrono::milliseconds(1500));
    for (int i = 0; i < 10; ++i)
      queue.pop();

    int item = queue.pop();
    ASSERT_EQ(1337, item);
    item = queue.pop();
    ASSERT_EQ(42, item);
  };

  auto pusher_task = loop.call_soon(pusher);
  auto pop_task    = loop.call_soon(popper);

  loop.run_until_complete(pop_task);
}

int main(int ac, char **av)
{
  ell::initialize_logger();
  ::testing::InitGoogleTest(&ac, av);
  return RUN_ALL_TESTS();
}
