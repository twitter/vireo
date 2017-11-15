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

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/functional/media.hpp"
#include "vireo/internal/decode/types.h"

namespace vireo {
namespace internal {
namespace decode {

class PUBLIC H264_BYTESTREAM final : public functional::DirectVideo<H264_BYTESTREAM, std::function<RawSample(void)>> {
  std::shared_ptr<struct _H264_BYTESTREAM> _this;
public:
  H264_BYTESTREAM(common::Data32&& data);
  H264_BYTESTREAM(const H264_BYTESTREAM& h264_annexb);
  DISALLOW_ASSIGN(H264_BYTESTREAM);
  auto sps_pps() const -> header::SPS_PPS;
  auto operator()(uint32_t index) const -> std::function<RawSample(void)>;
};

}}}
