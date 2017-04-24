//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>

#include "imagecore/image/yuv.h"
#include "imagecore/imagecore.h"
#include "imagecore/formats/writer.h"
#undef LOCAL
#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/common/enum.hpp"
#include "vireo/encode/jpg.h"
#include "vireo/error/error.h"
#include "vireo/util/util.h"
#include "vireo/frame/util.h"

namespace vireo {
using common::Data16;

CONSTRUCTOR static void _Init() {
  RegisterImageCoreAssertionHandler(&error::ImageCoreHandler);
}

namespace encode {

using namespace imagecore;

struct _JPG {
  int quality = 0;
  int optimization = 0;
  functional::Video<frame::YUV> frames;
};

JPG::JPG(const functional::Video<frame::YUV>& frames, int quality, int optimization)
  : functional::DirectVideo<JPG, common::Data32>(frames.a(), frames.b()), _this(new _JPG()) {
  THROW_IF(_this->frames.count() >= security::kMaxSampleCount, Unsafe);
  THROW_IF(optimization < 0 || optimization > 1, InvalidArguments);
  _this->quality = quality;
  _this->optimization = optimization;
  _this->frames = frames;
}

auto JPG::operator()(uint32_t index) const -> common::Data32 {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->frames.count(), OutOfRange);

  frame::YUV frame = _this->frames(index);
  THROW_IF(frame.uv_ratio().first != 2 || frame.uv_ratio().second != 2, Unsupported); // Only YUV420 supported

  auto fill_padding = [&frame]() {
    uint8_t* const src[] = {
      (uint8_t* const)frame.plane(frame::Y).bytes().data(),
      (uint8_t* const)frame.plane(frame::U).bytes().data(),
      (uint8_t* const)frame.plane(frame::V).bytes().data()
    };
    const int stride[] = {
      frame.plane(frame::Y).row(),
      frame.plane(frame::U).row(),
      frame.plane(frame::V).row()
    };
    const int width[] = {
      frame.plane(frame::Y).width(),
      frame.plane(frame::U).width(),
      frame.plane(frame::V).width()
    };
    const int height[] = {
      frame.plane(frame::Y).height(),
      frame.plane(frame::U).height(),
      frame.plane(frame::V).height()
    };
    for (auto p: enumeration::Enum<frame::PlaneIndex>(frame::Y, frame::V)) {
      for (int y = 0; y < height[p]; ++y) {
        int s = stride[p];
        int w = width[p];
        uint8_t* ptr = src[p] + y * s + w;
        uint8_t v = *(ptr-1) ;
        for (int x = w; x < s; ++x) {
          *ptr++ = v;
        }
      }
    }
  };


  ImageWriter::MemoryStorage storage(frame.plane(frame::Y).row() * frame.plane(frame::Y).height());
  unique_ptr<ImageWriter> writer(ImageWriter::createWithFormat(kImageFormat_JPEG, &storage));
  CHECK(writer);
  fill_padding();
  ImageWriter::EWriteOptions write_options = (ImageWriter::EWriteOptions)(ImageWriter::kWriteOption_CopyColorProfile |
                                                                          ImageWriter::kWriteOption_AssumeMCUPaddingFilled);
  if (_this->optimization == 0) {
    write_options = (ImageWriter::EWriteOptions)(write_options | ImageWriter::kWriteOption_QualityFast);
  }
  writer->setWriteOptions(write_options);
  writer->setQuality(_this->quality);
  auto src = unique_ptr<ImageYUV>(as_imagecore(frame));
  writer->writeImage(src.get());

  uint8_t* buffer = NULL;
  uint64_t length = 0;
  storage.ownBuffer(buffer, length);
  common::Data32 jpg_data(buffer, (uint32_t)length, [](uint8_t* p){ free(p); });
  CHECK(jpg_data.capacity());
  jpg_data.set_bounds(0, (uint32_t)storage.totalBytesWritten());

  return jpg_data;
}

}}
