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
static const int kDefaultH264KeyintMax = 1 << 30;
static const int kDefaultH264KeyintMin = 0;

enum RCMethod {
  CRF = 0,
  CBR = 1,
  ABR = 2
};

enum PyramidMode {
  None = 0,
  Strict = 1,
  Normal = 2
};

enum VideoProfileType {
  ConstrainedBaseline = 0,
  Baseline = 1,
  Main = 2,
  High = 3
};

enum AdaptiveQuantizationMode {
  AqNone = 0,
  Variance = 1,
  AutoVariance = 2,
  AutoVarianceBiased = 3
};

enum MotionEstimationMethod {
  Diamond = 0,
  Hexagon = 1,
  MultiHex = 2,
  Exhaustive = 3,
  TransformedExhaustive = 4
};

struct H264Params {
  struct ComputationalParams {
    uint32_t optimization;
    uint32_t thread_count;
    ComputationalParams(uint32_t optimization = 3,
                        uint32_t thread_count = 0)
      : optimization(optimization), thread_count(thread_count) {
      THROW_IF(optimization < kH264MinOptimization || optimization > kH264MaxOptimization, InvalidArguments);
      THROW_IF(thread_count < kH264MinThreadCount || thread_count > kH264MaxThreadCount, InvalidArguments);
    };
  } computation;

  struct RateControlParams {
    RCMethod rc_method;
    float crf;
    uint32_t max_bitrate;
    uint32_t bitrate;
    uint32_t buffer_size;
    float buffer_init; // <=1: fraction of buffer_size. >1: kbit
    uint32_t look_ahead;
    bool is_second_pass; // if this is the second pass for a dual-pass encoding
    bool enable_mb_tree; // if to enable mb_tree based rate control
    AdaptiveQuantizationMode aq_mode; // adaptive quatization mode
    uint32_t qp_min; // minimum qp value
    string stats_log_path; // input/output path for the intermediate log file in dual-pass encoding
    bool mixed_refs; // if to select refs on 8x8 partition
    // trellis quatization mode
    // - 0: disabled
    // - 1: enabled only on the final encode of a MB
    // - 2: enabled on all mode decisions
    uint32_t trellis;
    MotionEstimationMethod me_method;
    // subpixel motion estimation quality
    // - 0: fullpel only
    // - 1: SAD mode decision, one qpel iteration
    // - 2: SATD mode decision
    // - 3-5: Progressively more qpel
    // - 6: RD mode decision for I/P-frames
    // - 7: RD mode decision for all frames
    // - 8: RD refinement for I/P-frames
    // - 9: RD refinement for all frames
    // - 10: QP-RD - requires trellis=2, aq-mode>0
    // - 11: Full RD: disable all early terminations
    uint32_t subpel_refine;
    RateControlParams(RCMethod rc_method = RCMethod::CRF,
                      float crf = 28.0f,
                      uint32_t max_bitrate = 0,
                      uint32_t bitrate = 0,
                      uint32_t buffer_size = 0,
                      float buffer_init = 0.0f,
                      uint32_t look_ahead = 40,
                      bool is_second_pass = false,
                      bool enable_mb_tree = true,
                      AdaptiveQuantizationMode aq_mode = AdaptiveQuantizationMode::Variance,
                      uint32_t qp_min = 0,
                      string stats_log_path = "",
                      bool mixed_refs = true,
                      uint32_t trellis = 1,
                      MotionEstimationMethod me_method = MotionEstimationMethod::Hexagon,
                      uint32_t subpel_refine = 7)
      : rc_method(rc_method), crf(crf), max_bitrate(max_bitrate), bitrate(bitrate), buffer_size(buffer_size), buffer_init(buffer_init), look_ahead(look_ahead), is_second_pass(is_second_pass), enable_mb_tree(enable_mb_tree), aq_mode(aq_mode), qp_min(qp_min), stats_log_path(stats_log_path), mixed_refs(mixed_refs), trellis(trellis), me_method(me_method), subpel_refine(subpel_refine) {
      if (rc_method == RCMethod::CRF) {
        THROW_IF(crf < kH264MinCRF || crf > kH264MaxCRF, InvalidArguments);
      }
      if (rc_method == RCMethod::CBR) {
        THROW_IF(bitrate != max_bitrate, InvalidArguments, "CBR requires same bitrate and max_bitrate");
      }
      THROW_IF(trellis > 2, InvalidArguments, "trellis should be [0,2]");
      THROW_IF(qp_min > 69, InvalidArguments, "qp_min should be [0, 69]");
      THROW_IF(subpel_refine > 11, InvalidArguments, "subpel_refine should be [0, 11]");
    };
  } rc;

  struct GopParams {
    int32_t num_bframes;
    PyramidMode pyramid_mode;   /* Keep some B-frames as references: 0=off, 1=strict hierarchical, 2=normal */
    uint32_t keyint_max; // maximum key frame interval
    uint32_t keyint_min;
    uint32_t frame_references;
    GopParams(int32_t num_bframes = -1,
              PyramidMode mode = PyramidMode::Normal,
              uint32_t keyint_max = kDefaultH264KeyintMax,
              uint32_t keyint_min = kDefaultH264KeyintMin,
              uint32_t frame_references = 3)
      : num_bframes(num_bframes), pyramid_mode(mode), keyint_max(keyint_max), keyint_min(keyint_min), frame_references(frame_references) {};
  } gop;

  // Other Params
  VideoProfileType profile;
  float fps;

  H264Params(const ComputationalParams& computation,
             const RateControlParams& rc,
             const GopParams& gop,
             const VideoProfileType profile,
             float fps = 30.0)
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
