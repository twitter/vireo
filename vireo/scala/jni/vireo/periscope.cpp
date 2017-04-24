//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
