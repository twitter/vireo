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

extern "C" {
#include "libavutil/frame.h"
#include "libavutil/opt.h"
#include "libswscale/swscale.h"
}
#include "vireo/base_cpp.h"
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

template <typename Available = has_swscale>
auto stretch_swscale(const RGB& rgb, int num_x, int denum_x, int num_y, int denum_y) -> RGB;

template <>
auto RGB::rgb<std::true_type>(uint8_t component_count) const -> RGB {
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

template <>
auto RGB::yuv<std::true_type>(uint8_t uv_x_ratio, uint8_t uv_y_ratio) const -> YUV {
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

template <>
auto stretch_swscale<std::true_type>(const RGB& rgb, int num_x, int denum_x, int num_y, int denum_y) -> RGB {
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

}}
