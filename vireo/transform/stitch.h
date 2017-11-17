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
