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
