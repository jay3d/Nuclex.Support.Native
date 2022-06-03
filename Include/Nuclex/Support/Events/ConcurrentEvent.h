#pragma region CPL License
/*
Nuclex Native Framework
Copyright (C) 2002-2021 Nuclex Development Labs

This library is free software; you can redistribute it and/or
modify it under the terms of the IBM Common Public License as
published by the IBM Corporation; either version 1.0 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
IBM Common Public License for more details.

You should have received a copy of the IBM Common Public
License along with this library
*/
#pragma endregion // CPL License

#ifndef NUCLEX_SUPPORT_EVENTS_CONCURRENTEVENT_H
#define NUCLEX_SUPPORT_EVENTS_CONCURRENTEVENT_H

#include "Nuclex/Support/Config.h"
#include "Nuclex/Support/Events/Delegate.h"
#include "Nuclex/Support/ScopeGuard.h"

#include <cstdint> // for std::uint8_t
#include <atomic> // for std::atomic
#include <memory> // for std::shared_ptr
#include <mutex> // for std::mutex
#include <algorithm> // for std::copy_n()

namespace Nuclex { namespace Support { namespace Events {

  // ------------------------------------------------------------------------------------------- //

  // Prototype, required for variable argument template
  template<typename> class ConcurrentEvent;

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Manages a list of subscribers that receive callbacks when the event fires</summary>
  /// <typeparam name="TResult">Type of results the callbacks will return</typeparam>
  /// <typeparam name="TArguments">Types of the arguments accepted by the callback</typeparam>
  template<typename TResult, typename... TArguments>
  class ConcurrentEvent<TResult(TArguments...)> {

    /// <summary>Number of subscribers the event can subscribe withou allocating memory</summary>
    /// <remarks>
    ///   To reduce complexity, this value is baked in and not a template argument. It is
    ///   the number of subscriber slots that are baked into the event, enabling it to handle
    ///   a small number of subscribers without allocating heap memory. Each slot takes the size
    ///   of a delegate, 64 bits on a 32 bit system or 128 bits on a 64 bit system.
    /// </remarks>
    private: const static std::size_t BuiltInSubscriberCount = 2;

    /// <summary>Type of value that will be returned by the delegate</summary>
    public: typedef TResult ResultType;
    /// <summary>Method signature for the callbacks notified through this event</summary>
    public: typedef TResult CallType(TArguments...);
    /// <summary>Type of delegate used to call the event's subscribers</summary>
    public: typedef Delegate<TResult(TArguments...)> DelegateType;

    #pragma region struct SharedMutex

    /// <summary>Queue of subscribers to which the event will be broadcast</summary>
    private: struct SharedMutex {

      /// <summary>Initializes a new shared mutex</summary>
      public: SharedMutex() :
        Mutex(),
        ReferenceCount(1) {}

      /// <summary>Mutex that is shared between multiple owners</summary>
      public: std::mutex Mutex;
      /// <summary>Number of references to this instance of shared mutex<</summary>
      public: std::atomic<std::size_t> ReferenceCount;

    };

    #pragma endregion // struct SharedMutex

    /// <summary>Initializes a new concurrent event</summary>
    public: ConcurrentEvent() = default;

    /// <summary>Subscribes the specified free function to the event</summary>
    /// <typeparam name="TMethod">Free function that will be subscribed</typeparam>
    public: template<TResult(*TMethod)(TArguments...)>
    void Subscribe() {
      Subscribe(DelegateType::template Create<TMethod>());
    }

    /// <summary>Subscribes the specified object method to the event</summary>
    /// <typeparam name="TClass">Class the object method is a member of</typeparam>
    /// <typeparam name="TMethod">Object method that will be subscribed to the event</typeparam>
    /// <param name="instance">Instance on which the object method will be called</param>
    public: template<typename TClass, TResult(TClass::*TMethod)(TArguments...)>
    void Subscribe(TClass *instance) {
      Subscribe(DelegateType::template Create<TClass, TMethod>(instance));
    }

    /// <summary>Subscribes the specified const object method to the event</summary>
    /// <typeparam name="TClass">Class the object method is a member of</typeparam>
    /// <typeparam name="TMethod">Object method that will be subscribed to the event</typeparam>
    /// <param name="instance">Instance on which the object method will be called</param>
    public: template<typename TClass, TResult(TClass::*TMethod)(TArguments...) const>
    void Subscribe(const TClass *instance) {
      Subscribe(DelegateType::template Create<TClass, TMethod>(instance));
    }

    /// <summary>Subscribes the specified delegate to the event</summary>
    /// <param name="delegate">Delegate that will be subscribed</param>
    public: void Subscribe(const DelegateType &delegate) {
      std::shared_ptr<SharedMutex> currentEditMutex = acquireMutex();
      ON_SCOPE_EXIT {
        releaseMutex(currentEditMutex);
      };
      {
        std::lock_guard<std::mutex> mutexLockScope(currentEditMutex->Mutex);

        // Build a new broadcast list with the new subscriber appended to the end
        {
          std::shared_ptr<BroadcastQueue> newQueue;

          // We either have to create an entirely new list of subscribers or
          // clone an existing subscriber list while adding one entry to the end.
          if(!this->subscribers) {
            newQueue = std::make_shared<BroadcastQueue>(1);
            new(newQueue->Subscribers) DelegateType(delegate);
          } else { // Non-empty subscriber list present, create clone with extra entry
            std::size_t currentSubscriberCount = this->subscribers->SubscriberCount;
            newQueue = std::make_shared<BroadcastQueue>(currentSubscriberCount + 1);
            for(std::size_t index = 0; index < currentSubscriberCount; ++index) {
              new(newQueue->Subscribers + index) DelegateType(this->subscribers->Subscribers[index]);
            }
            new(newQueue->Subscribers + currentSubscriberCount) DelegateType(delegate);
          }

          // This might be a little more efficient if I drop the const BroadcastQueue stuff.
          // And isn't there an std::atomic_store for std::shared_ptr with move semantics?
          std::atomic_store(&this->subscribers, std::shared_ptr<const BroadcastQueue>(newQueue));
        } // Queue replacement beauty scope
      } // edit mutex lock scope
    }

    /// <summary>Unsubscribes the specified free function from the event</summary>
    /// <typeparam name="TMethod">
    ///   Free function that will be unsubscribed from the event
    /// </typeparam>
    /// <returns>True if the object method was subscribed and has been unsubscribed</returns>
    public: template<TResult(*TMethod)(TArguments...)>
    bool Unsubscribe() {
      return Unsubscribe(DelegateType::template Create<TMethod>());
    }

    /// <summary>Unsubscribes the specified object method from the event</summary>
    /// <typeparam name="TClass">Class the object method is a member of</typeparam>
    /// <typeparam name="TMethod">
    ///   Object method that will be unsubscribes from the event
    /// </typeparam>
    /// <param name="instance">Instance on which the object method was subscribed</param>
    /// <returns>True if the object method was subscribed and has been unsubscribed</returns>
    public: template<typename TClass, TResult(TClass::*TMethod)(TArguments...)>
    bool Unsubscribe(TClass *instance) {
      return Unsubscribe(DelegateType::template Create<TClass, TMethod>(instance));
    }

    /// <summary>Unsubscribes the specified object method from the event</summary>
    /// <typeparam name="TClass">Class the object method is a member of</typeparam>
    /// <typeparam name="TMethod">
    ///   Object method that will be unsubscribes from the event
    /// </typeparam>
    /// <param name="instance">Instance on which the object method was subscribed</param>
    /// <returns>True if the object method was subscribed and has been unsubscribed</returns>
    public: template<typename TClass, TResult(TClass::*TMethod)(TArguments...) const>
    bool Unsubscribe(const TClass *instance) {
      return Unsubscribe(DelegateType::template Create<TClass, TMethod>(instance));
    }

    /// <summary>Unsubscribes the specified delegate from the event</summary>
    /// <param name="delegate">Delegate that will be unsubscribed</param>
    /// <returns>True if the callback was found and unsubscribed, false otherwise</returns>
    public: bool Unsubscribe(const DelegateType &delegate) {
      std::shared_ptr<SharedMutex> currentEditMutex = acquireMutex();
      ON_SCOPE_EXIT {
        releaseMutex(currentEditMutex);
      };
      {
        std::lock_guard<std::mutex> mutexLockScope(currentEditMutex->Mutex);
        {
          if(!this->subcribers) {
            return false; // There were no subscribers...
          }

          std::size_t oldSubscriberCount = this->subscribers->SubscriberCount;
          std::shared_ptr<BroadcastQueue> newQueue = std::make_shared<BroadcastQueue>(
            oldSubscriberCount - 1
          );

          DelegateType *oldSubscribers = this->subscribers->Subscribers;
          DelegateType *newSubscribers = newQueue->Subscribers;
          while(oldSubscriberCount > 0) {
            if(*oldSubscribers == delegate) {
              ++oldSubscribers;
              std::copy_n(oldSubscribers, oldSubscriberCount - 1, newSubscribers);

              // This might be a little more efficient if I drop the const BroadcastQueue stuff.
              // And isn't there an std::atomic_store for std::shared_ptr with move semantics?
              std::atomic_store(&this->subscribers, std::shared_ptr<const BroadcastQueue>(newQueue));

              return true;
            }

            *newSubscribers = *oldSubscribers;
            ++oldSubscribers;
            ++newSubscribers;
            --oldSubscriberCount;
          }

          // We didn't find the subscriber that was to be unsubscribes, so we also didn't
          // edit the subscriber list and there is not need to replace or change anything.
          return false;

        } // Queue replacement beauty scope
      } // edit mutex lock scope
    }

    #pragma region struct BroadcastQueue

    /// <summary>Queue of subscribers to which the event will be broadcast</summary>
    private: struct BroadcastQueue {

      /// <summary>
      ///   Initializes a new broadcast queue for the specified number of subscribers
      /// </summary>
      /// <param name="count">
      ///   Number of subscribers the broadcast queue will be initialized for
      /// </param>
      public: BroadcastQueue(std::size_t count) :
        Subscribers(allocateUninitializedDelegates(count)),
        SubscriberCount(count) {}

      /// <summary>Frees all memory owned by the broadcast queue</summary>
      public: ~BroadcastQueue() {
        // We do not call the destructors of the delegates here because we know they're
        // trivially constructible. This is only the case because delegates do not support
        // lambdas which would have to capture arbitrary types with possible destructors.
        freeDelegatesWithoutDestructor(this->Subscribers);
      }

      // Ensure the queue isn't copied or moved with default semantics
      BroadcastQueue(const BroadcastQueue &other) = delete;
      BroadcastQueue(BroadcastQueue &&other) = delete;
      void operator =(const BroadcastQueue &other) = delete;
      void operator =(BroadcastQueue &&other) = delete;

      /// <summary>Allocates an memory for delegates without calling their constructor</summary>
      /// <param name="count">Number of delegates to allocate memory for</param>
      /// <returns>A pointer to the first allocated delegate in the array</returns>
      private: static DelegateType *allocateUninitializedDelegates(std::size_t count) {
        return reinterpret_cast<DelegateType *>(
          new std::uint8_t *[sizeof(DelegateType[2]) * count / 2]
        );
      }

      /// <summary>Frees an array of delegates without calling any destructors</summary>
      /// <param name="delegates">Delegate array that will be freed</param>
      private: static void freeDelegatesWithoutDestructor(DelegateType *delegates) {
        delete[] reinterpret_cast<std::uint8_t *>(delegates);
      }

      /// <summary>Plain array of all subscribers to which the event is broadcast</summary>
      public: DelegateType *Subscribers;
      /// <summary>Number of subscribers stored in the array</summary>
      public: std::size_t SubscriberCount;

    };

    #pragma endregion // struct BroadcastQueue

    /// <summary>Acquires the edit mutex held while editing the broadcast queue</summary>
    /// <returns>The current edit mutex</returns>
    /// <remarks>
    ///   This goes through some hoops to ensure the mutex only exists while the broadcast
    ///   queue is being edited, while also ensuring that if a mutex exist, only one exists
    ///   and is shared by all threads competing to edit the broadcast queue.
    /// </remarks>
    private: std::shared_ptr<SharedMutex> acquireMutex() {
      std::shared_ptr<SharedMutex> currentEditMutex = std::atomic_load_explicit(
        &this->editMutex, std::memory_order::memory_order_consume // if() carries dependency
      );
      std::shared_ptr<SharedMutex> newEditMutex; // Leave as nullptr until we know we need it
      for(;;) {

        // If we got a shared mutex instance, it may still be on the brink of being abandoned
        // (once the reference counter goes to 0, it must not be revived to avoid a race)
        if(unlikely(static_cast<bool>(currentEditMutex))) {
          std::size_t knownReferenceCount = currentEditMutex->ReferenceCount.load(
            std::memory_order::memory_order_consume
          );

          // Was the mutex just now still occupied by another thread? If so, attempt to
          // increment its reference count one further via a C-A-S operation.
          while(likely(knownReferenceCount >= 1)) {
            bool wasExchanged = currentEditMutex->ReferenceCount.compare_exchange_weak(
              knownReferenceCount, knownReferenceCount + 1
            );
            if(likely(wasExchanged)) {
              return currentEditMutex; // We managed to increment the reference count above 1
            }
          }

          // The shared mutex for which we got the reference count was about to be dropped
          currentEditMutex.reset(); // Assume the other thread has already dropped it by now
        }

        // Either there was no existing mutex or it was about to be dropped,
        // so attempt to insert our own, fresh mutex with a reference count of 1.
        if(!newEditMutex) {
          newEditMutex = std::make_shared<SharedMutex>();
        }
        bool wasExchanged = std::atomic_compare_exchange_strong(
          &this->editMutex, // pointer that will be replaced
          &currentEditMutex, // expected prior mutex, receives current on failure
          newEditMutex // new mutex to assign if previous was empty
        );
        if(likely(wasExchanged)) {
          return newEditMutex; // We got our new mutex in with 1 reference count held by us
        }

      } // for(;;) checking loop for C-A-S until a mutex is in place
    }

    /// <summary>Releases the shared mutex again, potentially dropping it entirely</summary>
    /// <param name="currentEditMutex">Shared mutex as returned by the acquire method</param>
    private: void releaseMutex(const std::shared_ptr<SharedMutex> &currentEditMutex) {
      std::size_t previousReferenceCount = currentEditMutex->ReferenceCount.fetch_sub(
        1, std::memory_order::memory_order_seq_cst
      );

      // If we just decremented the reference counter to zero, drop the shared mutex.
      // This would be a race condition for a naively implemented acquireMutex() method,
      // but we sidestep this by making acquireMutex() C-A-S the reference count for
      // the uptick operation and consider the whole whole shared mutex dead when seeing
      // it at a reference count of zero.
      if(likely(previousReferenceCount == 1)) {
        std::atomic_store(&this->editMutex, std::shared_ptr<SharedMutex>());
      }
    }

    /// <summary>Stores the current subscribers to the event</summary>
    public: /* atomic */ std::shared_ptr<const BroadcastQueue> subscribers;
    /// <summary>Will be present while subscriptions/unsubscriptions happen</summary>
    public: /* atomic */ std::shared_ptr<SharedMutex> editMutex;

  };

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Events

#endif // NUCLEX_SUPPORT_EVENTS_CONCURRENTEVENT_H
