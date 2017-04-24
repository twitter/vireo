//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

// Movie
JNIEXPORT void    JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniInit(JNIEnv*, jobject, jobject);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniClose(JNIEnv*, jobject);
JNIEXPORT jbyte   JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniFileType(JNIEnv*, jobject, jlong);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_decode(JNIEnv*, jobject, jlong, jbyte, jint);
JNIEXPORT jlong   JNICALL Java_com_twitter_vireo_demux_jni_Movie_duration(JNIEnv*, jobject, jlong, jbyte);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_editBoxes(JNIEnv*, jobject, jlong, jbyte);
JNIEXPORT jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_nal(JNIEnv*, jobject, jlong, jbyte, jint, jboolean);
JNIEXPORT void    JNICALL Java_com_twitter_vireo_demux_jni_Movie_freeNal(JNIEnv*, jobject, jlong, jobject);

#ifdef __cplusplus
}
#endif
