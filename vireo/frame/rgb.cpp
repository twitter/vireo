//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "imagecore/image/rgba.h"
#include "imagecore/image/yuv.h"
extern "C" {
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
}
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

auto RGB::rgb(uint8_t component_count) const -> RGB {
  THROW_IF(component_count != 3 && component_count != 4, InvalidArguments);
  THROW_IF(component_count == this->component_count(), InvalidArguments);
  frame::RGB rgb(width(), height(), component_count);
  uint8_t* const src[] = { (uint8_t* const)plane().bytes().data() };
  const int src_stride[] = { plane().row() };
  uint8_t* const dst[] = { (uint8_t* const)rgb.plane().bytes().data() };
  const int dst_stride[] = { rgb.plane().row() };
  AVPixelFormat src_format = AV_PIX_FMT_NONE;
  if (this->component_count() == 3) {
    src_format = AV_PIX_FMT_RGB24;
  } else if (this->component_count() == 4) {
    src_format = AV_PIX_FMT_RGBA;
  }
  CHECK(src_format != AV_PIX_FMT_NONE);
  AVPixelFormat dst_format = AV_PIX_FMT_NONE;
  if (component_count == 3) {
    dst_format = AV_PIX_FMT_RGB24;
  } else if (component_count == 4) {
    dst_format = AV_PIX_FMT_RGBA;
  }
  CHECK(dst_format != AV_PIX_FMT_NONE);
  struct SwsContext* img_convert_ctx = sws_getContext(width(), height(), src_format,
                                                      width(), height(), dst_format,
                                                      SWS_LANCZOS, NULL, NULL, NULL);
  CHECK(img_convert_ctx);
  sws_scale(img_convert_ctx, src, src_stride, 0, height(), dst, dst_stride);
  sws_freeContext(img_convert_ctx);
  return rgb;
}

auto RGB::yuv(uint8_t uv_x_ratio, uint8_t uv_y_ratio) const -> YUV {
  frame::YUV yuv(width(), height(), uv_x_ratio, uv_y_ratio);
  uint8_t* const src[] = { (uint8_t* const)plane().bytes().data() };
  const int src_stride[] = { plane().row() };
  uint8_t* const dst[] = {
    (uint8_t* const)yuv.plane(frame::Y).bytes().data(),
    (uint8_t* const)yuv.plane(frame::U).bytes().data(),
    (uint8_t* const)yuv.plane(frame::V).bytes().data()
  };
  const int dst_stride[] = {
    yuv.plane(frame::Y).row(),
    yuv.plane(frame::U).row(),
    yuv.plane(frame::V).row()
  };
  AVPixelFormat src_format = AV_PIX_FMT_NONE;
  if (component_count() == 3) {
    src_format = AV_PIX_FMT_RGB24;
  } else if (component_count() == 4) {
    src_format = AV_PIX_FMT_RGBA;
  }
  CHECK(src_format != AV_PIX_FMT_NONE);
  AVPixelFormat dst_format = AV_PIX_FMT_NONE;
  if (uv_x_ratio == 2 && uv_y_ratio == 2) {
    dst_format = AV_PIX_FMT_YUV420P;
  } else if (uv_x_ratio == 2 && uv_y_ratio == 1) {
    dst_format = AV_PIX_FMT_YUV422P;
  }
  CHECK(dst_format != AV_PIX_FMT_NONE);
  // Need to do custom init to work around ffmpeg bug with sws_getContext, that doesn't set
  // lumConvertRange to ctx->lumRangeToJpeg_c and ctx->chrConvertRange to chrRangeToJpeg_c otherwise.
  SwsContext* img_convert_ctx = sws_alloc_context();
  if (yuv.full_range()) {
    av_opt_set_int(img_convert_ctx, "srcw", width(), 0);
    av_opt_set_int(img_convert_ctx, "srch", height(), 0);
    av_opt_set_int(img_convert_ctx, "src_format", src_format, 0);
    av_opt_set_int(img_convert_ctx, "dstw", width(), 0);
    av_opt_set_int(img_convert_ctx, "dsth", height(), 0);
    av_opt_set_int(img_convert_ctx, "dst_format", dst_format, 0);
    sws_setColorspaceDetails(img_convert_ctx, sws_getCoefficients(SWS_CS_DEFAULT), AVCOL_RANGE_JPEG,
                                              sws_getCoefficients(SWS_CS_DEFAULT), AVCOL_RANGE_JPEG,
                                              0, 1 << 16, 1 << 16);
    int32_t res = sws_init_context(img_convert_ctx, NULL, NULL);
    CHECK(res >= 0);
  }
  sws_scale(img_convert_ctx, src, src_stride, 0, height(), dst, dst_stride);
  sws_freeContext(img_convert_ctx);
  return yuv;
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

static auto stretch_ffmpeg(const RGB& rgb, int num_x, int denum_x, int num_y, int denum_y) -> RGB {
  THROW_IF(!(num_x < 10000 && denum_x < 10000 && num_x >= 0 && denum_x >= 0), InvalidArguments);
  THROW_IF(!(num_y < 10000 && denum_y < 10000 && num_y >= 0 && denum_y >= 0), InvalidArguments);
  const uint32_t new_width = common::round_divide((uint32_t)rgb.width(), (uint32_t)num_x, (uint32_t)denum_x);
  const uint32_t new_height = common::round_divide((uint32_t)rgb.height(), (uint32_t)num_y, (uint32_t)denum_y);

  THROW_IF(new_width > numeric_limits<uint16_t>::max(), Overflow);
  THROW_IF(new_height > numeric_limits<uint16_t>::max(), Overflow);
  frame::RGB new_rgb((uint16_t)new_width, (uint16_t)new_height, rgb.component_count());

  AVPixelFormat format = AV_PIX_FMT_NONE;
  if (rgb.component_count() == 3) {
    format = AV_PIX_FMT_RGB24;
  } else if (rgb.component_count() == 4) {
    format = AV_PIX_FMT_RGBA;
  }
  CHECK(format != AV_PIX_FMT_NONE);
  SwsContext* img_convert_ctx = sws_getContext(rgb.width(), rgb.height(), format, new_width, new_height,
                                               format, SWS_LANCZOS, NULL, NULL, NULL);
  CHECK(img_convert_ctx);

  // sws_scale checks for all 4 components (r, g, b, a) even though we pass in a packed format
  // to avoid valgrind issues we just pass the same pointer and stride 4 times
  uint8_t* const src_ptr = (uint8_t* const)rgb.plane().bytes().data();
  const int src_stride = rgb.plane().row();
  uint8_t* const dst_ptr = (uint8_t* const)new_rgb.plane().bytes().data();
  const int dst_stride = new_rgb.plane().row();

  uint8_t* const src[] = { src_ptr, src_ptr, src_ptr, src_ptr };
  const int srcStride[] = { src_stride, src_stride, src_stride, src_stride };
  uint8_t* const dst[] = { dst_ptr, dst_ptr, dst_ptr, dst_ptr };
  const int dstStride[] = { dst_stride, dst_stride, dst_stride, dst_stride };

  sws_scale(img_convert_ctx, src, srcStride, 0, rgb.height(), dst, dstStride);
  sws_freeContext(img_convert_ctx);
  return new_rgb;
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
    return stretch_ffmpeg(*this, num_x, denum_x, num_y, denum_y);
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
