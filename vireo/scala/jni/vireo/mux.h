//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// MP2TS
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_jniInit(JNIEnv*, jobject, jobject, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_encode(JNIEnv*, jobject, jlong);

// MP4
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_MP4_jniInit(JNIEnv*, jobject, jobject, jobject, jobject, jobject, jbyte);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_MP4_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_mux_jni_MP4_encode(JNIEnv*, jobject, jlong, jbyte);

// WebM
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_WebM_jniInit(JNIEnv*, jobject, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_mux_jni_WebM_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_mux_jni_WebM_encode(JNIEnv*, jobject, jlong);

#ifdef __cplusplus
}
#endif
