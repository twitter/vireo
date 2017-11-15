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
#include "vireo/common/ref.h"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/error/error.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/util.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

using namespace imagecore;

template <typename Available = has_swscale>
auto stretch_swscale(const RGB& rgb, int num_x, int denum_x, int num_y, int denum_y) -> RGB;

template <>
auto stretch_swscale<std::false_type>(const RGB& rgb, int num_x, int denum_x, int num_y, int denum_y) -> RGB {
  THROW_IF(true, MissingDependency);
}

struct _RGB : common::Ref<_RGB> {
  Plane plane;
  uint8_t component_count;
  _RGB(uint8_t component_count, Plane&& plane) : plane(move(plane)), component_count(component_count) {}
};

RGB::RGB(uint8_t component_count, Plane&& plane) {
  THROW_IF(component_count < 3 || component_count > 4, InvalidArguments);
  _this = new _RGB(component_count, move(plane));
}

RGB::RGB(uint16_t width, uint16_t height, uint8_t component_count) {
  THROW_IF(component_count < 3 || component_count > 4, InvalidArguments);
  THROW_IF(!security::valid_dimensions(width, height), Unsafe);
  const uint16_t row = common::align_shift(width * component_count, IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT);
  const uint32_t size = row * height + IMAGE_ROW_DEFAULT_ALIGNMENT; // sws_scale uses vector registers that access extra bytes after the meaningful data
  common::Data32 rgb_data((uint8_t*)memalign(IMAGE_ROW_DEFAULT_ALIGNMENT, size), size, [](uint8_t* p) { free(p); });
  memset((void*)rgb_data.data(), 0, size);
  frame::Plane plane(row, width * component_count, height, move(rgb_data));
  _this = new _RGB(component_count, move(plane));
}

RGB::RGB(RGB&& rgb) : RGB(rgb.component_count(), move(rgb._this->plane)) {
}

RGB::RGB(const RGB& rgb) {
  _this = rgb._this->ref();
  _this->component_count = rgb.component_count();
}

RGB::~RGB() {
  _this->unref();
}

auto RGB::width() const -> uint16_t {
  return _this->plane.width() / _this->component_count;
}

auto RGB::height() const -> uint16_t {
  return _this->plane.height();
}

auto RGB::component_count() const -> uint8_t {
  return _this->component_count;
}

auto RGB::plane() const -> const Plane& {
 return _this->plane;
}

template <>
auto RGB::yuv<std::false_type>(uint8_t uv_x_ratio, uint8_t uv_y_ratio) const -> YUV {
  THROW_IF(true, MissingDependency);
}

auto RGB::crop(uint16_t x_offset, uint16_t y_offset, uint16_t cropped_width, uint16_t cropped_height) const -> RGB {
  THROW_IF(!(cropped_width > 0 && cropped_height > 0 &&
             cropped_width <= 8192 && cropped_height <= 8192), InvalidArguments);
  THROW_IF(x_offset + cropped_width > width() || y_offset + cropped_height > height(), InvalidArguments);
  frame::RGB rgb_cropped(cropped_width, cropped_height, component_count());
  uint16_t y = 0;
  for (auto line: plane()) {
    if (y >= y_offset + cropped_height) {
      break;
    }
    if (y >= y_offset) {
      memcpy((void*)rgb_cropped.plane()(y - y_offset).data(),
             (const void*)(line.data() + x_offset * component_count()),
             cropped_width * component_count());
    }
    ++y;
  }
  return rgb_cropped;
}

auto RGB::rotate(Rotation direction) -> RGB {
  THROW_IF(direction == Rotation::None, InvalidArguments);
  const bool flip_coords = direction == Rotation::Left || direction == Rotation::Right;
  const uint16_t new_width  = flip_coords ? height() : width();
  const uint16_t new_height = flip_coords ? width() : height();
  frame::RGB new_rgb(new_width, new_height, 4);

  unique_ptr<ImageRGBA> src(as_imagecore(*this));
  unique_ptr<ImageRGBA> dst(as_imagecore(new_rgb));

  switch (direction) {
    case Rotation::Right:
      src->rotate(dst.get(), EImageOrientation::kImageOrientation_Right);
      break;
    case Rotation::Down:
      src->rotate(dst.get(), EImageOrientation::kImageOrientation_Down);
      break;
    case Rotation::Left:
      src->rotate(dst.get(), EImageOrientation::kImageOrientation_Left);
      break;
    default:
      src->copy(dst.get());
  }

  return new_rgb;
}

auto RGB::stretch(int num_x, int denum_x, int num_y, int denum_y) const -> RGB {
  THROW_IF(!(num_x < 10000 && denum_x < 10000 && num_x >= 0 && denum_x >= 0), InvalidArguments);
  THROW_IF(!(num_y < 10000 && denum_y < 10000 && num_y >= 0 && denum_y >= 0), InvalidArguments);
  if (component_count() == 3 || num_x > denum_x || num_y > denum_y) {
    return stretch_swscale(*this, num_x, denum_x, num_y, denum_y);
  } else {
    const uint32_t new_width = common::round_divide((uint32_t)width(), (uint32_t)num_x, (uint32_t)denum_x);
    const uint32_t new_height = common::round_divide((uint32_t)height(), (uint32_t)num_y, (uint32_t)denum_y);

    unique_ptr<ImageRGBA> src(as_imagecore(*this));

    THROW_IF(new_width > numeric_limits<uint16_t>::max(), Overflow);
    THROW_IF(new_height > numeric_limits<uint16_t>::max(), Overflow);
    frame::RGB new_rgb((uint16_t)new_width, (uint16_t)new_height, component_count());
    unique_ptr<ImageRGBA> dst(as_imagecore(new_rgb));

    src->resize(dst.get(), imagecore::EResizeQuality::kResizeQuality_High);

    return new_rgb;
  }
}

}}
