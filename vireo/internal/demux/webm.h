//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
