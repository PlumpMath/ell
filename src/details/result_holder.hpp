#pragma once

#include <memory>
#include <cassert>
#include <type_traits>

namespace ell
{
  namespace details
  {
    /**
     * An object that store the results of a `Task` using type erasure.
     *
     * A result can either be an exception or a anything else.
     *
     * The user of this class MUST make sure that it queries it correctly.
     * If `store<T>` is called, then `get` must also be called with `T` as
     * template parameter.
     *
     * @note This object use the "Small Object Optimization" and will
     * not make an other call to malloc to the store the concrete result
     * if it can store it.
     *
     * The ResultHolder type is typedef to ResultHolderImpl. This allows
     * easy customization.
     */
    template <std::size_t buffer_size_, std::size_t align_>
    struct ResultHolderImpl
    {
      using Storage             = typename std::aligned_storage<buffer_size_, align_>::type;
      using TypeErasedUniquePtr = std::unique_ptr<uint8_t, void (*)(uint8_t *)>;

      ResultHolderImpl()
          : obj_(nullptr, [](uint8_t *)
                 {
                 })
      {
      }

      /**
       * Store the given object into the result holder.
       * If `T` is passed by value or by reference it will either be moved, or copied
       * into the storage.
       */
      template <typename T>
      void store(T &&obj)
      {
        static_assert(alignof(T) <= align_, "Alignment isn't strong enough.");
        using ConcreteType = std::remove_reference_t<T>;

        if (sizeof(ConcreteType) <= buffer_size_)
        {
          obj_ = TypeErasedUniquePtr(
              reinterpret_cast<uint8_t *>(new (&storage_)
                                              ConcreteType(std::forward<T>(obj))),
              [](uint8_t *ptr)
              {
                reinterpret_cast<ConcreteType *>(ptr)->~ConcreteType();
              });
        }
        else
        {
          auto obj_ptr = new ConcreteType(std::forward<T>(obj));
          obj_ =
              TypeErasedUniquePtr(reinterpret_cast<uint8_t *>(obj_ptr), [](uint8_t *ptr)
                                  {
                                    delete reinterpret_cast<ConcreteType *>(ptr);
                                  });
        }
      }

      /**
       * Store an exception instead of a value.
       */
      void store_exception(std::exception_ptr eptr)
      {
        assert(eptr_ == nullptr);
        eptr_ = eptr;
      }

      /**
       * Return the object stored. Either construct-it by copy or by move.
       */
      template <typename T>
      T get()
      {
        using ConcreteType = std::remove_reference_t<T>;

        if (eptr_)
          std::rethrow_exception(eptr_);

        auto ptr = obj_.get();
        return ConcreteType(
            std::forward<ConcreteType>(*(reinterpret_cast<ConcreteType *>(ptr))));
      }

    private:
      Storage storage_;
      std::exception_ptr eptr_;
      TypeErasedUniquePtr obj_;
    };

    using ResultHolder = ResultHolderImpl<32, 8>;
  }
}