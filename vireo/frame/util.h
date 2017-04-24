//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"

namespace imagecore {

class ImageRGBA;
class ImageYUV;

}

namespace vireo {
namespace frame {

class RGB;
class YUV;

auto as_imagecore(const frame::YUV& yuv) -> imagecore::ImageYUV*;
auto as_imagecore(const frame::RGB& rgb) -> imagecore::ImageRGBA*;

}}
