//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
