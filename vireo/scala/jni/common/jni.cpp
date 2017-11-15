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

#ifdef __APPLE__
#include <mutex>
#endif

#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/error/error.h"
#include "vireo/scala/jni/common/jni.h"

#ifdef __APPLE__  // thread_local not supported by Apple clang in dylibs
using std::mutex;
using std::lock_guard;
#endif

namespace vireo {
namespace jni {

Wrap Wrap::Null;

struct _JNIStruct {
#ifdef __APPLE__
  mutex lock;
#endif
  map<tuple<jclass, string, string>, jfieldID> field_map;
  map<tuple<jclass, string, string>, jmethodID> method_map;
  unordered_map<string, jclass> class_map;
  jfieldID field(JNIEnv* env, jclass obj_class, const char* name, const char* sig) {
    CHECK(name);
    #ifdef __APPLE__
    lock_guard<mutex> lock_guard(lock);
    #endif
    auto found = field_map.find(make_tuple(obj_class, name, sig));
    if (found != field_map.end()) {
      return found->second;
    }
    jfieldID field = env->GetFieldID(obj_class, name, sig);
    CHECK(field);
    field_map[make_tuple(obj_class, name, sig)] = field;
    return field;
  }
  jmethodID method(JNIEnv* env, jclass obj_class, const char* name, const char* sig) {
    CHECK(name);
#ifdef __APPLE__
    lock_guard<mutex> lock_guard(lock);
#endif
    auto found = method_map.find(make_tuple(obj_class, name, sig));
    if (found != method_map.end()) {
      return found->second;
    }
    jmethodID method = env->GetMethodID(obj_class, name, sig);
    CHECK(method);
    method_map[make_tuple(obj_class, name, sig)] = method;
    return method;
  }
  jclass clazz(JNIEnv* env, const char* name) {
    CHECK(name);
#ifdef __APPLE__
    lock_guard<mutex> lock_guard(lock);
#endif
    auto found = class_map.find(name);
    if (found != class_map.end()) {
      return found->second;
    }
    jclass clazz = env->FindClass(name);
    CHECK(clazz);
    class_map[name] = (jclass)env->NewGlobalRef(clazz);
    return clazz;
  }
};

struct _Wrap {
#ifndef __APPLE__
  thread_local
#endif
  static _JNIStruct _jni;
  JNIEnv* env;
  string obj_class_name;
  jobject obj_global_ref;
  static string GetClassName(JNIEnv* env, jobject obj) {
    jclass clazz = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(clazz, "toString", "()Ljava/lang/String;");
    auto str_obj = (jstring)env->CallObjectMethod(clazz, method);
    const char* str = env->GetStringUTFChars(str_obj, NULL);
    string name = str;
    env->ReleaseStringUTFChars(str_obj, str);
    CHECK(name.length());
    for (int i = 0; i < name.length(); ++i) {
      if (name[i] == '.') {
        name[i] = '/';
      }
    }
    const auto desc_str = string("class ");
    CHECK(name.find(desc_str) == 0);
    return name.substr(desc_str.size());
  }
  template <typename DATA>
  static DATA ConvertToData(JNIEnv* env, jbyteArray array_obj) {
    CHECK(array_obj);
    auto array_obj_ref = (jbyteArray)env->NewGlobalRef(array_obj);
    DCHECK(env->IsSameObject(array_obj_ref, array_obj));
    auto bytes = env->GetByteArrayElements(array_obj_ref, NULL);
    CHECK(bytes);
    auto length = env->GetArrayLength(array_obj_ref);
    if (length > 0) {
      return DATA((const uint8_t*)bytes, (uint32_t)length, [env=env, array_obj_ref](uint8_t* p) {
        env->ReleaseByteArrayElements(array_obj_ref, (jbyte*)p, JNI_ABORT);
        env->DeleteGlobalRef(array_obj_ref);
      });
    } else {
      return DATA(NULL, 0, NULL);
    }
  }
  template <typename DATA>
  DATA get_field_data(const char* name) const {
    auto field = _Wrap::_jni.field(env, obj_class(), name, "[B");
    jbyteArray array_obj = (jbyteArray)env->GetObjectField(obj_global_ref, field);
    CHECK(array_obj);
    return ConvertToData<DATA>(env, array_obj);
  }
  static common::Sample16 ConvertToSample16(JNIEnv* env, jshortArray array_obj) {
    CHECK(array_obj);
    auto array_obj_ref = (jshortArray)env->NewGlobalRef(array_obj);
    DCHECK(env->IsSameObject(array_obj_ref, array_obj));
    auto bytes = env->GetShortArrayElements(array_obj_ref, NULL);
    CHECK(bytes);
    auto length = env->GetArrayLength(array_obj_ref);
    if (length > 0) {
      return common::Sample16((const int16_t*)bytes, (uint16_t)length, [env=env, array_obj_ref](int16_t* p) {
        env->ReleaseShortArrayElements(array_obj_ref, (jshort*)p, JNI_ABORT);
        env->DeleteGlobalRef(array_obj_ref);
      });
    } else {
      return common::Sample16(NULL, 0, NULL);
    }
  }
  jclass obj_class() const {
    return _jni.clazz(env, obj_class_name.c_str());
  }
};

template <>
common::Sample16 _Wrap::get_field_data<common::Sample16>(const char* name) const {
  auto field = _Wrap::_jni.field(env, obj_class(), name, "[S");
  jshortArray array_obj = (jshortArray)env->GetObjectField(obj_global_ref, field);
  CHECK(array_obj);
  return ConvertToSample16(env, array_obj);
}

#ifndef __APPLE__
thread_local
#endif
_JNIStruct _Wrap::_jni;

Wrap::Wrap() : _this(nullptr) {
}

Wrap::Wrap(JNIEnv* env, jobject obj, const char* class_name) {
  CHECK(obj);
  _this = new _Wrap();
  _this->env = env;
  _this->obj_global_ref = env->NewGlobalRef(obj);
  _this->obj_class_name = class_name ? class_name : _this->GetClassName(env, _this->obj_global_ref);
  DCHECK(_this->obj_class_name == _this->GetClassName(env, _this->obj_global_ref));
  CHECK(_this->obj_class_name.length());
}

Wrap::Wrap(JNIEnv* env, const char* class_name, const char* init_sig, ...) {
  _this = new _Wrap();
  _this->env = env;
  jclass clazz = _Wrap::_jni.clazz(env, class_name);
  auto method = _Wrap::_jni.method(_this->env, clazz, "<init>", init_sig);
  va_list args;
  va_start(args, init_sig);
  jobject obj = env->NewObjectV(clazz, method, args);
  va_end(args);
  _this->obj_global_ref = _this->env->NewGlobalRef(obj);
  _this->obj_class_name = class_name;
  DCHECK(_this->obj_class_name == _this->GetClassName(_this->env, _this->obj_global_ref));
  CHECK(_this->obj_class_name.length());
}

Wrap::Wrap(const Wrap& wrap) : Wrap{ wrap._this->env, wrap._this->obj_global_ref, wrap._this->obj_class_name.c_str() } {}

Wrap::Wrap(Wrap&& wrap) {
  _this = wrap._this;
  wrap._this = nullptr;
}

Wrap::~Wrap() {
  if (_this) {
    _this->env->DeleteGlobalRef(_this->obj_global_ref);
    delete _this;
  }
}

jobject Wrap::operator*() const {
  return _this->env->NewLocalRef(_this->obj_global_ref);
}

bool Wrap::is_subclass_of_class_named(const std::string& class_name) const {
  auto class_obj = _Wrap::_jni.clazz(_this->env, class_name.c_str());
  return _this->env->IsInstanceOf(_this->obj_global_ref, class_obj);
}

string Wrap::class_name() const {
  return _this->obj_class_name;
}

template <>
jboolean Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "Z");
  return _this->env->GetBooleanField(_this->obj_global_ref, field);
}

template <>
jbyte Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "B");
  return _this->env->GetByteField(_this->obj_global_ref, field);
}

template <>
jshort Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "S");
  return _this->env->GetShortField(_this->obj_global_ref, field);
}

template <>
jint Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "I");
  return _this->env->GetIntField(_this->obj_global_ref, field);
}

template <>
jlong Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "J");
  return _this->env->GetLongField(_this->obj_global_ref, field);
}

template <>
jfloat Wrap::get(const char* name) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "F");
  return _this->env->GetFloatField(_this->obj_global_ref, field);
}

template <>
void Wrap::set(const char* name, const jboolean& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "Z");
  return _this->env->SetBooleanField(_this->obj_global_ref, field, value);
}

template <>
void Wrap::set(const char* name, const jbyte& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "B");
  return _this->env->SetByteField(_this->obj_global_ref, field, value);
}

template <>
void Wrap::set(const char* name, const jshort& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "S");
  return _this->env->SetShortField(_this->obj_global_ref, field, value);
}

template <>
void Wrap::set(const char* name, const jint& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "I");
  return _this->env->SetIntField(_this->obj_global_ref, field, value);
}

template <>
void Wrap::set(const char* name, const jlong& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "J");
  return _this->env->SetLongField(_this->obj_global_ref, field, value);
}

template <>
void Wrap::set(const char* name, const jdouble& value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, "D");
  return _this->env->SetDoubleField(_this->obj_global_ref, field, value);
}

template <>
common::Data16 Wrap::get(const char* name) const {
  return _this->get_field_data<common::Data16>(name);
}

template <>
common::Data32 Wrap::get(const char* name) const {
  return _this->get_field_data<common::Data32>(name);
}

template <>
common::Sample16 Wrap::get(const char* name) const {
  return _this->get_field_data<common::Sample16>(name);
}

jobject Wrap::get(const char* name, const char* sig) const {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, sig);
  return _this->env->GetObjectField(_this->obj_global_ref, field);
}

void Wrap::set(const char* name, const char* sig, jobject value) {
  auto field = _Wrap::_jni.field(_this->env, _this->obj_class(), name, sig);
  _this->env->SetObjectField(_this->obj_global_ref, field, value);
}

template <>
jint Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  const jint result = _this->env->CallIntMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return result;
}

template <>
jlong Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  const jlong result = _this->env->CallLongMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return result;
}

template <>
jboolean Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  const jboolean result = _this->env->CallBooleanMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return result;
}

template <>
jobject Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  const jobject result = _this->env->CallObjectMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return result;
}

template <>
void Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  _this->env->CallObjectMethodV(_this->obj_global_ref, method, args);
  va_end(args);
}

template <>
common::Data32 Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  auto array_obj = (jbyteArray)_this->env->CallObjectMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return _Wrap::ConvertToData<common::Data32>(_this->env, array_obj);
}

template <>
common::Sample16 Wrap::call(const char* name, const char* sig, ...) const {
  auto method = _Wrap::_jni.method(_this->env, _this->obj_class(), name, sig);
  va_list args;
  va_start(args, sig);
  auto array_obj = (jshortArray)_this->env->CallObjectMethodV(_this->obj_global_ref, method, args);
  va_end(args);
  return _Wrap::ConvertToSample16(_this->env, array_obj);
}

void ExceptionHandler::JavaExceptionToNativeException(JNIEnv *env, const jthrowable exception_obj) {
  if (exception_obj) {
    env->ExceptionClear();
    auto exception = Wrap(env, exception_obj);
    auto msg_obj = (jstring)exception.call<jobject>("toString", "()Ljava/lang/String;");
    const char* msg_str = env->GetStringUTFChars(msg_obj, NULL);
    string msg_str_copy = msg_str;
    env->ReleaseStringUTFChars(msg_obj, msg_str);
    throw error::Exception(msg_str_copy.c_str());
  }
}

void ExceptionHandler::NativeExceptionToJavaException(JNIEnv *env, const std::exception& exception) {
  jclass jc = env->FindClass("com/twitter/vireo/VireoException");
  if (jc) {
    env->ThrowNew(jc, exception.what());
  }
}

void ExceptionHandler::ThrowGenericJavaException(JNIEnv *env) {
  jclass jc = env->FindClass("com/twitter/vireo/VireoException");
  if (jc) {
    env->ThrowNew(jc, "unknown error in native code");
  }
}

void ExceptionHandler::CatchJavaExceptionThrowNativeException(JNIEnv *env) {
  jthrowable exception_obj = env->ExceptionOccurred();
  if (exception_obj) {
    JavaExceptionToNativeException(env, exception_obj);
  }
}

void ExceptionHandler::SafeExecuteFunction(JNIEnv* env,
                                           const std::function<void(void)>& function,
                                           const std::function<void(void)>& finalize) {
  __try {
    function();
  } __catch(const std::exception& exception) {
    NativeExceptionToJavaException(env, exception);
    finalize();
  } __catch (...) {
    ThrowGenericJavaException(env);
  }
}

}}
