//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/editbox.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace transform {

class PUBLIC Stitch final {
  std::shared_ptr<struct _Stitch> _this = NULL;
public:
  Stitch(const vector<functional::Video<decode::Sample>> video_tracks,
         const vector<vector<common::EditBox>> edit_boxes_per_track = vector<vector<common::EditBox>>())
    : Stitch(vector<functional::Audio<decode::Sample>>(), video_tracks, edit_boxes_per_track) {};
  Stitch(const vector<functional::Audio<decode::Sample>> audio_tracks,
         const vector<functional::Video<decode::Sample>> video_tracks,
         const vector<vector<common::EditBox>> edit_boxes_per_track = vector<vector<common::EditBox>>());
  Stitch(const Stitch& stitch);
  Stitch(Stitch&& stitch);
  DISALLOW_ASSIGN(Stitch);

  class VideoTrack final : public functional::DirectVideo<VideoTrack, decode::Sample> {
    std::shared_ptr<_Stitch> _this;
    VideoTrack(const std::shared_ptr<_Stitch>& _this);
    friend class Stitch;
  public:
    VideoTrack(const VideoTrack& video_track);
    DISALLOW_ASSIGN(VideoTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto fps() const -> float;
    auto operator()(const uint32_t index) const -> decode::Sample;
  } video_track;

  class AudioTrack final : public functional::DirectAudio<AudioTrack, decode::Sample> {
    std::shared_ptr<_Stitch> _this;
    AudioTrack(const std::shared_ptr<_Stitch>& _this);
    friend class Stitch;
  public:
    AudioTrack(const AudioTrack& audio_track);
    DISALLOW_ASSIGN(AudioTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(const uint32_t index) const -> decode::Sample;
  } audio_track;
};

}}
