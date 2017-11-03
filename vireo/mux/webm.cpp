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

#include "mkvmuxer.hpp"
#include "mkvwriter.hpp"
#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/util.h"
#include "vireo/error/error.h"
#include "vireo/mux/webm.h"
#include "vireo/version.h"
#include "vireo/types.h"

namespace vireo {
namespace mux {

struct _WebM {
  struct Writer : public mkvmuxer::MkvWriter {
    static const size_t kSize_Default = 512 * 1024;
    unique_ptr<common::Data32> movie = NULL;
    mkvmuxer::int64 Position() const {
      return movie.get() ? movie->a() : 0;
    }
    mkvmuxer::int32 Position(mkvmuxer::int64 position) {
      if (!movie.get() || position < 0 || position > movie->b()) {
        return 1;
      }
      movie->set_bounds((uint32_t)position, movie->b());
      return 0;
    }
    bool Seekable() const {
      return true;
    }
    mkvmuxer::int32 Write(const void* buffer, mkvmuxer::uint32 length) {
      if (length > security::kMaxWriteSize) {
        return 1;
      }
      if (!movie || length + movie->a() > movie->capacity()) {
        const uint32_t new_capacity = movie ? movie->capacity() + common::align_divide(length + movie->a() - movie->capacity(), (uint32_t)kSize_Default) : common::align_divide(length, (uint32_t)kSize_Default);
        common::Data32* new_data = new common::Data32(new uint8_t[new_capacity], new_capacity, [](uint8_t* p) { delete[] p; });
        THROW_IF(!new_data->data(), OutOfMemory);
        if (movie) {
          new_data->set_bounds(movie->a(), movie->b());
          memcpy((uint8_t*)new_data->data(), movie->data(), movie->b());
        } else {
          new_data->set_bounds(0, 0);
        }
        movie.reset(new_data);
      }
      memcpy((uint8_t*)movie->data() + movie->a(), buffer, length);
      movie->set_bounds(movie->a() + length, std::max((uint32_t)movie->b(), (uint32_t)(movie->a() + length)));
      return 0;
    }
    void ElementStartNotify(mkvmuxer::uint64 element_id, mkvmuxer::int64 position) {}
  };
  Writer writer;
  mkvmuxer::Segment muxer_segment;
  uint32_t movie_timescale;
  struct Track {
    uint32_t track_ID = 0;
    uint32_t timescale = 0;
    uint64_t num_frames = 0;
  };
  class {
    Track _tracks[2];
  public:
    auto operator()(const SampleType type) -> Track& {
      uint32_t i = type - SampleType::Video;
      return _tracks[i];
    };
  } tracks;
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
  bool initialized = false;
  bool finalized = false;

  auto mux(const encode::Sample& sample) -> void {
    THROW_IF(sample.nal.count() >= 0x400000, Unsafe);
    THROW_IF(sample.type != SampleType::Video && sample.type != SampleType::Audio, InvalidArguments);
    THROW_IF(tracks(SampleType::Audio).num_frames >= security::kMaxSampleCount, Unsafe);
    THROW_IF(tracks(SampleType::Video).num_frames >= security::kMaxSampleCount, Unsafe);

    THROW_IF(!sample.nal.data(), Invalid);
    THROW_IF(muxer_segment.AddFrame((const mkvmuxer::uint8*)sample.nal.data(),
                                    sample.nal.count(),
                                    tracks(sample.type).track_ID,
                                    common::round_divide((uint64_t)sample.pts * kMicroSecondScale,
                                                         (uint64_t)kMicroSecondScale,
                                                         (uint64_t)tracks(sample.type).timescale * kMilliSecondScale),
                                    sample.keyframe) == false, InvalidArguments);
    ++tracks(sample.type).num_frames;
  }

  auto flush() -> void {
    if (!finalized) {
      THROW_IF(!initialized, Uninitialized);

      order_samples(tracks(SampleType::Audio).timescale, audio,
                    tracks(SampleType::Video).timescale, video,
                    [this](const encode::Sample& sample) {
                      mux(sample);
                    });

      CHECK(muxer_segment.Finalize());
      writer.Close();
      writer.movie->set_bounds(0, writer.movie->b());
      finalized = true;
    }
  }
};

WebM::WebM(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video)
  : _this(new _WebM()) {
  THROW_IF(!audio.count() && !video.count(), InvalidArguments);
  THROW_IF(video.settings().timescale == 0 && audio.settings().sample_rate == 0.0, InvalidArguments);
  THROW_IF(video.settings().sps_pps.sps.count() >= security::kMaxHeaderSize, Unsafe);
  THROW_IF(video.settings().sps_pps.pps.count() >= security::kMaxHeaderSize, Unsafe);
  if (video.settings().timescale) {
    THROW_IF(video.settings().orientation != settings::Video::Orientation::Landscape, Unsupported);
    THROW_IF(!security::valid_dimensions(video.settings().width, video.settings().height), Unsafe);
  }
  CHECK(_this->muxer_segment.Init(&_this->writer));

  mkvmuxer::SegmentInfo* const info = _this->muxer_segment.GetSegmentInfo();
  info->set_timecode_scale(kMicroSecondScale);
  char vireo_version[64];
  snprintf(vireo_version, 64, "Vireo Feet v%s", VIREO_VERSION);
  info->set_writing_app(vireo_version);
  snprintf(vireo_version, 64, "Vireo Wings v%s", VIREO_VERSION);
  info->set_muxing_app(vireo_version);

  uint32_t track_id = 0;
  if (video.settings().timescale != 0.0) {  // Video
    _this->tracks(SampleType::Video).track_ID = ++track_id;
    THROW_IF(_this->muxer_segment.AddVideoTrack(video.settings().width,
                                                video.settings().height,
                                                _this->tracks(SampleType::Video).track_ID) != _this->tracks(SampleType::Video).track_ID, InvalidArguments);

    mkvmuxer::VideoTrack* const track = static_cast<mkvmuxer::VideoTrack*>(_this->muxer_segment.GetTrackByNumber(_this->tracks(SampleType::Video).track_ID));
    CHECK(track);
    track->set_display_width(video.settings().width);
    track->set_display_height(video.settings().height);

    _this->tracks(SampleType::Video).timescale = video.settings().timescale;
    _this->video = video;
  }

  if (audio.settings().sample_rate != 0.0) {  // Audio
    _this->tracks(SampleType::Audio).track_ID = ++track_id;
    THROW_IF(_this->muxer_segment.AddAudioTrack(audio.settings().sample_rate,
                                                audio.settings().channels,
                                                _this->tracks(SampleType::Audio).track_ID) != _this->tracks(SampleType::Audio).track_ID, InvalidArguments);

    mkvmuxer::AudioTrack* const track = static_cast<mkvmuxer::AudioTrack*>(_this->muxer_segment.GetTrackByNumber(_this->tracks(SampleType::Audio).track_ID));
    CHECK(track);
    auto codec_private = audio.settings().as_extradata(settings::Audio::ExtraDataType::vorbis);
    track->SetCodecPrivate(codec_private.data(), codec_private.count());
    track->set_bit_depth(CHAR_BIT * sizeof(int16_t));

    _this->tracks(SampleType::Audio).timescale = audio.settings().timescale;
    _this->audio = audio;
  }

  _this->initialized = true;

  *static_cast<std::function<common::Data32(void)>*>(this) = [_this = _this]() {
    THROW_IF(!_this->initialized, Uninitialized);
    _this->flush();
    CHECK(_this->writer.movie.get());
    return *_this->writer.movie.get();
  };
}

WebM::WebM(const WebM& webm)
  : Function<common::Data32>(*static_cast<const functional::Function<common::Data32>*>(&webm)), _this(webm._this) {
}

WebM::WebM(WebM&& webm)
  : Function<common::Data32>(*static_cast<const functional::Function<common::Data32>*>(&webm)), _this(webm._this) {
  webm._this = NULL;
}

}}
