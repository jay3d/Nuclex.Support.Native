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

#include "../Source/Threading/Posix/PosixProcessApi.h"

#if !defined(NUCLEX_SUPPORT_WIN32)

#include "../Source/Threading/Posix/PosixFileApi.h"

#include <gtest/gtest.h>

namespace Nuclex { namespace Support { namespace Threading { namespace Posix {

  // ------------------------------------------------------------------------------------------- //

  TEST(PosixProcessApiTest, CanGetFutureTime) {
    struct ::timespec futureTime;
    futureTime = PosixProcessApi::GetTimePlusMilliseconds(
      CLOCK_MONOTONIC, std::chrono::milliseconds(100)
    );

    // Obtain the current time *after* fetching the 'future' time.
    // This way we can check if the tested method really returns a time in the future.
    struct ::timespec currentTime;
    int result = ::clock_gettime(CLOCK_MONOTONIC, &currentTime);
    ASSERT_NE(result, -1);

    bool isFutureTimeInFuture = (
      (futureTime.tv_sec > currentTime.tv_sec) ||
      (
        (futureTime.tv_sec == currentTime.tv_sec) &&
        (futureTime.tv_nsec > currentTime.tv_nsec)
      )
    );
    ASSERT_TRUE(isFutureTimeInFuture);
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(PosixProcessApiTest, CanDetectTimeout) {
    struct ::timespec pastTime;
    int result = ::clock_gettime(CLOCK_MONOTONIC, &pastTime);
    ASSERT_NE(result, -1);

    // Wait until the clock's reported time has changed. Once that happens,
    // the previously queried time is guaranteed to lie in the past.
    for(std::size_t spin = 0; spin < 1000000; ++spin) {
      struct ::timespec currentTime;
      int result = ::clock_gettime(CLOCK_MONOTONIC, &currentTime);
      ASSERT_NE(result, -1);
      if((currentTime.tv_sec != pastTime.tv_sec) ||(currentTime.tv_nsec != pastTime.tv_nsec)) {
        break;
      }
    }

    // Also get a sample of a future point in time for a time point that
    // is guaranteed to not have timed out yet
    struct ::timespec futureTime;
    futureTime = PosixProcessApi::GetTimePlusMilliseconds(
      CLOCK_MONOTONIC, std::chrono::milliseconds(100)
    );

    ASSERT_TRUE(PosixProcessApi::HasTimedOut(CLOCK_MONOTONIC, pastTime));
    ASSERT_FALSE(PosixProcessApi::HasTimedOut(CLOCK_MONOTONIC, futureTime));
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(PosixProcessApiTest, ExecutableIsResolvedInWindowsDirectory) {
    std::string path;
    PosixProcessApi::GetAbsoluteExecutablePath(path, u8"ls");

    EXPECT_GT(path.length(), 5); // shortest possible valid path
    EXPECT_TRUE(PosixFileApi::DoesFileExist(path));
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(PosixProcessApiTest, ExecutableIsResolvedInOwnDirectory) {
    std::string path;
    PosixProcessApi::GetAbsoluteExecutablePath(path, u8"NuclexSupportNativeTests");

    EXPECT_GT(path.length(), 26); // shortest possible valid path
    EXPECT_TRUE(PosixFileApi::DoesFileExist(path));
  }

  // ------------------------------------------------------------------------------------------- //

  TEST(PosixProcessApiTest, RelativeWorkingDirectoryStartsInOwnDirectory) {
    std::string path;
    PosixProcessApi::GetAbsoluteExecutablePath(path, u8"NuclexSupportNativeTests");

    std::string directory;
    PosixProcessApi::GetAbsoluteWorkingDirectory(directory, u8".");

    // The directory may end with a /. since we specified '.' as the target.
    // This isn't required, so we accept both variants. In case the dot is returned,
    // remove it so the path can be compared against the executable path.
    if(directory.length() >= 2) {
      if(directory[directory.length() - 1] == '.') {
        if(directory[directory.length() - 2] == '/') {
          directory.resize(directory.length() - 2);
        } else {
          directory.resize(directory.length() - 1);
        }
      }
    }

    EXPECT_GT(directory.length(), 2); // shortest possible valid path
    EXPECT_NE(path.find(directory), std::string::npos);
  }

  // ------------------------------------------------------------------------------------------- //

}}}} // namespace Nuclex::Support::Threading::Posix

#endif // !defined(NUCLEX_SUPPORT_WIN32)
