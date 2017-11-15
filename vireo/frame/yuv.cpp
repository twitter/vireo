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
extern "C" {
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
}
#include "vireo/base_cpp.h"
#include "vireo/common/ref.h"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/common/enum.hpp"
#include "vireo/constants.h"
#include "vireo/error/error.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/util.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

using namespace imagecore;

struct _YUV : common::Ref<_YUV> {
  Plane y;
  Plane u;
  Plane v;
  bool full_range = true;
  _YUV(Plane&& y, Plane&& u, Plane&& v, bool full_range) : y(move(y)), u(move(u)), v(move(v)), full_range(full_range) {}
};

YUV::YUV(Plane&& y, Plane&& u, Plane&& v, bool full_range)
  : domain::Interval<YUV, std::function<common::Data16(PlaneIndex)>, uint16_t>(0, y.height()) {
  THROW_IF(!security::valid_dimensions(y.width(), y.height()), Unsafe);
  _this = new _YUV(move(y), move(u), move(v), full_range);
  THROW_IF(uv_ratio().first > 2 || uv_ratio().second > 2 || uv_ratio().first == 0 || uv_ratio().second == 0, InvalidArguments);
}

YUV::YUV(uint16_t width, uint16_t height, uint8_t uv_x_ratio, uint8_t uv_y_ratio, bool full_range)
  : domain::Interval<YUV, std::function<common::Data16(PlaneIndex)>, uint16_t>(0, height) {
  THROW_IF(uv_x_ratio > 2 || uv_y_ratio > 2 || uv_x_ratio == 0 || uv_y_ratio == 0, InvalidArguments);
  THROW_IF(!security::valid_dimensions(width, height), Unsafe);
  const uint16_t row = common::align_shift(width, IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT);
  const uint16_t column = common::align_shift(height, IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT);
  const uint32_t size = row * column + IMAGE_ROW_DEFAULT_ALIGNMENT; // sws_scale uses vector registers that access extra bytes after the meaningful data
  const uint16_t uv_width = uv_x_ratio == 1 ? width : (width + 1) / uv_x_ratio;
  const uint16_t uv_height = uv_y_ratio == 1 ? height : (height + 1) / uv_y_ratio;
  const uint16_t uv_row = common::align_shift(row / uv_x_ratio, IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT);
  const uint16_t uv_column = common::align_shift(column / uv_y_ratio, IMAGE_ROW_DEFAULT_ALIGNMENT_SHIFT);
  const uint32_t uv_size = uv_row * uv_column + IMAGE_ROW_DEFAULT_ALIGNMENT; // sws_scale uses vector registers that access extra bytes after the meaningful data
  common::Data32 y_data((uint8_t*)memalign(IMAGE_ROW_DEFAULT_ALIGNMENT, size), size,       [](uint8_t* p) { free(p); });
  common::Data32 u_data((uint8_t*)memalign(IMAGE_ROW_DEFAULT_ALIGNMENT, uv_size), uv_size, [](uint8_t* p) { free(p); });
  common::Data32 v_data((uint8_t*)memalign(IMAGE_ROW_DEFAULT_ALIGNMENT, uv_size), uv_size, [](uint8_t* p) { free(p); });
  memset((void*)y_data.data(), 0, size);
  memset((void*)u_data.data(), 128, uv_size);
  memset((void*)v_data.data(), 128, uv_size);
  frame::Plane y(row,    width,    height,    move(y_data));
  frame::Plane u(uv_row, uv_width, uv_height, move(u_data));
  frame::Plane v(uv_row, uv_width, uv_height, move(v_data));
  _this = new _YUV(move(y), move(u), move(v), full_range);
}

YUV::YUV(YUV&& yuv) : YUV(move(yuv._this->y), move(yuv._this->u), move(yuv._this->v), yuv._this->full_range) {}

YUV::YUV(const YUV& yuv)
  : domain::Interval<YUV, std::function<common::Data16(PlaneIndex)>, uint16_t>(0, yuv.height()) {
  _this = yuv._this->ref();
}

YUV::~YUV() {
  _this->unref();
}

auto YUV::width() const -> uint16_t {
  return _this->y.width();
}

auto YUV::height() const -> uint16_t {
  return _this->y.height();
}

auto YUV::uv_ratio() const -> pair<uint8_t, uint8_t> {
  return make_pair((width() + 1) / _this->u.width(), (height() + 1) / _this->u.height());
}

auto YUV::full_range() const -> bool {
  return _this->full_range;
}

auto YUV::plane(PlaneIndex index) const -> const Plane& {
  switch (index) {
    case Y: return _this->y;
    case U: return _this->u;
    case V: return _this->v;
  }
}

auto YUV::rgb(uint8_t component_count) -> RGB {
  THROW_IF(component_count < 3 || component_count > 4, InvalidArguments);
  frame::RGB rgb(width(), height(), component_count);
  uint8_t* const src[] = {
    (uint8_t* const)plane(frame::Y).bytes().data(),
    (uint8_t* const)plane(frame::U).bytes().data(),
    (uint8_t* const)plane(frame::V).bytes().data()
  };
  const int src_stride[] = {
    plane(frame::Y).row(),
    plane(frame::U).row(),
    plane(frame::V).row()
  };
  uint8_t* const dst[] = { (uint8_t* const)rgb.plane().bytes().data() };
  const int dst_stride[] = { rgb.plane().row() };
  AVPixelFormat src_format = AV_PIX_FMT_NONE;
  if (uv_ratio().first == 2 && uv_ratio().second == 2) {
    src_format = AV_PIX_FMT_YUV420P;
  } else if (uv_ratio().first == 2 && uv_ratio().second == 1) {
    src_format = AV_PIX_FMT_YUV422P;
  }
  CHECK(src_format != AV_PIX_FMT_NONE);
  AVPixelFormat dst_format = AV_PIX_FMT_NONE;
  if (component_count == 3) {
    dst_format = AV_PIX_FMT_RGB24;
  } else if (component_count == 4) {
    dst_format = AV_PIX_FMT_RGBA;
  }
  CHECK(dst_format != AV_PIX_FMT_NONE);
  struct SwsContext* img_convert_ctx = sws_getContext(width(), height(), src_format, width(), height(),
                                                      dst_format, SWS_LANCZOS, NULL, NULL, NULL);
  if (full_range()) {
    sws_setColorspaceDetails(img_convert_ctx, sws_getCoefficients(SWS_CS_DEFAULT), AVCOL_RANGE_JPEG,
                             sws_getCoefficients(SWS_CS_DEFAULT), AVCOL_RANGE_UNSPECIFIED,
                             0, 1 << 16, 1 << 16);
  }
  CHECK(img_convert_ctx);
  sws_scale(img_convert_ctx, src, src_stride, 0, height(), dst, dst_stride);
  sws_freeContext(img_convert_ctx);
  return rgb;
}

auto YUV::full_range(bool full_range) -> YUV {
  THROW_IF(_this->full_range == full_range, InvalidArguments);
  frame::YUV new_yuv(width(), height(), uv_ratio().first, uv_ratio().second, full_range);
  uint8_t* const src[] = {
    (uint8_t* const)plane(frame::Y).bytes().data(),
    (uint8_t* const)plane(frame::U).bytes().data(),
    (uint8_t* const)plane(frame::V).bytes().data()
  };
  uint8_t* const dst[] = {
    (uint8_t* const)new_yuv.plane(frame::Y).bytes().data(),
    (uint8_t* const)new_yuv.plane(frame::U).bytes().data(),
    (uint8_t* const)new_yuv.plane(frame::V).bytes().data()
  };
  const int src_stride[] = {
    plane(frame::Y).row(),
    plane(frame::U).row(),
    plane(frame::V).row()
  };
  const int dst_stride[] = {
    new_yuv.plane(frame::Y).row(),
    new_yuv.plane(frame::U).row(),
    new_yuv.plane(frame::V).row()
  };
  const int width[] = {
    plane(frame::Y).width(),
    plane(frame::U).width(),
    plane(frame::V).width()
  };
  const int height[] = {
    plane(frame::Y).height(),
    plane(frame::U).height(),
    plane(frame::V).height()
  };

#define CLIP3(x, low, high) ({\
  __typeof__(x) _x = x;\
  (((_x) < (low)) ? (low) : ((_x) > (high)) ? (high) : (_x));\
})

  const int16_t   _0 =   0;
  const int16_t  _16 =  16;
  const int16_t _219 = 219;
  const int16_t _224 = 224;
  const int16_t _128 = 128;
  const int16_t _235 = 235;
  const int16_t _240 = 240;
  const int16_t _255 = 255;
  if (full_range) {
    int p = frame::Y;
    for (int y = 0; y < height[p]; ++y) {
      uint8_t* sptr = src[p] + y * src_stride[p];
      uint8_t* dptr = dst[p] + y * dst_stride[p];
      for (int x = 0, w = width[p]; x < w; ++x) {
        *dptr++ = (uint8_t)CLIP3((int16_t)_255 * ((int16_t)*sptr++ - _16) / _219, _0, _255);
      }
    }
    for (auto p: enumeration::Enum<frame::PlaneIndex>(frame::U, frame::V)) {
      for (int y = 0; y < height[p]; ++y) {
        uint8_t* sptr = src[p] + y * src_stride[p];
        uint8_t* dptr = dst[p] + y * dst_stride[p];
        for (int x = 0, w = width[p]; x < w; ++x) {
          *dptr++ = (uint8_t)CLIP3((int16_t)_255 * ((int16_t)*sptr++ - _128) / _224 + _128, _0, _255);
        }
      }
    }
  } else {
    int p = frame::Y;
    for (int y = 0; y < height[p]; ++y) {
      uint8_t* sptr = src[p] + y * src_stride[p];
      uint8_t* dptr = dst[p] + y * dst_stride[p];
      for (int x = 0, w = width[p]; x < w; ++x) {
        *dptr++ = (uint8_t)CLIP3((int16_t)_219 * *sptr++ / _255 + _16, _16, _235);
      }
    }
    for (auto p: enumeration::Enum<frame::PlaneIndex>(frame::U, frame::V)) {
      for (int y = 0; y < height[p]; ++y) {
        uint8_t* sptr = src[p] + y * src_stride[p];
        uint8_t* dptr = dst[p] + y * dst_stride[p];
        for (int x = 0, w = width[p]; x < w; ++x) {
          *dptr++ = (uint8_t)CLIP3((int16_t)_224 * ((int16_t)*sptr++ - _128) / _255 + _128, _16, _240);
        }
      }
    }
  }
  return new_yuv;
}

auto YUV::crop(uint16_t x_offset, uint16_t y_offset, uint16_t cropped_width, uint16_t cropped_height) const -> YUV {
  THROW_IF(uv_ratio().first != 2 || uv_ratio().second != 2, Unsupported);
  THROW_IF(!(cropped_width > 0 && cropped_height > 0 && cropped_width <= 8192 && cropped_height <= 8192), InvalidArguments);
  THROW_IF(x_offset + cropped_width > width() || y_offset + cropped_height > height(), InvalidArguments);

  unique_ptr<ImageYUV> src_yuv(as_imagecore(*this));
  ImageRegion bounding_box(cropped_width, cropped_height, x_offset, y_offset);

  frame::YUV new_yuv(cropped_width, cropped_height, uv_ratio().first, uv_ratio().second, full_range());
  unique_ptr<ImageYUV> dst_yuv(as_imagecore(new_yuv));

  src_yuv->crop(bounding_box);
  src_yuv->copy(dst_yuv.get());

  return new_yuv;
}

auto YUV::rotate(Rotation direction) -> YUV {
  THROW_IF(uv_ratio().first != 2 || uv_ratio().second != 2, Unsupported);
  THROW_IF(direction == Rotation::None, InvalidArguments);
  const bool flip_coords = direction == Rotation::Left || direction == Rotation::Right;
  const uint16_t new_width  = flip_coords ? height() : width();
  const uint16_t new_height = flip_coords ? width() : height();
  const uint16_t new_uv_x_ratio = flip_coords ? uv_ratio().second : uv_ratio().first;
  const uint16_t new_uv_y_ratio = flip_coords ? uv_ratio().first  : uv_ratio().second;
  frame::YUV new_yuv(new_width, new_height, new_uv_x_ratio, new_uv_y_ratio, full_range());

  unique_ptr<ImageYUV> src(as_imagecore(*this));
  unique_ptr<ImageYUV> dst(as_imagecore(new_yuv));

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

  return new_yuv;
}

auto YUV::stretch(int num_x, int denum_x, int num_y, int denum_y, bool high_quality) -> YUV {
  THROW_IF(uv_ratio().first != 2 || uv_ratio().second != 2, Unsupported);
  THROW_IF(!(num_x < 10000 && denum_x < 10000 && num_x >= 0 && denum_x >= 0), InvalidArguments);
  THROW_IF(!(num_y < 10000 && denum_y < 10000 && num_y >= 0 && denum_y >= 0), InvalidArguments);
  const uint32_t new_width = common::round_divide((uint32_t)width(), (uint32_t)num_x, (uint32_t)denum_x);
  const uint32_t new_height = common::round_divide((uint32_t)height(), (uint32_t)num_y, (uint32_t)denum_y);

  unique_ptr<ImageYUV> src_yuv(as_imagecore(*this));

  THROW_IF(new_width > numeric_limits<uint16_t>::max(), Overflow);
  THROW_IF(new_height > numeric_limits<uint16_t>::max(), Overflow);
  frame::YUV new_yuv((uint16_t)new_width, (uint16_t)new_height, uv_ratio().first, uv_ratio().second, full_range());
  unique_ptr<ImageYUV> dst_yuv(as_imagecore(new_yuv));

  bool is_up_sample = (num_x > denum_x) || (num_y > denum_y);
  EResizeQuality resize_quality = high_quality ? kResizeQuality_High : (is_up_sample ? kResizeQuality_Low : kResizeQuality_Bilinear);
  src_yuv->resize(dst_yuv.get(), resize_quality);

  return new_yuv;
}

}}
