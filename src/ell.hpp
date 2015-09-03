#pragma once

#include "ell_fwd.hpp"
#include "details/ell_log.hpp"
#include "details/ell_assert.hpp"

namespace ell
{
  namespace details
  {
    class DefaultEventLoop;
  }
  using EventLoop = details::DefaultEventLoop;

  // We have one event loop per thread running at the same time.
  // We a run*() method is called, the event set itself as the current loop
  // for the thread. This allows static helper function to works as intended.
  namespace details
  {
    /**
     * Hides global thread_local variable inside a get/set functions
     */
    EventLoop *set_current_event_loop(EventLoop *loop, bool set = true)
    {
      static thread_local EventLoop *current_loop = nullptr;
      if (set)
        current_loop = loop;
      return current_loop;
    }

    /**
     * Retrieve the current event loop for the current thread.
     */
    EventLoop *get_current_event_loop()
    {
      return set_current_event_loop(nullptr, false);
    }
  }
}

#include "task.hpp"
#include "details/default_event_loop.hpp"

namespace ell
{
  /**
   * Initialize the logging system.
   */
  void initialize_logger()
  {
    auto console = spdlog::stdout_logger_mt("ell_console");
    ELL_ASSERT(console, "Cannot create logger.");
    console->set_level(spdlog::level::debug);
    ELL_DEBUG("Hello {}!", 1);
    ELL_DEBUG("World");
  }

  /**
  * Simply yield, being nice and giving other a chance to run!
  */
  void yield()
  {
    details::get_current_event_loop()->suspend_current_task();
  }

  template <typename Duration>
  void sleep(const Duration &duration)
  {
    details::get_current_event_loop()->sleep_current_task(duration);
  }

  /**
   * Yield to an other callable, waiting for this callable
   * to return in order to continue.
   */
  template <typename Callable>
  auto yield(const Callable &callable) -> decltype(callable())
  {
    auto loop = details::get_current_event_loop();
    return loop->yield(callable);
  }
}