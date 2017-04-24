//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <fstream>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/error/error.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/sound.h"
#include "vireo/scala/jni/vireo/util.h"

using namespace vireo;

jobject JNICALL Java_com_twitter_vireo_sound_PCM_mix(JNIEnv* env, jobject pcm_obj, jbyte channels) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    return *jni::create(env, jni::createPCM(env, pcm_obj, jni::Wrap::Null, false).mix((uint8_t)channels));
  }, NULL);
}
