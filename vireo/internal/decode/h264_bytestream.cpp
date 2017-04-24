//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/annexb.h"
#include "vireo/internal/decode/h264_bytestream.h"
#include "vireo/internal/decode/util.h"

#define NALU_LENGTH_SIZE 4

namespace vireo {
namespace internal {
namespace decode {

struct _H264_BYTESTREAM {
  common::Data32 data;
  vector<RawSample> samples;
  unique_ptr<header::SPS_PPS> sps_pps = NULL;
  _H264_BYTESTREAM(common::Data32&& data) : data(move(data)) {}
  bool finish_initialization() {
    ANNEXB<H264NalType> annexb_parser(data);
    uint32_t index = 0;

    // Parse SPS
    auto sps_info = annexb_parser(index++);
    THROW_IF(sps_info.type != H264NalType::SPS, Invalid);
    common::Data16 sps = common::Data16(data.data() + sps_info.byte_offset, sps_info.size, NULL);

    // Parse PPS
    auto pps_info = annexb_parser(index++);
    THROW_IF(pps_info.type != H264NalType::PPS, Invalid);
    common::Data16 pps = common::Data16(data.data() + pps_info.byte_offset, pps_info.size, NULL);

    // Save SPS/PPS
    sps_pps.reset(new header::SPS_PPS(sps, pps, NALU_LENGTH_SIZE));

    // Parse frames
    annexb_parser.set_bounds(index, annexb_parser.b());
    for (auto info: annexb_parser) {
      THROW_IF(info.type != H264NalType::IDR && info.type != H264NalType::FRM, Invalid);
      bool keyframe = (info.type == H264NalType::IDR) ? true : false;
      common::Data32 nal = common::Data32(data.data() + info.byte_offset, info.size, NULL);
      samples.push_back({ keyframe, move(nal) });
    }
    return true;
  }
};

H264_BYTESTREAM::H264_BYTESTREAM(common::Data32&& data)
  : _this(new _H264_BYTESTREAM(move(data))) {
  CHECK(data.count());
  if (_this->finish_initialization()) {
    set_bounds(0, (uint32_t)_this->samples.size());
  } else {
    _this->sps_pps.reset(NULL);
    THROW_IF(true, Uninitialized);
  }
}

H264_BYTESTREAM::H264_BYTESTREAM(const H264_BYTESTREAM& h264_annexb)
  : _this(h264_annexb._this), functional::DirectVideo<H264_BYTESTREAM, std::function<RawSample(void)>>(h264_annexb.a(), h264_annexb.b()) {
}

auto H264_BYTESTREAM::sps_pps() const -> header::SPS_PPS {
  return *_this->sps_pps;
}

auto H264_BYTESTREAM::operator()(uint32_t index) const -> function<RawSample(void)> {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  return [_this = _this, index]() -> RawSample {
    auto sample = _this->samples[index];
    auto size = sample.nal.count();
    common::Data32 nal = common::Data32(new uint8_t[size + NALU_LENGTH_SIZE],
                                        size + NALU_LENGTH_SIZE,
                                        [](uint8_t* bytes){ delete[] bytes; });
    write_nal_size(nal, size, NALU_LENGTH_SIZE);
    nal.set_bounds(NALU_LENGTH_SIZE, nal.b());
    nal.copy(sample.nal);
    nal.set_bounds(0, nal.b());
    return { sample.keyframe, nal };
  };
}

}}}
