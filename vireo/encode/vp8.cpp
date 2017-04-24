//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/vp8.h"
#include "vireo/error/error.h"
#include "vireo/frame/frame.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

namespace vireo {
using common::Data16;

namespace encode {

struct _VP8 {
  bool initialized = false;
  unsigned long deadline = 0;
  unique_ptr<vpx_codec_ctx_t, function<void(vpx_codec_ctx_t*)>> codec = { new vpx_codec_ctx_t(), [&](vpx_codec_ctx_t* codec) {
    if (initialized) {
      vpx_codec_destroy(codec);
    }
    delete codec;
  }};
  functional::Video<frame::Frame> frames;
};

VP8::VP8(const functional::Video<frame::Frame>& frames, int quantizer, int optimization, float fps, int max_bitrate)
  : functional::DirectVideo<VP8, Sample>(frames.a(), frames.b()), _this(new _VP8()) {
  THROW_IF(_this->frames.count() >= security::kMaxSampleCount, Unsafe);
  THROW_IF(quantizer < kVP8MinQuantizer || quantizer > kVP8MaxQuantizer, InvalidArguments);
  THROW_IF(optimization < kVP8MinOptimization || optimization > kVP8MaxOptimization, InvalidArguments);
  THROW_IF(max_bitrate < 0, InvalidArguments);
  THROW_IF(fps <= 0.0f, InvalidArguments);
  THROW_IF(!security::valid_dimensions(frames.settings().width, frames.settings().height), Unsafe);

  const vpx_codec_iface_t* codec_iface = vpx_codec_vp8_cx();
  CHECK(codec_iface);

  vpx_codec_enc_cfg_t cfg;
  {  // Codec config
    CHECK(vpx_codec_enc_config_default(codec_iface, &cfg, 0) == VPX_CODEC_OK);

    cfg.g_w = frames.settings().width;
    cfg.g_h = frames.settings().height;
    cfg.g_timebase.num = 1000;
    float effective_fps = max(fps, 1.0f); // VP8 does not accept fps < 1.0f
    cfg.g_timebase.den = (int)(effective_fps * 1000.0f + 0.5f);
    if (max_bitrate == 0) {
      cfg.rc_end_usage = VPX_Q;
      cfg.rc_max_quantizer = quantizer;
    } else {
      cfg.rc_end_usage = VPX_VBR;
      cfg.rc_target_bitrate = max_bitrate;
      cfg.rc_min_quantizer = quantizer;
    }
    cfg.g_error_resilient = 0;
    cfg.g_threads = 0;
  }
  {  // Encoder
    THROW_IF(vpx_codec_enc_init(_this->codec.get(), codec_iface, &cfg, 0) != VPX_CODEC_OK, InvalidArguments);
    _this->initialized = true;

    if (optimization == 0) {
      _this->deadline = VPX_DL_REALTIME;
    } else if (optimization == 1) {
      _this->deadline = VPX_DL_GOOD_QUALITY;
    } else {
      _this->deadline = VPX_DL_BEST_QUALITY;
    }
  }
  _this->frames = frames;
  _settings = frames.settings();
  _settings.codec = settings::Video::Codec::VP8;
}

VP8::VP8(const VP8& vp8)
  : functional::DirectVideo<VP8, Sample>(vp8.a(), vp8.b()), _this(vp8._this) {
}

auto VP8::operator()(uint32_t index) const -> Sample {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->frames.count(), OutOfRange);

  const frame::Frame& frame = _this->frames(index);
  const frame::YUV yuv = frame.yuv();

  vpx_image_t raw;
  CHECK(vpx_img_wrap(&raw, VPX_IMG_FMT_I420, yuv.width(), yuv.height(), IMAGE_ROW_DEFAULT_ALIGNMENT, NULL) == &raw);
  raw.planes[0] = (unsigned char*)yuv.plane(frame::PlaneIndex::Y).bytes().data();
  raw.planes[1] = (unsigned char*)yuv.plane(frame::PlaneIndex::U).bytes().data();
  raw.planes[2] = (unsigned char*)yuv.plane(frame::PlaneIndex::V).bytes().data();
  raw.stride[0] = yuv.plane(frame::PlaneIndex::Y).row();
  raw.stride[1] = yuv.plane(frame::PlaneIndex::U).row();
  raw.stride[2] = yuv.plane(frame::PlaneIndex::V).row();

  CHECK(vpx_codec_encode(_this->codec.get(), &raw, index, 1, 0, _this->deadline) == VPX_CODEC_OK);
  vpx_img_free(&raw);

  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t* pkt = vpx_codec_get_cx_data(_this->codec.get(), &iter);
  CHECK(pkt && pkt->kind == VPX_CODEC_CX_FRAME_PKT);
  CHECK(pkt->data.frame.sz && pkt->data.frame.sz < 8 * 1048576 && pkt->data.frame.buf);
  CHECK((pkt->data.frame.flags & 0xf) == 0 || (pkt->data.frame.flags & 0xf) == VPX_FRAME_IS_KEY);
  auto data = common::Data32((uint8_t*)pkt->data.frame.buf, (uint32_t)pkt->data.frame.sz, NULL);

  return Sample((uint64_t)frame.pts, (uint64_t)frame.pts, (bool)(pkt->data.frame.flags & VPX_FRAME_IS_KEY), SampleType::Video, data);
}

}}
