//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/common/math.h"
#include "vireo/transform/stitch.h"

namespace vireo {
namespace transform {

template <int Type>
struct Track {
  vector<decode::Sample> samples;
  vector<common::EditBox> edit_boxes;
  settings::Settings<Type> settings;
  uint64_t duration = 0;
  Track() : settings(settings::Settings<Type>::None) {};
};

struct _Stitch {
  Track<SampleType::Video> video;
  Track<SampleType::Audio> audio;

  void adjust_timescale(decode::Sample& sample, const uint32_t new_timescale, const uint32_t org_timescale) {
    THROW_IF(sample.type == SampleType::Audio, InvalidArguments, "cannot change audio timescale without changing sample rate");
    if (org_timescale != new_timescale) {
      sample.pts = common::round_divide<uint64_t>(sample.pts, new_timescale, org_timescale);
      sample.dts = common::round_divide<uint64_t>(sample.dts, new_timescale, org_timescale);
    }
  };

  void adjust_timescale(common::EditBox& edit_box, const uint32_t new_timescale, const uint32_t org_timescale) {
    if (org_timescale != new_timescale) {
      edit_box.start_pts = edit_box.start_pts == EMPTY_EDIT_BOX ? EMPTY_EDIT_BOX : (int64_t)common::round_divide<uint64_t>(edit_box.start_pts, new_timescale, org_timescale);
      edit_box.duration_pts = common::round_divide<uint64_t>(edit_box.duration_pts, new_timescale, org_timescale);
    }
  };

  template <typename T>
  void adjust_timescale(vector<T>& elems, const uint32_t new_timescale, const uint32_t org_timescale) {
    if (org_timescale != new_timescale) {
      for (auto& elem: elems) {
        adjust_timescale(elem, new_timescale, org_timescale);
      }
    }
  }

  void shift_and_append_samples(vector<decode::Sample>& in_samples, vector<decode::Sample>& out_samples, int64_t offset) {
    for (const auto& sample: in_samples) {
      if (offset < 0 && -offset > sample.pts && -offset > sample.dts) {
        // should only happen if very first audio sample pts < very first video sample pts - skip
        CHECK(sample.type == SampleType::Audio);
      } else {
        out_samples.push_back(sample.shift(offset));
      }
    }
  }

  void shift_and_append_editboxes(vector<common::EditBox>& in_edit_boxes, vector<common::EditBox>& out_edit_boxes, int64_t offset) {
    for (const auto& edit_box: in_edit_boxes) {
      out_edit_boxes.push_back(edit_box.shift(offset));
    }
  }

  static uint64_t CalculateDuration(const vector<decode::Sample>& samples) {
    THROW_IF(!samples.size(), InvalidArguments);
    vector<uint64_t> dts_offsets;
    uint64_t track_duration = 0;
    uint64_t prev_dts = 0;
    bool first = true;
    for (const auto& sample: samples) {
      if (first) {
        first = false;
        prev_dts = sample.dts;
        continue;
      }
      THROW_IF(sample.dts < prev_dts, Invalid);
      uint64_t dts_offset = sample.dts - prev_dts;
      track_duration += dts_offset;
      dts_offsets.push_back(dts_offset);
      prev_dts = sample.dts;
    }
    uint64_t last_sample_duration = common::median(dts_offsets);
    track_duration += last_sample_duration;
    return track_duration;
  }

  static uint64_t CalculateDuration(const vector<common::EditBox>& edit_boxes) {
    THROW_IF(!common::EditBox::Valid(edit_boxes), Invalid);
    uint64_t duration = 0;
    for (const auto& edit_box: edit_boxes) {
      duration += edit_box.duration_pts;
    }
    return duration;
  };

  static vector<common::EditBox> FilterBySampleType(const vector<common::EditBox>& edit_boxes, const SampleType type) {
    vector<common::EditBox> filtered;
    for (const auto& edit_box: edit_boxes) {
      if (edit_box.type == type) {
        filtered.push_back(edit_box);
      }
    }
    return filtered;
  }

  static vector<decode::Sample> RemoveOverlappingSamples(const vector<decode::Sample>& samples) {
    vector<decode::Sample> filtered;
    if (samples.size()) {
      decode::Sample last_sample = samples.back();
      filtered.push_back(last_sample);
      for (auto it = samples.rbegin(); it != samples.rend(); ++it) {
        decode::Sample sample = *it;
        CHECK(sample.type == last_sample.type);
        if (sample.pts < last_sample.pts && sample.dts < last_sample.dts) {
          filtered.push_back(sample);
          last_sample = sample;
        }
      }
    }
    reverse(filtered.begin(), filtered.end());
    return filtered;
  };

  _Stitch(const vector<functional::Audio<decode::Sample>>& audio_tracks,
          const vector<functional::Video<decode::Sample>>& video_tracks,
          const vector<vector<common::EditBox>>& edit_boxes_per_track) {
    // Check size of input vectors
    THROW_IF(video_tracks.size() == 0, InvalidArguments, "At least one video track is required");
    THROW_IF(audio_tracks.size() && audio_tracks.size() != video_tracks.size(), InvalidArguments, "Number of audio tracks need to match the number of video tracks");
    THROW_IF(edit_boxes_per_track.size() && edit_boxes_per_track.size() != video_tracks.size(), InvalidArguments, "Number of edit box list need to match the number of video tracks");

    // Check if audio is present in input
    bool input_has_audio = false;
    for (const auto& audio_track: audio_tracks) {
      input_has_audio = input_has_audio | audio_track.count();
    }

    // Check if edit boxes present in input
    bool input_has_edit_boxes = false;
    for (const auto& edit_boxes: edit_boxes_per_track) {
      input_has_edit_boxes = input_has_edit_boxes | edit_boxes.size();
    }

    // All settings must match in order to successfully stitch tracks - use first settings as reference
    video.settings = video_tracks.front().settings();
    if (input_has_audio) {
      audio.settings = audio_tracks.front().settings();
    }

    for (uint32_t i = 0; i < video_tracks.size(); ++i) {
      // Unpack video track - check for settings match - adjust timescale if necessary
      const auto& video_track = video_tracks[i];
      const auto video_settings = video_track.settings();
      THROW_IF(video_settings.codec != video.settings.codec, InvalidArguments);
      THROW_IF(video_settings.width != video.settings.width, InvalidArguments);
      THROW_IF(video_settings.height != video.settings.height, InvalidArguments);
      THROW_IF(video_settings.orientation != video.settings.orientation, InvalidArguments);
      auto video_samples = video_track.vectorize();
      adjust_timescale(video_samples, video.settings.timescale, video_settings.timescale);

      // Unpack audio track (if any) - check for settings match
      auto audio_samples = vector<decode::Sample>();
      if (input_has_audio) {
        const auto& audio_track = audio_tracks[i];
        const auto audio_settings = audio_track.settings();
        THROW_IF(audio_settings.codec != audio.settings.codec, InvalidArguments);
        THROW_IF(audio_settings.timescale != audio.settings.timescale, InvalidArguments);
        THROW_IF(audio_settings.sample_rate != audio.settings.sample_rate, InvalidArguments);
        THROW_IF(audio_settings.channels != audio.settings.channels, InvalidArguments);
        audio_samples = audio_track.vectorize();
      }

      // Unpack edit boxes (if any) - adjust timescale if necessary
      auto video_edit_boxes = vector<common::EditBox>();
      auto audio_edit_boxes = vector<common::EditBox>();
      if (input_has_edit_boxes) {
        auto edit_boxes = edit_boxes_per_track[i];
        video_edit_boxes = FilterBySampleType(edit_boxes, SampleType::Video);
        THROW_IF(!common::EditBox::Valid(video_edit_boxes), Invalid);
        THROW_IF(video_edit_boxes.size() && i != 0 && video_edit_boxes.front().start_pts == EMPTY_EDIT_BOX, InvalidArguments);
        adjust_timescale(video_edit_boxes, video.settings.timescale, video_settings.timescale);
        audio_edit_boxes = FilterBySampleType(edit_boxes, SampleType::Audio);
        THROW_IF(!common::EditBox::Valid(audio_edit_boxes), Invalid);
        THROW_IF(audio_edit_boxes.size() && i != 0 && audio_edit_boxes.front().start_pts == EMPTY_EDIT_BOX, InvalidArguments);
      }

      // Calculate video duration
      THROW_IF(video_samples.size() == 0, InvalidArguments, "Every video track must contain data");
      uint64_t video_duration = CalculateDuration(video_samples);
      if (video_duration == 0) {
        // happens if there's a single frame in track
        THROW_IF(video_samples.size() == 1, Unsupported, "Single frame inputs are not supported");
      }
      CHECK(video_duration != 0);

      // Append video samples and edit boxes
      auto first_video_sample = video_samples.front();
      int64_t video_offset = video.duration - first_video_sample.dts;
      shift_and_append_samples(video_samples, video.samples, video_offset);
      if (input_has_edit_boxes) {
        if (video_edit_boxes.size()) {
          shift_and_append_editboxes(video_edit_boxes, video.edit_boxes, video_offset);
        } else {
          // add an edit box that spans the entire track if missing for current track
          video.edit_boxes.push_back(common::EditBox(video_samples.front().shift(video_offset).dts, video_duration, 1.0f, SampleType::Video));
        }
      }
      video.duration += video_duration;

      if (input_has_audio) {
        // Calculate audio duration based on corresponding video track - trim the end if longer than video duration (actual trimming happens via RemoveOverlappingSamples)
        THROW_IF(audio_samples.size() == 0, InvalidArguments, "Every audio track must contain data");
        uint64_t audio_duration = 0;
        if (audio_edit_boxes.size()) {
          // audio edit boxes present - retain all input samples
          audio_duration = CalculateDuration(audio_samples);
        } else if (video_edit_boxes.size()) {
          // no audio edit boxes - video edit boxes present - trim audio samples beyond video duration from edit boxes
          audio_duration = common::round_divide(CalculateDuration(video_edit_boxes), (uint64_t)audio.settings.timescale, (uint64_t)video.settings.timescale);
        } else {
          // no edit boxes - trim audio samples beyond video samples
          audio_duration = common::round_divide(video_duration, (uint64_t)audio.settings.timescale, (uint64_t)video.settings.timescale);
        }

        // Append audio samples and edit boxes
        auto first_audio_sample = audio_samples.front();
        THROW_IF(first_video_sample.dts < 0, Unsupported);
        int64_t audio_video_gap = first_audio_sample.dts - common::round_divide((uint64_t)first_video_sample.dts, (uint64_t)audio.settings.timescale, (uint64_t)video.settings.timescale);
        int64_t audio_offset = audio.duration - first_audio_sample.dts + audio_video_gap;
        shift_and_append_samples(audio_samples, audio.samples, audio_offset);
        if (input_has_edit_boxes) {
          if (audio_edit_boxes.size()) {
            shift_and_append_editboxes(audio_edit_boxes, audio.edit_boxes, audio_offset);
          } else {
            // add an edit box that spans the entire track if missing for current track
            audio.edit_boxes.push_back(common::EditBox(audio_samples.front().shift(audio_offset).dts, audio_duration, 1.0f, SampleType::Audio));
          }
        }
        audio.duration += audio_duration;
      }
    }
    audio.samples = RemoveOverlappingSamples(audio.samples);  // trim extra audio samples
  }
};

Stitch::Stitch(const vector<functional::Audio<decode::Sample>> audio_tracks,
               const vector<functional::Video<decode::Sample>> video_tracks,
               const vector<vector<common::EditBox>> edit_boxes_per_track)
  : _this(make_shared<_Stitch>(audio_tracks, video_tracks, edit_boxes_per_track)), audio_track(_this), video_track(_this) {
  audio_track._settings = _this->audio.settings;
  audio_track.set_bounds(0, (uint32_t)_this->audio.samples.size());
  video_track._settings = _this->video.settings;
  video_track.set_bounds(0, (uint32_t)_this->video.samples.size());
}

Stitch::Stitch(const Stitch& stitch) : _this(stitch._this), audio_track(_this), video_track(_this) {}

Stitch::Stitch(Stitch&& stitch) : _this(stitch._this), audio_track(_this), video_track(_this) {
  stitch._this = nullptr;
  stitch.audio_track._this = nullptr;
  stitch.video_track._this = nullptr;
}

Stitch::VideoTrack::VideoTrack(const std::shared_ptr<_Stitch>& _this) : _this(_this) {}

Stitch::VideoTrack::VideoTrack(const VideoTrack& video_track)
  : functional::DirectVideo<VideoTrack, decode::Sample>(video_track.a(), video_track.b()), _this(video_track._this) {}

auto Stitch::VideoTrack::duration() const -> uint64_t {
  return _this->video.duration;
}

auto Stitch::VideoTrack::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->video.edit_boxes;
}

auto Stitch::VideoTrack::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0;
}

auto Stitch::VideoTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  CHECK(index < _this->video.samples.size());
  return _this->video.samples[index];
}

Stitch::AudioTrack::AudioTrack(const std::shared_ptr<_Stitch>& _this) : _this(_this) {}

Stitch::AudioTrack::AudioTrack(const AudioTrack& audio_track)
  : functional::DirectAudio<AudioTrack, decode::Sample>(audio_track.a(), audio_track.b()), _this(audio_track._this) {}

auto Stitch::AudioTrack::duration() const -> uint64_t {
  return _this->audio.duration;
}

auto Stitch::AudioTrack::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->audio.edit_boxes;
}

auto Stitch::AudioTrack::operator()(const uint32_t index) const -> decode::Sample {
  THROW_IF(index < a() || index >= b(), OutOfRange,
           "index (" << index << ") has to be in range [" << a() << ", " << b() << ")");
  CHECK(index < _this->audio.samples.size());
  return _this->audio.samples[index];
}

}}
