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
#include "vireo/constants.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace internal {
namespace demux {

using namespace vireo::decode;

static const vector<uint8_t> kMP2TSFtyp = { MP2TS_SYNC_BYTE };

class MP2TS final {
  std::shared_ptr<struct _MP2TS> _this = nullptr;
public:
  MP2TS(common::Reader&& reader);
  MP2TS(MP2TS&& mp2ts);
  DISALLOW_COPY_AND_ASSIGN(MP2TS);

  class VideoTrack final : public functional::DirectVideo<VideoTrack, Sample> {
    std::shared_ptr<_MP2TS> _this;
    VideoTrack(const std::shared_ptr<_MP2TS>& _this);
    friend class MP2TS;
  public:
    VideoTrack(const VideoTrack& video_track);
    DISALLOW_ASSIGN(VideoTrack);
    auto duration() const -> uint64_t;
    auto fps() const -> float;
    auto operator()(uint32_t index) const -> Sample;
  } video_track;

  class AudioTrack final : public functional::DirectAudio<AudioTrack, Sample> {
    std::shared_ptr<_MP2TS> _this;
    AudioTrack(const std::shared_ptr<_MP2TS>& _this);
    friend class MP2TS;
  public:
    AudioTrack(const AudioTrack& audio_track);
    DISALLOW_ASSIGN(AudioTrack);
    auto duration() const -> uint64_t;
    auto operator()(uint32_t sample) const -> Sample;
  } audio_track;

  class DataTrack final : public functional::DirectData<DataTrack, Sample> {
    std::shared_ptr<_MP2TS> _this;
    DataTrack(const std::shared_ptr<_MP2TS>& _this);
    friend class MP2TS;
  public:
    DataTrack(const DataTrack& data_track);
    DISALLOW_ASSIGN(DataTrack);
    auto operator()(uint32_t index) const -> Sample;
  } data_track;

  class CaptionTrack final : public functional::DirectCaption<CaptionTrack, Sample> {
    std::shared_ptr<_MP2TS> _this;
    CaptionTrack(const std::shared_ptr<_MP2TS>& _this);
    friend class MP2TS;
  public:
    CaptionTrack(const CaptionTrack& caption_track);
    DISALLOW_ASSIGN(CaptionTrack);
    auto duration() const -> uint64_t;
    auto operator()(uint32_t index) const -> Sample;
  } caption_track;
};

}}}
