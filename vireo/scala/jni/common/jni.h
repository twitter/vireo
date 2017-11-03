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

#include <jni.h>

#include "vireo/base_h.h"

namespace vireo {
namespace jni {

// Workaround Scala bug: if no method is called on object it is collected (apply call later returns NULL)
// NOTE: probably safe to remove
#define PREVENT_IMMEDIATE_COLLECTION_OF(x) {\
  jobject obj = (x).call<jobject>("toString", "()Ljava/lang/String;");\
  env->DeleteLocalRef(obj);\
}

class Wrap {
  struct _Wrap* _this;
  Wrap();
public:
  Wrap(JNIEnv* env, jobject obj, const char* class_name = nullptr);
  Wrap(JNIEnv* env, const char* class_name, const char* init_sig, ...);
  Wrap(Wrap&& wrap);
  Wrap(const Wrap& wrap);
  ~Wrap();
  DISALLOW_ASSIGN(Wrap);
  jobject operator*() const;
  jobject obj() const;
  std::string class_name() const;
  bool is_subclass_of_class_named(const std::string& class_name) const;
  template <typename T> T get(const char* name) const;
  template <typename T> void set(const char* name, const T& value);
  jobject get(const char* name, const char* sig) const;
  void set(const char* name, const char* sig, jobject value);
  template <typename T> T call(const char* name, const char* sig, ...) const;
  static Wrap Null;
};

class LocalFrame {  // Simple wrapper around jni local ref frame
  static const int kDefaultCapacity = 32;
  JNIEnv* env;
  bool popped = false;
public:
  LocalFrame(JNIEnv* env, int size = kDefaultCapacity) : env(env) {
    env->PushLocalFrame(size);
  }
  ~LocalFrame() {
    if (!popped) {
      env->PopLocalFrame(nullptr);
    }
  }
  auto operator()(jobject result) -> jobject {
    popped = true;
    return env->PopLocalFrame(result);
  }
};

class ExceptionHandler {  // Utility class to handle exceptions across JNI calls
  static void JavaExceptionToNativeException(JNIEnv* env, const jthrowable exception_obj);
  static void NativeExceptionToJavaException(JNIEnv* env, const std::exception& exception);
  static void ThrowGenericJavaException(JNIEnv* env);
public:
  static void CatchJavaExceptionThrowNativeException(JNIEnv* env);
  static void SafeExecuteFunction(JNIEnv* env,
                                  const std::function<void(void)>& function,
                                  const std::function<void(void)>& finalize = [](){});
  template <typename ReturnType>
  static auto SafeExecuteFunctionAndReturn(JNIEnv* env,
                                           const std::function<ReturnType(void)>& function,
                                           ReturnType default_value) -> ReturnType {
    __try {
      return function();
    } __catch(const std::exception& exception) {
      NativeExceptionToJavaException(env, exception);
    } __catch (...) {
      ThrowGenericJavaException(env);
    }
    return default_value;
  }
};

}}
