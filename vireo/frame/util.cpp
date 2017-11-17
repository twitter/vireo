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

#include "imagecore/image/rgba.h"
#include "imagecore/image/yuv.h"
#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/error/error.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/util.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

using namespace imagecore;

auto as_imagecore(const frame::YUV& yuv) -> ImageYUV* {
  ImagePlane8* planes[3];
  int index = 0;
  for (auto plane_type: enumeration::Enum<frame::PlaneIndex>(frame::PlaneIndex::Y, frame::PlaneIndex::V)) {
    const auto& plane = yuv.plane(plane_type);
    auto image_plane = ImagePlane8::create((uint8_t*)plane.bytes().data(), plane.bytes().count());
    if (!image_plane) {
      for (int i = 0; i < index; ++i) {
        delete planes[i];
      }
    }
    image_plane->setDimensions(plane.width(), plane.height(), 0, plane.alignment());
    planes[index++] = image_plane;
  }
  ImageYUV* dst = ImageYUV::create(planes[0], planes[1], planes[2]);
  CHECK(dst);
  return dst;
}

auto as_imagecore(const frame::RGB& rgb) -> ImageRGBA* {
  THROW_IF(rgb.component_count() != 4, InvalidArguments);
  ImageRGBA* dst = ImageRGBA::create((uint8_t*)rgb.plane().bytes().data(), rgb.plane().bytes().count());
  CHECK(dst);
  dst->setDimensions(rgb.width(), rgb.height(), 0, rgb.plane().row());
  return dst;
}

}}
