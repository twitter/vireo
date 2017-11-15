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
