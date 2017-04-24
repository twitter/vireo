//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_com_twitter_vireo_sound_PCM_mix(JNIEnv*, jobject, jbyte);

#ifdef __cplusplus
}
#endif
