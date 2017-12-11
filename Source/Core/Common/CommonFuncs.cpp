// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// The code in GetErrorMessage can't handle some systems having the
// GNU version of strerror_r and other systems having the XSI version,
// so we undefine _GNU_SOURCE here in an attempt to always get the XSI version.
// We include cstring before all other headers in case cstring is included
// indirectly (without undefining _GNU_SOURCE) by some other header.
#ifdef _GNU_SOURCE
#undef _GNU_SOURCE
#include <cstring>
#define _GNU_SOURCE
#else
#include <cstring>
#endif

#include <cstddef>
#include <errno.h>

#include "Common/CommonFuncs.h"

#ifdef _WIN32
#include <windows.h>
#define strerror_r(err, buf, len) strerror_s(buf, len, err)
#endif

constexpr size_t BUFFER_SIZE = 256;

// Only one of the handleStrerror functions will be used, so make 'inline' to avoid unused
// function warnings

inline std::string handleStrerror(int ret, char error_message[])
{
  if (ret)
    return "";
  return std::string(error_message);
}

inline std::string handleStrerror(const char* ret, char error_message[])
{
  if (ret)
    return std::string(ret);
  return "";
}

// Wrapper function to get last strerror(errno) string.
// This function might change the error code.
std::string LastStrerrorString()
{
  char error_message[BUFFER_SIZE];

  // The GNU version returns char*, which "may" be a pointer to the provided buffer or some
  // system-managed const buffer. The returned buffer is always null-terminated, but may be
  // truncated to fit.
  // The XSI version return int, which is 0 on success, and the provided buffer will be
  // filled with the string, otherwise it returns an error code
  auto result = strerror_r(errno, error_message, BUFFER_SIZE);
  return handleStrerror(result, error_message);
}

#ifdef _WIN32
// Wrapper function to get GetLastError() string.
// This function might change the error code.
std::string GetLastErrorString()
{
  char error_message[BUFFER_SIZE];

  FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error_message, BUFFER_SIZE, nullptr);
  return std::string(error_message);
}
#endif
