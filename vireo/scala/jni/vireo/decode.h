//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Audio_jniInit(JNIEnv*, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Audio_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_decode_jni_Audio_decode(JNIEnv*, jobject, jlong, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_decode_jni_Audio_pcm(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Audio_freePcm(JNIEnv*, jobject, jlong, jobject);

// Video
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Video_jniInit(JNIEnv*, jobject, jobject, jint);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Video_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_decode(JNIEnv*, jobject, jlong, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_yuv(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Video_freeYuv(JNIEnv*, jobject, jlong, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_rgb(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_decode_jni_Video_freeRgb(JNIEnv*, jobject, jlong, jobject);

#ifdef __cplusplus
}
#endif
