//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>

#include "imagecore/image/rgba.h"
#include "imagecore/imagecore.h"
#include "imagecore/formats/writer.h"
#undef LOCAL
#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/common/enum.hpp"
#include "vireo/encode/png.h"
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

struct _PNG {
  functional::Video<frame::RGB> frames;
};

PNG::PNG(const functional::Video<frame::RGB>& frames)
  : functional::DirectVideo<PNG, common::Data32>(frames.a(), frames.b()), _this(new _PNG()) {
  THROW_IF(_this->frames.count() >= security::kMaxSampleCount, Unsafe);
  _this->frames = frames;
}

auto PNG::operator()(uint32_t index) const -> common::Data32 {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->frames.count(), OutOfRange);

  frame::RGB frame = _this->frames(index);

  ImageWriter::MemoryStorage storage(frame.plane().bytes().count());
  unique_ptr<ImageWriter> writer(ImageWriter::createWithFormat(kImageFormat_PNG, &storage));
  CHECK(writer);
  auto src = unique_ptr<ImageRGBA>(as_imagecore(frame));
  writer->writeImage(src.get());

  uint8_t* buffer = NULL;
  uint64_t length = 0;
  storage.ownBuffer(buffer, length);
  common::Data32 png_data(buffer, (uint32_t)length, [](uint8_t* p){ free(p); });
  CHECK(png_data.capacity());
  png_data.set_bounds(0, (uint32_t)storage.totalBytesWritten());

  return png_data;
}

}}
