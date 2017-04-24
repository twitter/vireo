//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stitch
JNIEXPORT void    JNICALL Java_com_twitter_vireo_transform_jni_Stitch_jniInit(JNIEnv*, jobject, jobject, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_transform_jni_Stitch_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_transform_jni_Stitch_decode(JNIEnv*, jobject, jlong, jbyte, jint);
JNIEXPORT jlong   JNICALL Java_com_twitter_vireo_transform_jni_Stitch_duration(JNIEnv*, jobject, jlong, jbyte);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_transform_jni_Stitch_editBoxes(JNIEnv*, jobject, jlong, jbyte);

// Trim
JNIEXPORT void    JNICALL Java_com_twitter_vireo_transform_jni_Trim_jniInit(JNIEnv*, jobject, jobject, jobject, jlong, jlong);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_transform_jni_Trim_jniClose(JNIEnv*, jobject);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_transform_jni_Trim_decode(JNIEnv*, jobject, jlong, jint);
JNIEXPORT jlong   JNICALL Java_com_twitter_vireo_transform_jni_Trim_duration(JNIEnv*, jobject, jlong);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_transform_jni_Trim_editBoxes(JNIEnv*, jobject, jlong);

#ifdef __cplusplus
}
#endif
