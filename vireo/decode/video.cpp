//  Copyright (c) 2016 Twitter, Inc. All rights reserved.

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/decode/video.h"
#include "vireo/error/error.h"
#include "vireo/frame/frame.h"
#include "vireo/internal/decode/h264.h"
#include "vireo/internal/decode/image.h"
#include "vireo/util/util.h"

namespace vireo {
namespace decode {

struct _Video {
  functional::Video<frame::Frame> track;
};

Video::Video(const functional::Video<Sample>& track, uint32_t thread_count) : functional::DirectVideo<Video, frame::Frame>(), _this(new _Video) {
  const auto& settings = track.settings();
  THROW_IF(settings.codec != settings::Video::Codec::H264 && !settings::Video::IsImage(settings.codec), Unsupported);
  THROW_IF(!track(0).keyframe, Invalid, "Video has to start with a keyframe");
  if (settings.codec == settings::Video::Codec::H264) {
    _this->track = functional::Video<frame::Frame>(internal::decode::H264(move(track), thread_count));
  } else {
    _this->track = functional::Video<frame::Frame>(internal::decode::Image(move(track)));
  }
  _settings = _this->track.settings();
  THROW_IF(_settings.par_width != _settings.par_height, Unsupported, "Square pixel is expected for decoding output");

  set_bounds(_this->track.a(), _this->track.b());
}

Video::Video(const Video& video)
  : functional::DirectVideo<Video, frame::Frame>(video.a(), video.b(), video.settings()), _this(video._this) {}

auto Video::operator()(uint32_t index) const -> frame::Frame {
  return _this->track(index);
}

}}
