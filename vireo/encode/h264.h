//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"
#include "vireo/encode/types.h"
#include "vireo/encode/util.h"
#include "vireo/frame/frame.h"

namespace vireo {
namespace encode {

static const float kH264MinCRF = 0.0;
static const float kH264MaxCRF = 51.0;
static const int kH264MinOptimization = 0;
static const int kH264MaxOptimization = 9;
static const int kH264MinThreadCount = 0;
static const int kH264MaxThreadCount = 64;

struct H264Params {
  struct ComputationalParams {
    uint32_t optimization;
    uint32_t thread_count;
    ComputationalParams(uint32_t optimization = 3, uint32_t thread_count = 0) : optimization(optimization), thread_count(thread_count) {
      THROW_IF(optimization < kH264MinOptimization || optimization > kH264MaxOptimization, InvalidArguments);
      THROW_IF(thread_count < kH264MinThreadCount || thread_count > kH264MaxThreadCount, InvalidArguments);
    };
  } computation;

  struct RateControlParams {
    RCMethod method;
    float crf;
    uint32_t max_bitrate;
    uint32_t bitrate;
    uint32_t buffer_size;
    float buffer_init; // <=1: fraction of buffer_size. >1: kbit
    RateControlParams(RCMethod method = RCMethod::CRF, float crf = 28.0f, uint32_t max_bitrate = 0,
                      uint32_t bitrate = 0, uint32_t buffer_size = 0, float buffer_init = 0.0f) :
                      method(method), crf(crf), max_bitrate(max_bitrate), bitrate(bitrate),
                      buffer_size(buffer_size), buffer_init(buffer_init) {
      if (method == RCMethod::CRF) {
        THROW_IF(crf < kH264MinCRF || crf > kH264MaxCRF, InvalidArguments);
      }
      if (method == RCMethod::CBR) {
        THROW_IF(bitrate != max_bitrate, InvalidArguments, "CBR requires same bitrate and max_bitrate");
      }
    };
  } rc;

  struct GopParams {
    int32_t num_bframes;
    PyramidMode pyramid_mode;   /* Keep some B-frames as references: 0=off, 1=strict hierarchical, 2=normal */
    GopParams(int32_t num_bframes = -1, PyramidMode mode = PyramidMode::Normal) : num_bframes(num_bframes), pyramid_mode(mode) {};
  } gop;

  VideoProfileType profile;

  // Other Params
  float fps;

  H264Params(const ComputationalParams& computation, const RateControlParams& rc, const GopParams& gop, const VideoProfileType profile, float fps = 30.0f)
   : computation(computation), rc(rc), gop(gop), profile(profile), fps(fps) {};
};

class PUBLIC H264 final : public functional::DirectVideo<H264, Sample> {
  std::shared_ptr<struct _H264> _this;
public:
  H264(const functional::Video<frame::Frame>& frames, float crf, uint32_t optimization, float fps, uint32_t max_bitrate = 0, uint32_t thread_count = 0);
  H264(const functional::Video<frame::Frame>& frames, const H264Params& params);
  H264(const H264& h264);
  DISALLOW_ASSIGN(H264);
  auto operator()(uint32_t sample) const -> Sample;
};

}}
