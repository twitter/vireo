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

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/dependency.hpp"
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

  template<settings::Video::Codec codec, typename std::enable_if<codec == settings::Video::Codec::H264 && has_video_decoder<codec>::value>::type* = nullptr>
  void process(const functional::Video<Sample>& video_track, uint32_t thread_count) {
    track = internal::decode::H264(move(video_track), thread_count);
  }
  template<settings::Video::Codec codec, typename std::enable_if<codec == settings::Video::Codec::H264 && !has_video_decoder<codec>::value>::type* = nullptr>
  void process(const functional::Video<Sample>& video_track, uint32_t thread_count) {
    THROW_IF(true, MissingDependency);
  }
};

Video::Video(const functional::Video<Sample>& track, uint32_t thread_count) : functional::DirectVideo<Video, frame::Frame>(), _this(new _Video) {
  const auto& settings = track.settings();
  THROW_IF(settings.codec != settings::Video::Codec::H264 && !settings::Video::IsImage(settings.codec), Unsupported);
  THROW_IF(!track(0).keyframe, Invalid, "Video has to start with a keyframe");
  if (settings.codec == settings::Video::Codec::H264) {
    _this->process<settings::Video::Codec::H264>(track, thread_count);
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
