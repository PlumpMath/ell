#pragma once

#include <memory>
#include <future>
#include <cassert>
#include <iostream>
#include <boost/coroutine/coroutine.hpp>
#include <boost/pool/pool.hpp>
#include <unordered_set>
#include "ell_fwd.hpp"
#include "valgrind_allocator.hpp"
#include "details/result_holder.hpp"
#include "details/wait_handler.hpp"

namespace ell
{
  namespace details
  {
    /**
     * The class representing a user task from inside the library.
     *
     * This type provides type-erasure so the event loop can deal we
     * various task that do not returns the same thing.
     *
     * @note The constructor is private, and an instance shall be created
     * using the `create()` helper static method. This is to ensure proper
     * setup of the object.
     *
     * @note The task uses std::promise and std::future to store its result.
     * These 2 objects are type erased. The future corresponding to the
     * promise is retrieve once when the task is create.
     */
    class TaskImpl
    {
    public:
      template <typename Callable>
      TaskImpl(const Callable &callable)
          : yield_(nullptr)
          , wait_count_(0)
      {
        setup_coroutine(callable);
      }

      TaskImpl(const TaskImpl &) = delete;
      TaskImpl(TaskImpl &&) = delete;

      TaskImpl &operator=(const TaskImpl &) = delete;
      TaskImpl &operator=(TaskImpl &&) = delete;

      /**
       * Returns the result of the task.
       */
      template <typename T>
      T get_result()
      {
        return result_.get<T>();
      }

      /**
       * Resume the task, effectively calling the coroutine
       * so that it runs.
       */
      void resume()
      {
        // std::cout << "Resuming task !" << std::endl;
        coroutine_();
      }

      void suspend()
      {
        (*yield_)();
      }

      bool is_complete() const
      {
        return !coroutine_;
      }
      /*

            const std::unordered_set<WaitHandler> &wait_list() const
            {
      //        return wait_list_;
            }

            std::unordered_set<WaitHandler> &wait_list()
            {
      //        return wait_list_;
            }
      */

      WaitHandler &wait_handler() const
      {
        return wait_handler_;
      }

    private:
      template <typename Callable>
      auto wrap_user_call(const Callable &callable)
          -> std::enable_if_t<!std::is_same<void, decltype(callable())>::value, void>
      {
        result_.store(callable());
      }

      template <typename Callable>
      auto wrap_user_call(const Callable &callable)
          -> std::enable_if_t<std::is_same<void, decltype(callable())>::value, void>
      {
        callable();
        result_.store();
      }

      /**
       * Create the coroutine object and configure it to run the Callable.
       */
      template <typename Callable>
      void setup_coroutine(const Callable &callable)
      {
        auto attr = boost::coroutines::attributes();
        attr.size = 1024 * 4;

        // We must now setup the boost coroutine object.
        // We will wrap the user callable into a coroutine, adding some
        // code to handles return values, exceptions, and initialization.
        coroutine_ = CoroutineCall(
            [&](CoroutineYield &yield)
            {
              // Perform some initialization task, then yield.

              // Save a pointer to self while the lambda capture has a correct
              // reference to "task".
              TaskImpl *self = this; // While the coroutine is alive, the TaskImpl
                                     // obj is alive too.
              self->yield_ = &yield;
              yield();

              try
              {
                wrap_user_call(callable);
              }
              catch (const std::exception &)
              {
                result_.store_exception(std::current_exception());
              }
            },
            attr, valgrind_stack_allocator());
        // Run the coroutine to perform initialization task.
        coroutine_();
      }

      using CoroutineCall  = boost::coroutines::symmetric_coroutine<void>::call_type;
      using CoroutineYield = boost::coroutines::symmetric_coroutine<void>::yield_type;

      /**
       * The user-code that will be run is wrapped into a Coroutine object.
       */
      CoroutineCall coroutine_;

      /**
       * A pointer to the yield object that the boost coroutine gives us.
       */
      CoroutineYield *yield_;

      ResultHolder result_;

      /**
       * What are we waiting for.
       */
      // std::unordered_set<WaitHandler> wait_list_;

      /**
       * Wait handler for this task.
       */
      mutable WaitHandler wait_handler_;

    public:
      int wait_count_;
    };
  }
}
