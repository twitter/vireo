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
#include "vireo/dependency.hpp"
#include "vireo/demux/movie.h"
#include "vireo/internal/demux/image.h"
#include "vireo/internal/demux/mp2ts.h"
#include "vireo/internal/demux/mp4.h"
#include "vireo/internal/demux/webm.h"
#include "vireo/util/ftyp.h"

namespace vireo {
namespace demux {

template <int Type>
struct Track {
  functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type> track;
  uint64_t duration = 0;
  vector<common::EditBox> edit_boxes;
  // used for mitigation of MEDIASERV-4820, MEDIASERV-5667, MEDIASERV-6317, MEDIASERV-6423, MEDIASERV-5384
  vector<uint64_t> unique_pts;
  vector<uint64_t> unique_dts;

  void enforce_unique_pts_dts() {
    // NOTE: We limit the size of existing pts / dts dictionary, so we can potentially miss non-unique
    // pts / dts samples that are more than kMaxLookback apart. However duplicate pts / dts typically
    // happen only for neighboring samples so this optimization should be fine.
    const static uint32_t kMaxLookback = 16;
    const static uint32_t kMaxAdjustments = 32;
    THROW_IF(kMaxAdjustments < kMaxLookback, Invalid);

    set<uint64_t> existing_pts;
    set<uint64_t> existing_dts;
    uint32_t num_adjustments = 0;
    for (const auto& sample: track) {
      uint64_t pts = sample.pts;
      uint64_t dts = sample.dts;
      while (existing_pts.find(pts) != existing_pts.end() || existing_dts.find(dts) != existing_dts.end()) {
        THROW_IF(++num_adjustments > kMaxAdjustments, Unsafe);  // not tested, relax if such a video is received
        pts++;
        dts++;
      }
      if (existing_pts.size() >= kMaxLookback) {
        existing_pts.erase(existing_pts.begin());
        THROW_IF(existing_pts.size() >= kMaxLookback, Unsupported);
      }
      if (existing_dts.size() >= kMaxLookback) {
        existing_dts.erase(existing_dts.begin());
        THROW_IF(existing_dts.size() >= kMaxLookback, Unsupported);
      }
      unique_pts.push_back(pts);
      unique_dts.push_back(dts);
      existing_pts.insert(pts);
      existing_dts.insert(dts);
    }
  }
};

struct _Movie {
  FileType file_type = FileType::UnknownFileType;
  unique_ptr<internal::demux::MP4> mp4_decoder;
  unique_ptr<internal::demux::MP2TS> mp2ts_decoder;
  unique_ptr<internal::demux::WebM> webm_decoder;
  unique_ptr<internal::demux::Image> image_decoder;
  Track<SampleType::Video> video;
  Track<SampleType::Audio> audio;
  Track<SampleType::Data> data;
  Track<SampleType::Caption> caption;
  template <FileType Ftyp, typename Supported = typename has_dependency<Ftyp>::type>
  void parse(common::Reader&& reader) {
    THROW_IF(true, MissingDependency);
  }
};

template<>
void _Movie::parse<FileType::MP4, std::true_type>(common::Reader&& reader) {
  file_type = FileType::MP4;
  mp4_decoder.reset(new internal::demux::MP4(move(reader)));
  video.track = functional::Video<decode::Sample>(mp4_decoder->video_track);
  video.duration = mp4_decoder->video_track.duration();
  video.edit_boxes.insert(video.edit_boxes.end(),
                          mp4_decoder->video_track.edit_boxes().begin(),
                          mp4_decoder->video_track.edit_boxes().end());
  audio.track = functional::Audio<decode::Sample>(mp4_decoder->audio_track);
  audio.duration = mp4_decoder->audio_track.duration();
  audio.edit_boxes.insert(audio.edit_boxes.end(),
                          mp4_decoder->audio_track.edit_boxes().begin(),
                          mp4_decoder->audio_track.edit_boxes().end());
  caption.track = functional::Caption<decode::Sample>(mp4_decoder->caption_track);
  caption.duration = mp4_decoder->caption_track.duration();
  caption.edit_boxes.insert(caption.edit_boxes.end(),
                            mp4_decoder->caption_track.edit_boxes().begin(),
                            mp4_decoder->caption_track.edit_boxes().end());
}

template<>
void _Movie::parse<FileType::MP2TS, std::true_type>(common::Reader&& reader) {
  file_type = FileType::MP2TS;
  mp2ts_decoder.reset(new internal::demux::MP2TS(move(reader)));
  video.track = functional::Video<decode::Sample>(mp2ts_decoder->video_track);
  video.duration = mp2ts_decoder->video_track.duration();
  audio.track = functional::Audio<decode::Sample>(mp2ts_decoder->audio_track);
  audio.duration = mp2ts_decoder->audio_track.duration();
  data.track = functional::Data<decode::Sample>(mp2ts_decoder->data_track);
  caption.track = functional::Caption<decode::Sample>(mp2ts_decoder->caption_track);
  caption.duration = mp2ts_decoder->caption_track.duration();
}

template<>
void _Movie::parse<FileType::WebM, std::true_type>(common::Reader&& reader) {
  file_type = FileType::WebM;
  webm_decoder.reset(new internal::demux::WebM(move(reader)));
  video.track = functional::Video<decode::Sample>(webm_decoder->video_track);
  video.duration = webm_decoder->video_track.duration();
  audio.track = functional::Audio<decode::Sample>(webm_decoder->audio_track);
  audio.duration = webm_decoder->audio_track.duration();
}

template<>
void _Movie::parse<FileType::Image, std::true_type>(common::Reader&& reader) {
  file_type = FileType::Image;
  image_decoder.reset(new internal::demux::Image(move(reader)));
  video.track = functional::Video<decode::Sample>(image_decoder->track);
  video.duration = image_decoder->track.duration();
}

Movie::Movie(common::Reader&& reader) : _this(make_shared<_Movie>()), audio_track(_this), video_track(_this), data_track(_this), caption_track(_this) {
  vector<vector<uint8_t>> supported_ftyps = { internal::demux::kWebMFtyp, internal::demux::kMP2TSFtyp };
  supported_ftyps.insert(supported_ftyps.end(), internal::demux::kImageFtyps.begin(), internal::demux::kImageFtyps.end());

  uint32_t read_len = 0;
  for (auto ftyp: supported_ftyps) {
    read_len = (uint32_t)max(read_len, (uint32_t)ftyp.size());
  }
  common::Data32 data = reader.read(0, read_len);
  THROW_IF(data.count() != read_len, ReaderError, "not enough data to check file ftyp");

  if (util::FtypUtil::Matches(internal::demux::kImageFtyps, data)) {
    _this->parse<FileType::Image>(move(reader));
  } else if (util::FtypUtil::Matches(internal::demux::kMP2TSFtyp, data)) {
    _this->parse<FileType::MP2TS>(move(reader));
  } else if (util::FtypUtil::Matches(internal::demux::kWebMFtyp, data)) {
    _this->parse<FileType::WebM>(move(reader));
  } else {
    _this->parse<FileType::MP4>(move(reader));
  }

  video_track.set_bounds(_this->video.track.a(), _this->video.track.b());
  video_track._settings = _this->video.track.settings();
  _this->video.enforce_unique_pts_dts();
  audio_track.set_bounds(_this->audio.track.a(), _this->audio.track.b());
  audio_track._settings = _this->audio.track.settings();
  _this->audio.enforce_unique_pts_dts();
  data_track.set_bounds(_this->data.track.a(), _this->data.track.b());
  data_track._settings = _this->data.track.settings();
  _this->data.enforce_unique_pts_dts();
  caption_track.set_bounds(_this->caption.track.a(), _this->caption.track.b());
  caption_track._settings = _this->caption.track.settings();
  _this->caption.enforce_unique_pts_dts();
}

Movie::Movie(Movie&& movie) : audio_track(_this), video_track(_this), data_track(_this), caption_track(_this) {
  _this = movie._this;
  movie._this = nullptr;
}

auto Movie::file_type() -> FileType {
  return _this->file_type;
}

Movie::VideoTrack::VideoTrack(const std::shared_ptr<_Movie>& _this) : _this(_this) {}

Movie::VideoTrack::VideoTrack(const VideoTrack& video_track)
  : functional::DirectVideo<VideoTrack, decode::Sample>(video_track.a(), video_track.b()), _this(video_track._this) {}

auto Movie::VideoTrack::duration() const -> uint64_t {
  return _this->video.duration;
}

auto Movie::VideoTrack::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->video.edit_boxes;
}

auto Movie::VideoTrack::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0;
}

auto Movie::VideoTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  auto sample = _this->video.track(index);
  return decode::Sample(sample, _this->video.unique_pts[index], _this->video.unique_dts[index]);
}

Movie::AudioTrack::AudioTrack(const std::shared_ptr<_Movie>& _this) : _this(_this) {}

Movie::AudioTrack::AudioTrack(const AudioTrack& audio_track)
  : functional::DirectAudio<AudioTrack, decode::Sample>(audio_track.a(), audio_track.b()), _this(audio_track._this) {}

auto Movie::AudioTrack::duration() const -> uint64_t {
  return _this->audio.duration;
}

auto Movie::AudioTrack::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->audio.edit_boxes;
}

auto Movie::AudioTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  auto sample = _this->audio.track(index);
  return decode::Sample(sample, _this->audio.unique_pts[index], _this->audio.unique_dts[index]);
}

Movie::DataTrack::DataTrack(const std::shared_ptr<_Movie>& _this) : _this(_this) {}

Movie::DataTrack::DataTrack(const DataTrack& data_track)
  : functional::DirectData<DataTrack, decode::Sample>(data_track.a(), data_track.b()), _this(data_track._this) {}

auto Movie::DataTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  auto sample = _this->data.track(index);
  return decode::Sample(sample, _this->data.unique_pts[index], _this->data.unique_dts[index]);
}

Movie::CaptionTrack::CaptionTrack(const std::shared_ptr<_Movie>& _this) : _this(_this) {}

Movie::CaptionTrack::CaptionTrack(const CaptionTrack& caption_track)
  : functional::DirectCaption<CaptionTrack, decode::Sample>(caption_track.a(), caption_track.b()), _this(caption_track._this) {}

auto Movie::CaptionTrack::duration() const -> uint64_t {
  return _this->caption.duration;
}

auto Movie::CaptionTrack::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->caption.edit_boxes;
}

auto Movie::CaptionTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  auto sample = _this->caption.track(index);
  return decode::Sample(sample, _this->caption.unique_pts[index], _this->caption.unique_dts[index]);
}

}}
