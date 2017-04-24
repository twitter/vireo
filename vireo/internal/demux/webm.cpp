//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <functional>
#include <stdio.h>

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/header/header.h"
#include "vireo/internal/demux/webm.h"
#include "vireo/settings/settings.h"
#include "vireo/types.h"
#include "mkvparser.hpp"
#include "mkvreader.hpp"

namespace vireo {
namespace internal {
namespace demux {

const static uint32_t kTimescale = 100000;  // we will normalize to millisecond scale to fit pts / dts to uint32_t

struct WebMReader : public mkvparser::IMkvReader {
  common::Reader reader;
  WebMReader(common::Reader&& reader) : reader(move(reader)) {}
  int Length(long long* total, long long* available) {
    if (total) {
      *total = reader.size();
    }
    if (available) {
      *available = reader.size();
    }
    return 0;
  }
  int Read(long long offset, long len, unsigned char* buffer) {
    if (offset < 0 || len < 0 || offset + len >= reader.size()) {
      return -1;
    }
    if (len) {
      common::Data32 data = reader.read((uint32_t)offset, (uint32_t)len);
      THROW_IF(data.count() != len, ReaderError);
      memcpy(buffer, data.data(), len);
    }
    return 0;
  }
};

struct _WebM {
  WebMReader reader;
  bool initialized = false;
  uint64_t duration_in_ns = 0;

  struct Track {
    uint64_t track_ID = 0;
    uint32_t timescale = 0;
    uint64_t duration = 0;
    vector<Sample> samples;
  };
  class {
    Track _tracks[2];
  public:
    auto operator()(const SampleType type) -> Track& {
      uint32_t i = type - SampleType::Video;
      return _tracks[i];
    };
  } tracks;

  struct {
    settings::Video::Codec codec = settings::Video::Codec::Unknown;
    uint16_t width = 0;
    uint16_t height = 0;
    header::SPS_PPS sps_pps = header::SPS_PPS(common::Data16(), common::Data16(), 2);  // SPS / PPS does not exist for WebM video, mock it
  } video;

  struct {
    settings::Audio::Codec codec = settings::Audio::Codec::Unknown;
    uint8_t channels = 0;
    uint32_t bitrate = 0;
  } audio;

  _WebM(common::Reader&& reader) : reader(move(reader)) {}

  bool finish_initialization() {
    long long pos = 0;
    mkvparser::EBMLHeader ebml_header;
    ebml_header.Parse(&reader, pos);

    unique_ptr<mkvparser::Segment, function<void(mkvparser::Segment*)>> segment = { nullptr, [](mkvparser::Segment* ptr){ delete ptr; } };
    mkvparser::Segment* segment_ptr;
    THROW_IF(mkvparser::Segment::CreateInstance(&reader, pos, segment_ptr) != 0, Invalid);
    segment.reset(segment_ptr);
    CHECK(segment);
    THROW_IF(segment->Load() < 0, Invalid);

    const mkvparser::SegmentInfo* const segment_info = segment->GetInfo();
    CHECK(segment_info);
    THROW_IF(segment_info->GetTimeCodeScale() != kMicroSecondScale, Unsupported);
    duration_in_ns = segment_info->GetDuration();  // in nanoseconds
    THROW_IF(!duration_in_ns, Unsupported);

    const mkvparser::Tracks* const parser_tracks = segment->GetTracks();
    CHECK(parser_tracks);

    // Parse track info
    const uint64_t num_tracks = parser_tracks->GetTracksCount();
    for (uint32_t track_number = 0; track_number < num_tracks; ++track_number) {
      const mkvparser::Track* const track = parser_tracks->GetTrackByIndex(track_number);
      if (!track) {
        continue;
      }

      const uint64_t track_type = track->GetType();
      SampleType type = SampleType::Unknown;
      if (track_type == mkvparser::Track::kVideo) {
        type = SampleType::Video;
      } else if (track_type == mkvparser::Track::kAudio){
        type = SampleType::Audio;
      } else {
        continue;
      }

      if (tracks(type).track_ID) {  // Already found a track of the same type
        continue;
      }
      tracks(type).track_ID = track->GetNumber();
      CHECK(tracks(type).track_ID);

      if (type == SampleType::Video) {
        const mkvparser::VideoTrack* const video_track = static_cast<const mkvparser::VideoTrack*>(track);
        CHECK(video_track);
        video.width = video_track->GetWidth();
        video.height = video_track->GetHeight();
        tracks(type).timescale = kTimescale;
        if (string(track->GetCodecId()) == "V_VP8") {
          video.codec = settings::Video::Codec::VP8;
        }
        THROW_IF(!security::valid_dimensions(video.width, video.height), Unsafe);
        THROW_IF(video.codec != settings::Video::Codec::VP8, Unsupported);
      } else {
        const mkvparser::AudioTrack* const audio_track = static_cast<const mkvparser::AudioTrack*>(track);
        audio.channels = audio_track->GetChannels();
        tracks(type).timescale = audio_track->GetSamplingRate();
        THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), tracks(type).timescale) == kSampleRate.end(), Unsupported);
        if (string(track->GetCodecId()) == "A_VORBIS") {
          audio.codec = settings::Audio::Codec::Vorbis;
        }
        THROW_IF(!audio.channels, Invalid);
        THROW_IF(audio.channels > 2, Unsupported);
        THROW_IF(audio.codec != settings::Audio::Codec::Vorbis, Unsupported);
      }
      tracks(type).duration = common::round_divide(duration_in_ns, (uint64_t)tracks(type).timescale, kNanoSecondScale);
    }

    // Parse samples for existing tracks
    const mkvparser::Cluster* cluster = segment->GetFirst();
    while (cluster && !cluster->EOS()) {
      const mkvparser::BlockEntry* block_entry = nullptr;
      THROW_IF(cluster->GetFirst(block_entry) != 0, Invalid);
      while (block_entry && !block_entry->EOS()) {
        const mkvparser::Block* const block = block_entry->GetBlock();
        CHECK(block);
        THROW_IF(block->IsInvisible(), Unsupported);
        THROW_IF(block->GetFrameCount() != 1, Unsupported);  // > 1 frame per block is allowed but we don't know how to deal with them!
        const uint64_t trackNum = block->GetTrackNumber();
        const mkvparser::Track* const parser_track = parser_tracks->GetTrackByNumber(static_cast<unsigned long>(trackNum));
        CHECK(parser_track);
        const uint64_t track_type = parser_track->GetType();

        SampleType type = SampleType::Unknown;
        if (track_type == mkvparser::Track::kVideo) {
          type = SampleType::Video;
        } else if (track_type == mkvparser::Track::kAudio) {
          type = SampleType::Audio;
        }
        THROW_IF(!tracks(type).track_ID, Invalid);

        // Parse sample data and add to track
        if (type == SampleType::Video || type == SampleType::Audio) {
          // pts / dts
          const uint64_t time_ns = block->GetTime(cluster);  // in nanoseconds
          uint64_t ts = common::round_divide(time_ns, (uint64_t)tracks(type).timescale, kNanoSecondScale);
          THROW_IF(ts > numeric_limits<uint32_t>::max(), Overflow);
          // keyframe
          const bool keyframe = block->IsKey();
          // nal, pos, size
          const mkvparser::Block::Frame& frame = block->GetFrame(0);
          THROW_IF(frame.pos > numeric_limits<uint32_t>::max(), Overflow);
          THROW_IF(frame.len > numeric_limits<uint32_t>::max(), Overflow);
          auto nal = [reader = &reader, pos = frame.pos, len = frame.len]() -> common::Data32 {
            common::Data32 data(new uint8_t[len], (uint32_t)len, [](uint8_t* ptr){ delete[] ptr; });
            THROW_IF(reader->Read(pos, len, (uint8_t*)data.data()) != 0, Invalid);
            return move(data);
          };
          if (type == SampleType::Audio) {
            audio.bitrate += frame.len;
          }
          // add sample to track
          Sample sample((uint32_t)ts, (uint32_t)ts, keyframe, type, nal, (uint32_t)frame.pos, (uint32_t)frame.len);
          tracks(type).samples.push_back(sample);

          THROW_IF(cluster->GetNext(block_entry, block_entry) != 0, Invalid);
        }
      }
      cluster = segment->GetNext(cluster);
    }
    if (tracks(SampleType::Audio).duration) {
      audio.bitrate = (uint32_t)common::round_divide((uint64_t)audio.bitrate,
                                                     (uint64_t)tracks(SampleType::Audio).timescale * CHAR_BIT,
                                                     (uint64_t)tracks(SampleType::Audio).duration);
    }
    initialized = true;
    return true;
  }
};

WebM::WebM(common::Reader&& reader)
  : _this(make_shared<_WebM>(move(reader))), audio_track(_this), video_track(_this) {

  if (_this->finish_initialization()) {
    video_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Video).samples.size());
    audio_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Audio).samples.size());
    THROW_IF(!video_track.count() && !audio_track.count(), Invalid);

    if (_this->tracks(SampleType::Video).track_ID) {
      video_track._settings = (settings::Video){
        _this->video.codec,
        _this->video.width,
        _this->video.height,
        _this->tracks(SampleType::Video).timescale,
        settings::Video::Orientation::Landscape,
        _this->video.sps_pps
      };
    }
    if (_this->tracks(SampleType::Audio).track_ID) {
      audio_track._settings = (settings::Audio){
        _this->audio.codec,
        _this->tracks(SampleType::Audio).timescale,
        _this->tracks(SampleType::Audio).timescale, // sample_rate is same as timescale
        _this->audio.channels,
        _this->audio.bitrate
      };
    }
  } else {
    THROW_IF(true, Uninitialized);
  }
}

WebM::WebM(WebM&& webm) : audio_track(_this), video_track(_this) {
  _this = webm._this;
  webm._this = NULL;
}

WebM::VideoTrack::VideoTrack(const std::shared_ptr<_WebM>& _webm_this) : _this(_webm_this) {}

WebM::VideoTrack::VideoTrack(const VideoTrack& video_track)
  : functional::DirectVideo<VideoTrack, Sample>(video_track.a(), video_track.b()), _this(video_track._this) {}

auto WebM::VideoTrack::duration() const -> uint64_t {
  THROW_IF(!_this->initialized, Uninitialized);
  return _this->tracks(SampleType::Video).duration;
}

auto WebM::VideoTrack::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0.0f;
}

auto WebM::VideoTrack::operator()(const uint32_t index) const -> Sample {
  THROW_IF(!_this->initialized, Uninitialized);
  THROW_IF(index >= b(), OutOfRange);
  THROW_IF(index >= _this->tracks(SampleType::Video).samples.size(), OutOfRange);
  return _this->tracks(SampleType::Video).samples[index];
}

WebM::AudioTrack::AudioTrack(const std::shared_ptr<_WebM>& _webm_this) : _this(_webm_this) {}

WebM::AudioTrack::AudioTrack(const AudioTrack& audio_track)
  : functional::DirectAudio<AudioTrack, Sample>(audio_track.a(), audio_track.b()), _this(audio_track._this) {}

auto WebM::AudioTrack::duration() const -> uint64_t {
  THROW_IF(!_this->initialized, Uninitialized);
  return _this->tracks(SampleType::Audio).duration;
}

auto WebM::AudioTrack::operator()(const uint32_t index) const -> Sample {
  THROW_IF(!_this->initialized, Uninitialized);
  THROW_IF(index >= b(), OutOfRange);
  THROW_IF(index >= _this->tracks(SampleType::Audio).samples.size(), OutOfRange);
  return _this->tracks(SampleType::Audio).samples[index];
}

}}}
