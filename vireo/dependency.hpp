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
#include "vireo/config.h"
#include "vireo/settings/settings.h"

namespace vireo {

template <FileType Type>
struct has_demuxer : public std::false_type {};
template <SampleType Type, typename settings::Settings<Type>::Codec codec>
struct has_decoder : public std::false_type {};

template <>
struct has_demuxer<FileType::MP4> : public std::true_type {};

template <>
struct has_demuxer<FileType::Image> : public std::true_type {};

template <settings::Audio::Codec Codec>
using has_audio_decoder = has_decoder<SampleType::Audio, Codec>;

template <settings::Video::Codec Codec>
using has_video_decoder = has_decoder<SampleType::Video, Codec>;


#ifdef HAVE_LIBAVFORMAT
template <>
struct has_demuxer<FileType::MP2TS> : public std::true_type {};
#endif

#ifdef HAVE_LIBAVCODEC
template <>
struct has_decoder<SampleType::Video, settings::Video::Codec::H264> : public std::true_type {};
#endif

#ifdef HAVE_LIBFDK_AAC
using has_aac_decoder = std::true_type;
#else
using has_aac_decoder = std::false_type;
#endif

#ifdef HAVE_VORBIS
template <>
struct has_decoder<SampleType::Audio, settings::Audio::Codec::Vorbis> : public std::true_type {};
#endif

#ifdef HAVE_LIBSWSCALE
using has_swscale = std::true_type;
#else
using has_swscale = std::false_type;
#endif




}
