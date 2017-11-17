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
