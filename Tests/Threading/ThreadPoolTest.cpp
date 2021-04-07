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

// If the library is compiled as a DLL, this ensures symbols are exported
#define NUCLEX_SUPPORT_SOURCE 1

#include "Nuclex/Support/Threading/ThreadPool.h"
#include "Nuclex/Support/Threading/Thread.h"

#include <memory> // for std::unique_ptr

#include <gtest/gtest.h>

namespace {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Performs a simple calculation, used to test the thread pool</summary>
  /// <param name="a">First input for the useless formula</param>
  /// <param name="b">Second input for the useless formula</param>
  /// <returns>The result of the formula ab - a - b</returns>
  int testMethod(int a, int b) {
    return a * b - (a + b);
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Method that is simply slow to execute</summary>
  void slowMethod() {
    Nuclex::Support::Threading::Thread::Sleep(std::chrono::milliseconds(100));
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Method that fails with an exception</summary>
  int failingMethod() {
    throw std::underflow_error(u8"Hur dur, I'm an underflow error");
  }

  // ------------------------------------------------------------------------------------------- //

} // anonymous namespace

namespace Nuclex { namespace Support { namespace Threading {

  // ------------------------------------------------------------------------------------------- //

  TEST(ThreadPoolTest, HasDefaultConstructor) {
    EXPECT_NO_THROW(
      ThreadPool testPool;
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ThreadPoolTest, CanScheduleTasks) {
    ThreadPool testPool;

    // Schedule a task to run on a thread pool thread
    std::future<int> future = testPool.AddTask(&testMethod, 12, 34);

    // The future should immediately be valid and usable to chain calls and wait upon
    EXPECT_TRUE(future.valid());

    // Wait for the task to execute on the thread pool, filling the future
    int result = future.get();
    EXPECT_EQ(result, 362);

    // The thread pool is cleanly shut down as it goes out of scope
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ThreadPoolTest, ThreadPoolShutdownCancelsTasks) {
    std::unique_ptr<ThreadPool> testPool = std::make_unique<ThreadPool>(1, 1);

    // Add a slow task and our detector task. This thread pool only has
    // one thread, so the slow task will block the worker thread for 100 ms.
    testPool->AddTask(&slowMethod);
    std::future<int> canceledFuture = testPool->AddTask(&testMethod, 12, 34);

    EXPECT_TRUE(canceledFuture.valid());

    // Now we destroy the thread pool. All outstanding tasks will be destroyed,
    // canceling their returned std::futures without proving a result.
    testPool.reset();

    // An attempt to obtain the result from the canceled future should now
    // result in an std::future_error with the broken_promise error code.
    EXPECT_THROW(
      {
        int result = canceledFuture.get();
        (void)result;
      },
      std::future_error
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ThreadPoolTest, ExceptionInCallbackPropagatesToStdFuture) {
    ThreadPool testPool;

    // Schedule a task to run on a thread pool thread
    std::future<int> failedFuture = testPool.AddTask(&failingMethod);

    EXPECT_THROW(
      {
        int result = failedFuture.get();
        (void)result;
      },
      std::underflow_error
    );

    // The thread pool is cleanly shut down as it goes out of scope
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ThreadPoolTest, StressTestCompletes) {
    for(std::size_t repetition = 0; repetition < 10; ++repetition) {
      std::unique_ptr<ThreadPool> testPool = std::make_unique<ThreadPool>(1, 1);

      for(std::size_t task = 0; task < 1000; ++task) {
        testPool->AddTask(&testMethod, 12, 34);
      }

      testPool.reset();
    }
  }

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Threading
