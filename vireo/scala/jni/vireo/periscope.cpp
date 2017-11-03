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

#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/scala/jni/vireo/periscope.h"
#include "vireo/periscope/util.h"

using namespace vireo;

// Periscope Util Implementation
jobject JNICALL Java_com_twitter_vireo_periscope_jni_Util_jniParseID3Info(JNIEnv* env, jobject util_obj, jobject data_obj) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni_data = jni::Wrap(env, data_obj);
    auto data = jni::createData<common::Data32>(env, data_obj, jni_data, false);
    auto id3_info = periscope::Util::ParseID3Info(data);
    return *jni::Wrap(env, "com/twitter/vireo/periscope/ID3Info", "(BD)V", id3_info.orientation, id3_info.ntp_timestamp);
  }, NULL);
};
