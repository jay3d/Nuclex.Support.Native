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

#include "Nuclex/Support/Threading/Process.h"

#include <gtest/gtest.h>

#include <stdexcept> // for std::logic_error

// An executable that is in the default search path, has an exit code of 0,
// does not need super user privileges and does nothing bad when run.
#if defined(NUCLEX_SUPPORT_WIN32)
  #define NUCLEX_SUPPORT_HARMLESS_EXECUTABLE u8"hostname.exe"
#else
  #define NUCLEX_SUPPORT_HARMLESS_EXECUTABLE u8"ls"
#endif

namespace Nuclex { namespace Support { namespace Threading {

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, InstancesCanBeCreated) {
    EXPECT_NO_THROW(
      Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, UnstartedProcessIsNotRunning) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);
    EXPECT_FALSE(test.IsRunning());
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, WaitingOnUnstartedProcessCausesException) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);
    EXPECT_THROW(
      test.Wait(),
      std::logic_error
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, JoiningUnstartedProcessCausesException) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);
    EXPECT_THROW(
      test.Join(),
      std::logic_error
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, ProcessCanBeStarted) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);

    test.Start();

    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, JoinAfterWaitIsLegal) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);

    test.Start();
    EXPECT_TRUE(test.Wait());

    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, WaitAfterJoinCausesException) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);

    test.Start();
    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);

    EXPECT_THROW(
      test.Wait(),
      std::logic_error
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, DoubleJoinThrowsException) {
    Process test(NUCLEX_SUPPORT_HARMLESS_EXECUTABLE);

    test.Start();
    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);

    EXPECT_THROW(
      test.Join(),
      std::logic_error
    );
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(ProcessTest, CanTellIfProcessIsStillRunning) {
#if defined(NUCLEX_SUPPORT_WIN32)
    Process test(u8"cmd.exe");
    test.Start({u8"/c sleep 1"});
#else
    Process test(u8"sleep");
    test.Start({u8"0.25"});
#endif

    EXPECT_TRUE(test.IsRunning());
    EXPECT_TRUE(test.IsRunning());

    EXPECT_TRUE(test.Wait());

    EXPECT_FALSE(test.IsRunning());
    EXPECT_FALSE(test.IsRunning());

    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);

    EXPECT_FALSE(test.IsRunning());
    EXPECT_FALSE(test.IsRunning());
  }

  // ------------------------------------------------------------------------------------------- //

  class Observer {
    public: void AcceptStdOut(const char *characters, std::size_t count) {
      this->output.append(characters, count);
    }

    public: std::string output;
  };

  TEST(ProcessTest, CanCaptureStdout) {
    Observer observer;

#if defined(NUCLEX_SUPPORT_WIN32)
    Process test(u8"cmd.exe");
    test.StdOut.Subscribe<Observer, &Observer::AcceptStdOut>(&observer);
    test.Start({ u8"/c", "dir", "/b" });
#else
    Process test(u8"ls");
    test.StdOut.Subscribe<Observer, &Observer::AcceptStdOut>(&observer);
    test.Start({ u8"-l" });
#endif

    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 0);

    // Check for some directories that should have been listed by ls / dir
    EXPECT_GE(observer.output.length(), 21);
    //EXPECT_NE(observer.output.find(u8".."), std::string::npos);
  }

  // ------------------------------------------------------------------------------------------- //
#if defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  TEST(ProcessTest, ChildSegmentationFaultCausesExceptionInJoin) {
    Process test("./segfault");

    test.Start();
    EXPECT_THROW(
      test.Join(),
      std::runtime_error
    );
  }
#endif // defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  // ------------------------------------------------------------------------------------------- //
#if defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  TEST(ProcessTest, ExitCodeIsCapturedByJoin) {
    Process test("./badexit");

    test.Start();
    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 1);
  }
#endif // defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  // ------------------------------------------------------------------------------------------- //
#if defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  TEST(ProcessTest, ExitCodeIsCapturedByWait) {
    Process test("./badexit");

    test.Start();
    test.Wait(); // Wait reaps the zombie process here on Linux systems
    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 1);
  }
#endif // defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  // ------------------------------------------------------------------------------------------- //
#if defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  TEST(ProcessTest, ExitCodeIsCapturedByisRunning) {
    Process test("./badexit");

    test.Start();
    while(test.IsRunning()) {
      ;
    }
    int exitCode = test.Join();
    EXPECT_EQ(exitCode, 1);
  }
#endif // defined(NUCLEX_SUPPORT_HAVE_TEST_EXECUTABLES)
  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Threading
