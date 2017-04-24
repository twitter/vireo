//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Periscope Util
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_periscope_jni_Util_jniParseID3Info(JNIEnv*, jobject, jobject);

#ifdef __cplusplus
}
#endif
