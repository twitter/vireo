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

#include <fstream>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/error/error.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"
#include "vireo/scala/jni/vireo/frame.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/scala/jni/common/jni.h"

using namespace vireo;

jobject JNICALL Java_com_twitter_vireo_frame_RGB_rgb(JNIEnv* env, jobject rgb_obj, jint component_count) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createRGB(env, rgb_obj, jni::Wrap::Null, false).rgb((uint8_t)component_count));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_RGB_yuv(JNIEnv* env, jobject rgb_obj, jint uv_x_ratio, jint uv_y_ratio) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createRGB(env, rgb_obj, jni::Wrap::Null, false).yuv((uint8_t)uv_x_ratio, (uint8_t)uv_y_ratio));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_RGB_crop(JNIEnv* env, jobject rgb_obj, jint x_offset, jint y_offset, jint width, jint height) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto tmp = jni::Wrap(env, rgb_obj);
    return *jni::create(env, jni::createRGB(env, rgb_obj, jni::Wrap::Null, false).crop((uint16_t)x_offset, (uint16_t)y_offset, (uint16_t)width, (uint16_t)height));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_RGB_rotate(JNIEnv* env, jobject rgb_obj, jbyte direction) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createRGB(env, rgb_obj, jni::Wrap::Null, false).rotate((vireo::frame::Rotation)direction));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_RGB_stretch(JNIEnv* env, jobject rgb_obj, jint num_x, jint denum_x, jint num_y, jint denum_y) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createRGB(env, rgb_obj, jni::Wrap::Null, false).stretch((int)num_x, (int)denum_x, (int)num_y, (int)denum_y));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_YUV_rgb(JNIEnv* env, jobject yuv_obj, jint num_components) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createYUV(env, yuv_obj, jni::Wrap::Null, false).rgb((uint8_t)num_components));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_YUV_fullRange(JNIEnv* env, jobject yuv_obj, jboolean full_range) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createYUV(env, yuv_obj, jni::Wrap::Null, false).full_range((bool)full_range));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_YUV_crop(JNIEnv* env, jobject yuv_obj, jint x_offset, jint y_offset, jint width, jint height) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createYUV(env, yuv_obj, jni::Wrap::Null, false).crop((uint16_t)x_offset, (uint16_t)y_offset, (uint16_t)width, (uint16_t)height));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_YUV_rotate(JNIEnv* env, jobject yuv_obj, jbyte direction) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createYUV(env, yuv_obj, jni::Wrap::Null, false).rotate((vireo::frame::Rotation)direction));
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_frame_YUV_stretch(JNIEnv* env, jobject yuv_obj, jint num_x, jint denum_x, jint num_y, jint denum_y) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createYUV(env, yuv_obj, jni::Wrap::Null, false).stretch((int)num_x, (int)denum_x, (int)num_y, (int)denum_y));
  }, NULL);
}
