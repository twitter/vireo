/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

enum ErrorCategory {
  NoError = 0,                // CAUTION: DON'T USE WHEN THROWING EXCEPTIONS!
  ImageCore = 1,              // ImageCore raised an exception
  InternalInconsistency = 2,  // assertion fails, used by CHECK(...)
  Invalid = 3,                // invalid data (e.g. video with corrupt header etc.)
  InvalidArguments = 4,       // invalid arguments, both for internal and 3rdparty functions
  ReaderError = 5,            // an error occurred during abstract method call Reader.read()
  OutOfMemory = 6,            // memory allocation fail
  OutOfRange = 7,             // trying to access non-existing indexed elements
  Overflow = 8,               // a math operation resulted in overflow
  Uninitialized = 9,          // state of an object is uninitialized
  Unsafe = 10,                // due to enforced security limits
  Unsupported = 11,           // unsupported data (e.g. unsupported video codec)
  MissingDependency = 12,     // built without required library
};

const static char* kErrorCategoryToString[] = {
  "no error",
  "imagecore",
  "internal inconsistency",
  "invalid",
  "invalid arguments",
  "reader error",
  "out of memory",
  "out of range",
  "overflow",
  "uninitialized",
  "unsafe",
  "unsupported",
  "missing dependency",
};

const static char* kErrorCategoryToGenericReason[] = {
  "no error",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "file is invalid",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "unexpected error, please report back",
  "file is currently unsupported",
  "file is currently unsupported",
  "built without the library required",
};

#ifndef __EXCEPTIONS
#include <stdlib.h>  // For exit
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#include "vireo/base_h.h"

#ifdef ANDROID
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "vireo", __VA_ARGS__)
#endif

#define GET_OVERLOADED_MACRO_NAME_MAX3(_1, _2, _3, NAME, ...) NAME

// Define THROW(...)
#ifdef __EXCEPTIONS
#define THROW(FILE, FUNCTION, LINE, CONDITION, EXCEPTION, REASON)\
  throw error::Exception(FILE, FUNCTION, std::to_string(LINE).c_str(), CONDITION, kErrorCategoryToString[EXCEPTION], REASON);
#else
#ifdef ANDROID
#define THROW(FILE, FUNCTION, LINE, CONDITION, EXCEPTION, REASON)\
LOGD("[%s (%d)] \"%s\" condition failed; reason: %s", FILE, LINE, CONDITION, REASON);\
exit(EXCEPTION);
#else
#define THROW(FILE, FUNCTION, LINE, CONDITION, EXCEPTION, REASON)\
exit(EXCEPTION);
#endif
#endif

// define THROW_IF with optional REASON_STREAM field
#define THROW_IF(...) GET_OVERLOADED_MACRO_NAME_MAX3(__VA_ARGS__, THROW_IF3, THROW_IF2)(__VA_ARGS__)

#define THROW_IF2(CONDITION, EXCEPTION) THROW_IF3(CONDITION, EXCEPTION, kErrorCategoryToGenericReason[EXCEPTION])
#ifdef ANDROID
#define THROW_IF3(CONDITION, EXCEPTION, REASON_STREAM) if (CONDITION) {\
  THROW(__FILE__, __FUNCTION__, __LINE__, #CONDITION, EXCEPTION, kErrorCategoryToGenericReason[EXCEPTION]);\
}
#else
#define THROW_IF3(CONDITION, EXCEPTION, REASON_STREAM) if (CONDITION) {\
  std::stringstream reason; reason << REASON_STREAM;\
  THROW(__FILE__, __FUNCTION__, __LINE__, #CONDITION, EXCEPTION, reason.str().c_str());\
}
#endif

// define CHECK and DCHECK - use only for assertions
#ifdef CHECK
#undef CHECK
#endif
#define CHECK(CONDITION) THROW_IF(!(CONDITION), InternalInconsistency)
#ifndef NDEBUG
#define DCHECK(CONDITION) CHECK(CONDITION)
#else
#define DCHECK(CONDITION)
#endif

// define RETURN_IF and RETURN_IF_FALSE
#ifdef ANDROID
#define RETURN_IF(CONDITION, EXCEPTION) if((CONDITION)) {\
  return false;\
}
#else
#define RETURN_IF(CONDITION, EXCEPTION) THROW_IF((CONDITION), EXCEPTION)
#endif
#define RETURN_IF_FALSE(CONDITION, EXCEPTION) RETURN_IF(!(CONDITION), EXCEPTION)


namespace vireo {
namespace error {

extern "C" void ImageCoreHandler(int code, const char* message, const char* file, int line);

#ifdef __EXCEPTIONS

class Exception : public std::exception {
  const std::string _what;
public:
  Exception(const char* file, const char* function, const char* line, const char* condition, const char* what, const char* reason)
    : _what(std::string("[") + file + ": " + function + " (" + line + ")] " + what + ": \"" + condition + "\" condition failed; reason: " + reason) {}
  Exception(const char* what)
    : _what(what) {}
  virtual const char* what() const noexcept { return _what.c_str(); }
};

#endif

}}
