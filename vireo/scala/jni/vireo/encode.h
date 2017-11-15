/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
JNIEXPORT void    JNICALL Java_com_twitter_vireo_encode_jni_H264_jniInit(JNIEnv*, jobject, jobject, jint, jint, jbyte, jfloat, jint, jint, jint, jfloat, jint, jboolean, jboolean, jbyte, jint, jstring,  jboolean, jint, jbyte, jint, jint, jbyte, jint, jint, jint, jbyte, jfloat);
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
