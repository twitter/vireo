//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_RGB_rgb(JNIEnv*, jobject, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_RGB_yuv(JNIEnv*, jobject, jint, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_RGB_crop(JNIEnv*, jobject, jint, jint, jint, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_RGB_rotate(JNIEnv*, jobject, jbyte);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_RGB_stretch(JNIEnv*, jobject, jint, jint, jint, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_YUV_rgb(JNIEnv*, jobject, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_YUV_fullRange(JNIEnv*, jobject, jboolean);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_YUV_crop(JNIEnv*, jobject, jint, jint, jint, jint);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_YUV_rotate(JNIEnv*, jobject, jbyte);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_frame_YUV_stretch(JNIEnv*, jobject, jint, jint, jint, jint);

#ifdef __cplusplus
}
#endif
