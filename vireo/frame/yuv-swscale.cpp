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
#include "libswscale/swscale.h"
}
#include "vireo/frame/rgb.h"
#include "vireo/frame/yuv.h"

namespace vireo {
namespace frame {

template <>
auto YUV::rgb<std::true_type>(uint8_t component_count) -> RGB {
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

}}
