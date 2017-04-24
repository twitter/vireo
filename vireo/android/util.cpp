//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/android/util.h"
#include "vireo/base_cpp.h"
#include "vireo/common/editbox.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/error/error.h"
#include "vireo/encode/util.h"
#include "vireo/functional/media.hpp"
#include "vireo/transform/stitch.h"
#include "vireo/transform/trim.h"

namespace vireo {
namespace android {

auto frame_intervals(internal::demux::MP4& mp4_decoder) -> const vector<FrameInterval> {
  if (mp4_decoder.video_track.count() == 0) {
    return { { 0, 0 } };
  } else if (mp4_decoder.video_track.edit_boxes().empty()) {
    return { { 0, mp4_decoder.video_track.count() } };
  } else {
    vector<FrameInterval> intervals;
    for (auto edit_box: mp4_decoder.video_track.edit_boxes()) {
      if (edit_box.start_pts != EMPTY_EDIT_BOX) {
        uint64_t start_pts = edit_box.start_pts;
        uint64_t end_pts = start_pts + edit_box.duration_pts;
        int64_t start_index = -1;
        int64_t end_index = -1;
        uint32_t index = 0;
        for (auto sample: mp4_decoder.video_track) {
          if (start_index < 0 && sample.pts >= start_pts) {
            start_index = index;
          }
          if (sample.pts < end_pts) {
            end_index = index;
          }
          ++index;
        }
        CHECK(start_index >= 0);
        CHECK(end_index >= start_index);
        FrameInterval frame_interval;
        frame_interval.start_index = (uint32_t)start_index;
        frame_interval.num_frames = (uint32_t)end_index - frame_interval.start_index + 1;
        intervals.push_back(frame_interval);
      }
    }
    return intervals;
  }
}

static inline auto raw_sample_convert(internal::decode::RawSample sample, uint64_t pts, uint64_t dts) -> encode::Sample {
  return encode::Sample(pts, dts, sample.keyframe, SampleType::Video, sample.nal);
};

auto mux(internal::demux::MP4& mp4_decoder, internal::decode::H264_BYTESTREAM& h264_bytestream_decoder, int fps_factor, int width, int height) -> mux::MP4 {
  THROW_IF(h264_bytestream_decoder.count() == 0, Invalid);
  THROW_IF(fps_factor <= 0, InvalidArguments);
  THROW_IF(!((width > 0 && height > 0) || (width == 0 && height == 0)), InvalidArguments);

  vector<encode::Sample> audio_samples;
  vector<encode::Sample> video_samples;
  vector<common::EditBox> edit_boxes;

  // Get an ordered list of trimmed video pts (we assume H264 AnnexB has frames with ordered pts)
  vector<uint64_t> valid_pts;
  for (auto sample: mp4_decoder.video_track) {
    int64_t new_pts = common::EditBox::RealPts(mp4_decoder.video_track.edit_boxes(), sample.pts);
    if (new_pts != -1) {
      valid_pts.push_back(new_pts);
    }
  }
  sort(valid_pts.begin(), valid_pts.end());
  const uint32_t num_frames = common::ceil_divide((uint32_t)valid_pts.size(), (uint32_t)1, (uint32_t)fps_factor);
  THROW_IF(num_frames != h264_bytestream_decoder.count(), Invalid);

  const int64_t video_first_pts = valid_pts[0];
  const int64_t audio_pts_offset = video_first_pts * mp4_decoder.audio_track.settings().timescale / mp4_decoder.video_track.settings().timescale;

  uint32_t video_sample_index = 0;
  uint32_t audio_sample_index = 0;
  int64_t audio_first_dts = -1;
  for (auto sample_func: h264_bytestream_decoder) {
    auto v_sample = raw_sample_convert(sample_func(), valid_pts[video_sample_index] - video_first_pts, valid_pts[video_sample_index] - video_first_pts);
    const float v_dts = (float)v_sample.dts / (mp4_decoder.video_track.settings().timescale);

    while (audio_sample_index < mp4_decoder.audio_track.count()) {
      auto a_sample = mp4_decoder.audio_track(audio_sample_index);
      const float a_dts = (float)a_sample.dts / (mp4_decoder.audio_track.settings().timescale);

      if (a_dts < v_dts) {
        // Since we remove edit boxes from video, we will adjust audio edit boxes accordingly
        // Thus we should just keep the audio samples within new edit box bounds
        if (a_sample.dts >= audio_pts_offset) {
          if (audio_first_dts == -1) {
            audio_first_dts = a_sample.dts;
          }
          audio_samples.push_back(encode::Sample(mp4_decoder.audio_track(audio_sample_index).shift(-audio_first_dts)));
        }
        ++audio_sample_index;
      } else {
        break;
      }
    }
    video_samples.push_back(v_sample);
    video_sample_index += fps_factor;
  }
  // Since we remove edit boxes from video, we need to align audio edit boxes accordingly
  // Also if we removed any audio samples in the muxing process, we should reflect that here
  bool first_edit_box = true;
  for (auto edit_box: mp4_decoder.audio_track.edit_boxes()) {
    if (first_edit_box) {
      const int64_t start_pts = edit_box.start_pts + audio_pts_offset - audio_first_dts;
      const uint64_t duration_pts = edit_box.duration_pts - audio_pts_offset;
      edit_boxes.push_back(common::EditBox(start_pts, duration_pts, edit_box.rate, edit_box.type));
    } else {
      const int64_t start_pts = edit_box.start_pts - audio_first_dts;
      const uint64_t duration_pts = edit_box.duration_pts;
      edit_boxes.push_back(common::EditBox(start_pts, duration_pts, edit_box.rate, edit_box.type));
    }
  }

  auto output_video_settings = settings::Video { settings::Video::Codec::H264,
                                                 width ? (uint16_t)width : mp4_decoder.video_track.settings().width,
                                                 height ? (uint16_t)height : mp4_decoder.video_track.settings().height,
                                                 mp4_decoder.video_track.settings().timescale,
                                                 mp4_decoder.video_track.settings().orientation,
                                                 h264_bytestream_decoder.sps_pps() };
  return mux::MP4(functional::Audio<encode::Sample>(audio_samples, mp4_decoder.audio_track.settings()),
                  functional::Video<encode::Sample>(video_samples, output_video_settings), edit_boxes);
}

auto stitch(vector<function<common::Data32(void)>> movies, bool disable_audio) -> mux::MP4 {
  THROW_IF(movies.size() == 0, InvalidArguments);

  // Collect tracks from all movies
  vector<functional::Audio<decode::Sample>> audios;
  vector<functional::Video<decode::Sample>> videos;
  vector<vector<common::EditBox>> edit_boxes_per_track;
  for (auto movie_data: movies) {
    auto mp4_decoder = internal::demux::MP4(move(movie_data()));
    audios.push_back((functional::Audio<decode::Sample>)mp4_decoder.audio_track);
    videos.push_back((functional::Video<decode::Sample>)mp4_decoder.video_track);
    vector<common::EditBox> edit_boxes = mp4_decoder.audio_track.edit_boxes();
    edit_boxes.insert(edit_boxes.end(), mp4_decoder.video_track.edit_boxes().begin(), mp4_decoder.video_track.edit_boxes().end());
    edit_boxes_per_track.push_back(edit_boxes);
  }

  // Stitch
  auto stitched = transform::Stitch(audios, videos, edit_boxes_per_track);
  vector<common::EditBox> edit_boxes = stitched.audio_track.edit_boxes();
  edit_boxes.insert(edit_boxes.end(), stitched.video_track.edit_boxes().begin(), stitched.video_track.edit_boxes().end());
  return mux::MP4(functional::Audio<encode::Sample>(stitched.audio_track, encode::Sample::Convert),
                  functional::Video<encode::Sample>(stitched.video_track, encode::Sample::Convert),
                  edit_boxes);
}

auto trim(internal::demux::MP4& mp4_decoder, uint64_t start_ms, uint64_t duration_ms, bool respect_input_edit_boxes) -> mux::MP4 {
  THROW_IF(duration_ms == 0, InvalidArguments);
  THROW_IF(mp4_decoder.video_track.count() == 0 && mp4_decoder.audio_track.count() == 0, Invalid);

  // Trim video track
  auto trimmed_video = transform::Trim<SampleType::Video>(mp4_decoder.video_track, respect_input_edit_boxes ? mp4_decoder.video_track.edit_boxes() : vector<common::EditBox>(), start_ms, duration_ms);
  auto trimmed_audio = transform::Trim<SampleType::Audio>(mp4_decoder.audio_track, respect_input_edit_boxes ? mp4_decoder.audio_track.edit_boxes() : vector<common::EditBox>(), start_ms, duration_ms);

  // Convert samples
  auto video_track = functional::Video<encode::Sample>(trimmed_video.track, encode::Sample::Convert);
  auto audio_track = functional::Audio<encode::Sample>(trimmed_audio.track, encode::Sample::Convert);

  // Collect output edit boxes
  vector<common::EditBox> edit_boxes;
  edit_boxes.insert(edit_boxes.end(), trimmed_video.track.edit_boxes().begin(), trimmed_video.track.edit_boxes().end());
  edit_boxes.insert(edit_boxes.end(), trimmed_audio.track.edit_boxes().begin(), trimmed_audio.track.edit_boxes().end());

  // Send samples to mp4 encoder
  mux::MP4 mp4_encoder = mux::MP4(audio_track, video_track, edit_boxes);
  return mp4_encoder;
}

}}
