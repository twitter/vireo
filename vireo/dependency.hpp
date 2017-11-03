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

#include "vireo/types.h"
#ifndef TWITTER_INTERNAL
#include "vireo/config.h"
#endif

namespace vireo {

#ifdef TWITTER_INTERNAL
template <FileType Type> struct has_dependency : public std::true_type {};
#else
template <FileType Type> struct has_dependency : public std::false_type {};

template<> struct has_dependency<FileType::MP4> : public std::true_type {};
template<> struct has_dependency<FileType::Image> : public std::true_type {};

#ifdef HAVE_LIBWEBM
template<> struct has_dependency<FileType::WebM> : public std::true_type {};
#endif

#ifdef HAVE_LIBAVFORMAT
template<> struct has_dependency<FileType::MP2TS> : public std::true_type {};
#endif
#endif

}
