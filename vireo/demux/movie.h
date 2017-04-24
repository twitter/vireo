//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/editbox.h"
#include "vireo/common/reader.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace demux {

class PUBLIC Movie final {
  std::shared_ptr<struct _Movie> _this = NULL;
public:
  Movie(common::Reader&& reader);
  Movie(Movie&& movie);
  DISALLOW_COPY_AND_ASSIGN(Movie);
  auto file_type() -> FileType;

  class VideoTrack final : public functional::DirectVideo<VideoTrack, decode::Sample> {
    std::shared_ptr<_Movie> _this;
    VideoTrack(const std::shared_ptr<_Movie>& _this);
    friend class Movie;
  public:
    VideoTrack(const VideoTrack& video_track);
    DISALLOW_ASSIGN(VideoTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto fps() const -> float;
    auto operator()(const uint32_t index) const -> decode::Sample;
  } video_track;

  class AudioTrack final : public functional::DirectAudio<AudioTrack, decode::Sample> {
    std::shared_ptr<_Movie> _this;
    AudioTrack(const std::shared_ptr<_Movie>& _this);
    friend class Movie;
  public:
    AudioTrack(const AudioTrack& audio_track);
    DISALLOW_ASSIGN(AudioTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(const uint32_t index) const -> decode::Sample;
  } audio_track;

  class DataTrack final : public functional::DirectData<DataTrack, decode::Sample> {
    std::shared_ptr<_Movie> _this;
    DataTrack(const std::shared_ptr<_Movie>& _this);
    friend class Movie;
  public:
    DataTrack(const DataTrack& data_track);
    DISALLOW_ASSIGN(DataTrack);
    auto operator()(const uint32_t index) const -> decode::Sample;
  } data_track;

  class CaptionTrack final : public functional::DirectCaption<CaptionTrack, decode::Sample> {
    std::shared_ptr<_Movie> _this;
    CaptionTrack(const std::shared_ptr<_Movie>& _this);
    friend class Movie;
  public:
    CaptionTrack(const CaptionTrack& caption_track);
    DISALLOW_ASSIGN(CaptionTrack);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(const uint32_t index) const -> decode::Sample;
  } caption_track;
};

}}
