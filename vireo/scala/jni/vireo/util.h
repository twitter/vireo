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
#include "vireo/frame/yuv.h"
#include "vireo/frame/rgb.h"
#include "vireo/functional/media.hpp"
#include "vireo/sound/pcm.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/settings/settings.h"

namespace vireo {
namespace jni {

template <typename DATA>
struct _Struct;

template <typename DATA>
class Struct {
  struct _Struct<DATA>* _this;
public:
  Struct();
  virtual ~Struct();
  DISALLOW_COPY_AND_ASSIGN(Struct);
  void add_buffer_ref(DATA&& data, const Wrap& jni_byte_data);
  void remove_buffer_ref(const void* data_ptr);
  bool empty() const;
};

Wrap create(JNIEnv* env, const frame::Plane& plane);
Wrap create(JNIEnv* env, const frame::YUV& yuv);
Wrap create(JNIEnv* env, const frame::RGB& yuv);
Wrap create(JNIEnv* env, const sound::PCM& sound);
Wrap create(JNIEnv* env, const common::Data32& sound);
template <typename T>
Wrap create(JNIEnv* env, jni::Struct<T>* jni, T&& data, bool copy);

template <typename DATA>
DATA createData(JNIEnv* env, jobject byte_data_obj, const jni::Wrap& jni_deleter, bool deleter);
template <typename T>
std::function<T(void)> createFunc(JNIEnv* enc, jobject func_obj);
sound::PCM createPCM(JNIEnv* env, jobject pcm_obj, const jni::Wrap& jni_deleter, bool deleter);
frame::RGB createRGB(JNIEnv* env, jobject rgb_obj, const jni::Wrap& jni_deleter, bool deleter);
frame::YUV createYUV(JNIEnv* env, jobject yuv_obj, const jni::Wrap& jni_deleter, bool deleter);
settings::Audio createAudioSettings(JNIEnv* env, jobject audio_settings_obj);
settings::Video createVideoSettings(JNIEnv* env, jobject video_settings_obj);
settings::Caption createCaptionSettings(JNIEnv* env, jobject caption_settings_obj);
template <typename T>
vector<T> createVectorFromMedia(JNIEnv* env, jobject media_obj, function<T(jobject elem_obj)> convert_function);
template <typename T>
vector<T> createVectorFromSeq(JNIEnv* env, jobject seq_obj, function<T(jobject elem_obj)> convert_function);

void setAudioSettings(JNIEnv* env, jni::Wrap& jni, const settings::Audio& settings);
void setVideoSettings(JNIEnv* env, jni::Wrap& jni, const settings::Video& settings);
void setDataSettings(JNIEnv* env, jni::Wrap& jni, const settings::Data& settings);
void setCaptionSettings(JNIEnv* env, jni::Wrap& jni, const settings::Caption& settings);

}}
