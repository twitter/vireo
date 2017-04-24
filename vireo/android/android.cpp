//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/android/android.h"
#include "vireo/android/util.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/h264_bytestream.h"
#include "vireo/settings/settings.h"

using namespace vireo;

bool CanTrim(int in_fd) {
  __try {
    internal::demux::MP4 mp4_decoder(in_fd);
    if (mp4_decoder.video_track.count() == 0 && mp4_decoder.audio_track.count() == 0) {
      return false;
    }
  } __catch (std::exception& e) {
    return false;
  }
  return true;
}

int Trim(int in_fd, int out_fd, long start_ms, long duration_ms) {
  __try {
    internal::demux::MP4 mp4_decoder(in_fd);
    if (!mp4_decoder.video_track.count() && !mp4_decoder.audio_track.count()) {
      return ERR_NO_AUDIO_OR_VIDEO;
    }
    mux::MP4 mp4_encoder(android::trim(mp4_decoder, (uint64_t)start_ms, (uint64_t)duration_ms, false));
    out_fd << mp4_encoder();
  } __catch (std::exception& e) {
    return ERR_EXCEPTION;
  }
  return 0;
}

int GetFrameInterval(int in_fd, int* interval) {
  __try {
    internal::demux::MP4 mp4_decoder(in_fd);
    if (!mp4_decoder.video_track.count()) {
      return ERR_NO_AUDIO_OR_VIDEO;
    }
    auto frame_intervals = android::frame_intervals(mp4_decoder);
    if(frame_intervals.size() != 1) {
      return ERR_ASSERTION_FAIL;
    }
    interval[0] = frame_intervals[0].start_index;
    interval[1] = frame_intervals[0].num_frames;
  } __catch (std::exception& e) {
    return ERR_EXCEPTION;
  }
  return 0;
}

int Stitch(int* in_fds, int count, int out_fd) {
  __try {
    if (!in_fds || !out_fd || count <= 0) {
      return 4;
    }
    for (int i = 0; i < count; ++i) {
      if (!in_fds[i]) {
        return 5;
      }
    }
    internal::demux::MP4 mp4_decoder_0(in_fds[0]);
    if (!mp4_decoder_0.video_track.count() && !mp4_decoder_0.audio_track.count()) {
      return ERR_NO_AUDIO_OR_VIDEO;
    }
    vector<function<common::Data32(void)>> movie_data;
    for (int i = 0; i < count; ++i) {
      internal::demux::MP4 mp4_decoder(in_fds[i]);
      if (!mp4_decoder.video_track.count() && !mp4_decoder.audio_track.count()) {
        return ERR_NO_AUDIO_OR_VIDEO;
      }
      if (mp4_decoder_0.audio_track.settings().sample_rate != mp4_decoder.audio_track.settings().sample_rate
          || mp4_decoder_0.audio_track.settings().timescale != mp4_decoder.audio_track.settings().timescale) {
        return ERR_SETTING_MISMATCH;
      }
      if (mp4_decoder_0.video_track.settings().width != mp4_decoder.video_track.settings().width
          || mp4_decoder_0.video_track.settings().height != mp4_decoder.video_track.settings().height
          || mp4_decoder_0.video_track.settings().timescale != mp4_decoder.video_track.settings().timescale
          || mp4_decoder_0.video_track.settings().sps_pps.pps != mp4_decoder.video_track.settings().sps_pps.pps
          || mp4_decoder_0.video_track.settings().sps_pps.sps != mp4_decoder.video_track.settings().sps_pps.sps) {
        return ERR_SETTING_MISMATCH;
      }
      movie_data.push_back([in_fds, i]() {
        return common::Data32(in_fds[i], nullptr);
      });
    }

    mux::MP4 mp4_encoder(android::stitch(movie_data));

    out_fd << mp4_encoder();
  } __catch (std::exception& e) {
    return ERR_EXCEPTION;
  }
  return 0;
}

int Mux(int in_mp4_fd, int in_h264_bytestream_fd, int out_fd, int fps_factor, int width, int height) {
  __try {
    internal::demux::MP4 mp4_decoder(in_mp4_fd);
    if (!mp4_decoder.video_track.count() && !mp4_decoder.audio_track.count()) {
      return ERR_NO_AUDIO_OR_VIDEO;
    }
    internal::decode::H264_BYTESTREAM h264_bytestream_decoder(common::Data32(in_h264_bytestream_fd, nullptr));
    mux::MP4 mp4_encoder(android::mux(mp4_decoder, h264_bytestream_decoder, fps_factor, width, height));

    out_fd << mp4_encoder();
  } __catch (std::exception& e) {
    return ERR_EXCEPTION;
  }
  return 0;
}

#ifdef ANDROID
// We don't link against libstdc++ on Android because it's too big, and most of the stuff we use is purely in the headers.
// As a horrible workaround, provide definitions for these missing symbols...
#include <stdlib.h>

void std::__throw_length_error(char const*) { CHECK(0); }
void std::__throw_bad_function_call() { CHECK(0); }
void std::__throw_bad_alloc() { CHECK(0); }
void std::__throw_system_error(int) { CHECK(0); }
std::ios_base::Init::Init() {}
std::ios_base::Init::~Init() {}
#endif
