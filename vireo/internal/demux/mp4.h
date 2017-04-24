//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/editbox.h"
#include "vireo/common/reader.h"
#include "vireo/decode/types.h"

namespace vireo {
namespace internal {
namespace demux {

using namespace vireo::decode;

class PUBLIC MP4 final {
  std::shared_ptr<struct _MP4> _this = nullptr;
public:
  MP4(common::Reader&& reader);
  MP4(MP4&& mp4);
  DISALLOW_COPY_AND_ASSIGN(MP4);

  class VideoTrack final : public functional::DirectVideo<VideoTrack, Sample> {
    std::shared_ptr<_MP4> _this;
    VideoTrack(const std::shared_ptr<_MP4>& _this);
    friend class MP4;
  public:
    VideoTrack(const VideoTrack& video_track);
    DISALLOW_ASSIGN(VideoTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto fps() const -> float;
    auto operator()(const uint32_t index) const -> Sample;
  } video_track;

  class AudioTrack final : public functional::DirectAudio<AudioTrack, Sample> {
    std::shared_ptr<_MP4> _this;
    AudioTrack(const std::shared_ptr<_MP4>& _this);
    friend class MP4;
  public:
    AudioTrack(const AudioTrack& audio_track);
    DISALLOW_ASSIGN(AudioTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(const uint32_t index) const -> Sample;
  } audio_track;

  class CaptionTrack final : public functional::DirectCaption<CaptionTrack, Sample> {
    std::shared_ptr<_MP4> _this;
    CaptionTrack(const std::shared_ptr<_MP4>& _this);
    friend class MP4;
  public:
    CaptionTrack(const CaptionTrack& caption_track);
    DISALLOW_ASSIGN(CaptionTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(uint32_t index) const -> Sample;
  } caption_track;
};

}}}
