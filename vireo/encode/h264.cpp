//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/encode/h264.h"
#include "vireo/error/error.h"
extern "C" {
#include "x264.h"
}

#define X264_CSP X264_CSP_I420
#define X264_LOG_LEVEL X264_LOG_WARNING
#define X264_NALU_LENGTH_SIZE 4
#define X264_PROFILE "main"
#define X264_TUNE "ssim"
const static uint32_t kMaxDelay = 64;

static vireo::encode::VideoProfileType GetDefaultProfile(const int width, const int height) {
  auto min_dimension = min(width, height);
  if (min_dimension <= 480) {
    return vireo::encode::VideoProfileType::Baseline;
  } else if (min_dimension <= 720) {
    return vireo::encode::VideoProfileType::Main;
  } else {
    return vireo::encode::VideoProfileType::High;
  }
}

namespace vireo {
using common::Data16;

namespace encode {

struct _H264 {
  unique_ptr<x264_t, decltype(&x264_encoder_close)> encoder = { NULL, [](x264_t* encoder) { if (encoder) x264_encoder_close(encoder); } };
  functional::Video<frame::Frame> frames;
  uint32_t num_cached_frames = 0;
  static inline const char* const GetProfile(VideoProfileType profile) {
    THROW_IF(profile != VideoProfileType::Baseline && profile != VideoProfileType::Main && profile != VideoProfileType::High, Unsupported, "unsupported profile type");
    switch (profile) {
      case VideoProfileType::Baseline:
        return "baseline";
      case VideoProfileType::Main:
        return "main";
      case VideoProfileType::High:
        return "high";
      default:
        return "baseline";
    }
  }
};

H264::H264(const functional::Video<frame::Frame>& frames, float crf, uint32_t optimization, float fps, uint32_t max_bitrate, uint32_t thread_count)
  : H264(frames, H264Params(H264Params::ComputationalParams(optimization, thread_count), H264Params::RateControlParams(RCMethod::CRF, crf, max_bitrate), H264Params::GopParams(0), GetDefaultProfile(frames.settings().width, frames.settings().height), fps)) {};

H264::H264(const functional::Video<frame::Frame>& frames, const H264Params& params)
  : functional::DirectVideo<H264, Sample>(frames.a(), frames.b()), _this(new _H264()) {
  THROW_IF(frames.count() >= security::kMaxSampleCount, Unsafe);
  THROW_IF(params.computation.optimization < kH264MinOptimization || params.computation.optimization > kH264MaxOptimization, InvalidArguments);
  THROW_IF(params.fps < 0.0f, InvalidArguments);
  THROW_IF(params.rc.max_bitrate < 0.0f, InvalidArguments);
  THROW_IF(params.computation.thread_count < kH264MinThreadCount || params.computation.thread_count > kH264MaxThreadCount, InvalidArguments);
  THROW_IF(!security::valid_dimensions(frames.settings().width, frames.settings().height), Unsafe);
  THROW_IF(frames.settings().par_width != frames.settings().par_height, InvalidArguments);

  x264_param_t param;
  {  // Params
    x264_param_default_preset(&param, x264_preset_names[params.computation.optimization], X264_TUNE);
    param.i_threads = params.computation.thread_count ? params.computation.thread_count : 1;
    param.i_log_level = X264_LOG_LEVEL;
    param.i_width = (int)((frames.settings().width + 1) / 2) * 2;
    param.i_height = (int)((frames.settings().height + 1) / 2) * 2;
    param.i_fps_num = (int)(params.fps * 1000.0f + 0.5f);
    param.i_fps_den = params.fps > 0.0f ? 1000 : 0.0;
    param.i_csp = X264_CSP;
    param.b_annexb = 0;
    param.b_repeat_headers = 0;
    param.b_vfr_input = 0;
    param.b_sliced_threads = 1; // use slice based multithreading
    if (params.gop.num_bframes >= 0) { // if num_bframes < 0, use default settings
      param.i_bframe = params.gop.num_bframes;
      param.i_bframe_pyramid = params.gop.pyramid_mode;
    }
    if (param.i_bframe == 0) { // set these zerolatency settings in case number of b frames is 0, otherwise use default settings
      param.rc.i_lookahead = 0;
      param.i_sync_lookahead = 0;
      param.rc.b_mb_tree = 0;
    }

    switch (params.rc.method) {
      case RCMethod::CRF:
        param.rc.i_rc_method = X264_RC_CRF;
        THROW_IF(params.rc.crf < kH264MinCRF || params.rc.crf > kH264MaxCRF, InvalidArguments);
        param.rc.f_rf_constant = params.rc.crf;
        if (params.rc.max_bitrate != 0.0) {
          param.rc.i_vbv_max_bitrate = params.rc.max_bitrate;
          param.rc.i_vbv_buffer_size = params.rc.max_bitrate;
        }
        break;
      case RCMethod::CBR:
        CHECK(params.rc.bitrate == params.rc.max_bitrate);
        param.rc.i_rc_method = X264_RC_ABR;
        param.rc.i_bitrate = params.rc.bitrate;
        param.rc.i_vbv_max_bitrate = params.rc.bitrate;
        param.rc.i_vbv_buffer_size = params.rc.buffer_size;
        param.rc.f_vbv_buffer_init = params.rc.buffer_init;
        break;
      case RCMethod::ABR:
        param.rc.i_rc_method = X264_RC_ABR;
        param.rc.i_vbv_max_bitrate = params.rc.max_bitrate;
        param.rc.i_vbv_buffer_size = params.rc.buffer_size;
        param.rc.f_vbv_buffer_init = params.rc.buffer_init;
        break;

      default: // USE CRF as default mode
        param.rc.i_rc_method = X264_RC_CRF;
        param.rc.f_rf_constant = 28.0f;
        break;
    }

    THROW_IF(x264_param_apply_profile(&param, _H264::GetProfile(params.profile)) < 0, InvalidArguments);
  }
  _this->frames = frames;
  {  // Encoder
    _this->encoder.reset(x264_encoder_open(&param));
    CHECK(_this->encoder);
  }
  _settings = frames.settings();
  _settings.codec = settings::Video::Codec::H264;
  {
    x264_nal_t* nals;
    int count;
    THROW_IF(x264_encoder_headers(_this->encoder.get(), &nals, &count) < 0, InvalidArguments);
    CHECK(count >= 3);
    _settings.sps_pps = (header::SPS_PPS){
      Data16(nals[0].p_payload + X264_NALU_LENGTH_SIZE, nals[0].i_payload - X264_NALU_LENGTH_SIZE, NULL),
      Data16(nals[1].p_payload + X264_NALU_LENGTH_SIZE, nals[1].i_payload - X264_NALU_LENGTH_SIZE, NULL),
      X264_NALU_LENGTH_SIZE
    };
  }
}

H264::H264(const H264& h264)
  : functional::DirectVideo<H264, Sample>(h264.a(), h264.b(), h264.settings()), _this(h264._this) {
}

auto H264::operator()(uint32_t index) const -> Sample {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->frames.count(), OutOfRange);

  x264_nal_t* nals = nullptr;
  int i_nals;
  x264_picture_t out_picture;
  int size = 0;
  auto has_more_frames_to_encode = [_this = _this, &index]() -> bool {
    return index + _this->num_cached_frames < _this->frames.count();
  };
  if (has_more_frames_to_encode()) {
    while (size == 0 && has_more_frames_to_encode()) {
      const frame::Frame frame = _this->frames(index + _this->num_cached_frames);
      const uint64_t pts = frame.pts;
      const frame::YUV yuv = frame.yuv();

      x264_picture_t in_picture;
      x264_picture_init(&in_picture);
      in_picture.i_pts = pts;

      in_picture.img.i_csp = X264_CSP;
      in_picture.img.i_plane = 3;
      in_picture.img.plane[0] = (uint8_t*)yuv.plane(frame::Y).bytes().data();
      in_picture.img.plane[1] = (uint8_t*)yuv.plane(frame::U).bytes().data();
      in_picture.img.plane[2] = (uint8_t*)yuv.plane(frame::V).bytes().data();
      in_picture.img.i_stride[0] = (int)yuv.plane(frame::Y).row();
      in_picture.img.i_stride[1] = (int)yuv.plane(frame::U).row();
      in_picture.img.i_stride[2] = (int)yuv.plane(frame::V).row();
      size = x264_encoder_encode(_this->encoder.get(), &nals, &i_nals, &in_picture, &out_picture);
      _this->num_cached_frames += (size == 0);
      THROW_IF(_this->num_cached_frames > kMaxDelay, Unsupported);
    }
  }
  if (!has_more_frames_to_encode()) {
    CHECK(_this->num_cached_frames > 0);
    size = x264_encoder_encode(_this->encoder.get(), &nals, &i_nals, nullptr, &out_picture); // flush out cached frames
    _this->num_cached_frames--;
  }
  CHECK(size > 0);
  CHECK(nals);
  CHECK(i_nals != 0);
  CHECK(out_picture.i_pts >= 0);
  const auto nal = common::Data32(nals[0].p_payload, size, NULL);
  return Sample(out_picture.i_pts,  out_picture.i_dts, (bool)out_picture.b_keyframe, SampleType::Video, nal);
}

}}