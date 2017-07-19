//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
