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
#include "vireo/common/reader.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace internal {
namespace demux {

using namespace vireo::decode;

static const vector<uint8_t> kWebMFtyp = { 0x1A, 0x45, 0xDF, 0xA3 };

class WebM final {
  std::shared_ptr<struct _WebM> _this = NULL;
public:
  WebM(common::Reader&& reader);
  WebM(WebM&& webm);
  DISALLOW_COPY_AND_ASSIGN(WebM);

  class VideoTrack final : public functional::DirectVideo<VideoTrack, Sample> {
    std::shared_ptr<_WebM> _this;
    VideoTrack(const std::shared_ptr<_WebM>& _this);
    friend class WebM;
  public:
    VideoTrack(const VideoTrack& video_track);
    DISALLOW_ASSIGN(VideoTrack);
    auto duration() const -> uint64_t;
    auto fps() const -> float;
    auto operator()(const uint32_t index) const -> Sample;
  } video_track;

  class AudioTrack final : public functional::DirectAudio<AudioTrack, Sample> {
    std::shared_ptr<_WebM> _this;
    AudioTrack(const std::shared_ptr<_WebM>& _this);
    friend class WebM;
  public:
    AudioTrack(const AudioTrack& audio_track);
    DISALLOW_ASSIGN(AudioTrack);
    auto duration() const -> uint64_t;
    auto operator()(const uint32_t index) const -> Sample;
  } audio_track;
};

}}}
