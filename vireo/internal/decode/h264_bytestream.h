//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
