//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// AAC
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_AAC_jniInit(JNIEnv*, jobject, jobject, jbyte, jint);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_AAC_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_AAC_sample(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_AAC_freeSample(JNIEnv*, jobject, jlong, jobject);

// Vorbis
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_jniInit(JNIEnv*, jobject, jobject, jbyte, jint);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_sample(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_freeSample(JNIEnv*, jobject, jlong, jobject);

// H264
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_H264_jniInit(JNIEnv*, jobject, jobject, jint, jint, jbyte, jfloat, jint, jint, jint, jfloat, jint, jbyte, jbyte, jfloat);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_H264_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_H264_sample(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_H264_freeSample(JNIEnv*, jobject, jlong, jobject);

// VP8
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_VP8_jniInit(JNIEnv*, jobject, jobject, jint, jint, jfloat, jint);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_VP8_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_VP8_sample(JNIEnv*, jobject, jlong, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_VP8_freeSample(JNIEnv*, jobject, jlong, jobject);

// JPG
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_JPG_jniInit(JNIEnv*, jobject, jobject, jint, jint);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_JPG_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_JPG_encode(JNIEnv*, jobject, jlong, jint);

// PNG
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_PNG_jniInit(JNIEnv*, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_PNG_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_encode_jni_PNG_encode(JNIEnv*, jobject, jlong, jint);

#ifdef __cplusplus
}
#endif
