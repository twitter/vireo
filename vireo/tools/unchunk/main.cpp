//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <iomanip>
#include <fstream>

#include "vireo/base_cpp.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/util.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp4.h"
#include "vireo/util/util.h"
#include "vireo/version.h"

using namespace vireo;

using std::ifstream;
using std::ofstream;
using std::vector;

int main(int argc, const char* argv[]) {
  // Usage
  auto print_usage = [](const string name) {
    const int opt_len = 30;
    cout << "Usage: " << name << " [options] dts_offsets chunks original" << endl;
    cout << std::left << std::setw(opt_len) << "dts_offsets" << "dts offsets in sec for audio/video tracks in each chunk (format: a0:v0;a1:v1...) (e.g. 0.0:0.0;1.0:1.01...) (-1.0 means unavailable)" << endl;
    cout << std::left << std::setw(opt_len) << "chunks"      << "list of chunks" << endl;
    cout << std::left << std::setw(opt_len) << "original"    << "unchunked original file to be created" << endl;
    cout << endl << "Options:" << endl;
    cout << std::left << std::setw(opt_len) << "--help"       << "show usage" << endl;
  };

  struct dts_offset_in_sec {
    double audio;
    double video;
  };
  auto parse_dts_offsets = [](string dts_offsets_str) -> vector<dts_offset_in_sec> {
    vector<dts_offset_in_sec> dts_offsets;
    stringstream ss_semicolon(dts_offsets_str);
    std::string chunk_dts_offsets;
    __try {
      while(getline(ss_semicolon, chunk_dts_offsets, ';')) {
        stringstream ss_colon(chunk_dts_offsets);
        string audio;
        getline(ss_colon, audio, ':');
        string video;
        getline(ss_colon, video, ':');
        dts_offsets.push_back({ stod(audio), stod(video) });
      }
    } __catch (std::exception& e) {
      cerr << "error parsing dts offsets string \"" << dts_offsets_str << "\" (expected format: a0:v0;a1:v1...) (e.g. 0.0:0.0;1.0:1.01...) (-1.0 means unavailable)" << endl;
    }
    return dts_offsets;
  };

  const string name = common::Path::Filename(argv[0]);
  if (argc < 2) {
    print_usage(name);
    return 1;
  }

  // Parse arguments
  int last_arg = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      print_usage(name);
      return 0;
    }
  }
  uint32_t dts_offsets_arg = last_arg;
  uint32_t first_chunk_arg = dts_offsets_arg + 1;
  int32_t num_chunks = argc - first_chunk_arg - 1;
  if (num_chunks < 0) {  // one more arg for outfile
    cerr << "Need to specify dts_offsets, infiles and an outfile" << endl;
    return 1;
  }
  vector<dts_offset_in_sec> dts_offsets = parse_dts_offsets(argv[last_arg]);
  if (dts_offsets.size() == 0 || dts_offsets.size() != num_chunks) {
    cerr << "dts offsets must be defined for all chunks, # dts offsets = " << dts_offsets.size() << ", # chunks = " << num_chunks << endl;
    return 1;
  }

  __try {
    vector<common::EditBox> edit_boxes;
    auto audio_settings = settings::Audio::None;
    auto video_settings = settings::Video::None;
    bool audio_settings_initialized = false;
    bool video_settings_initialized = false;
    vector<decode::Sample> audio_samples;
    vector<decode::Sample> video_samples;
    uint64_t audio_prev_dts = 0;
    uint64_t video_prev_dts = 0;

    // Unchunk
    for (uint32_t i = 0; i < dts_offsets.size(); ++i) {
      const auto& movie = demux::Movie(string(argv[first_chunk_arg + i]));
      const auto& dts_offset = dts_offsets[i];
      if (movie.audio_track.count()) {
        if (!audio_settings_initialized) {
          audio_settings = movie.audio_track.settings();
          edit_boxes.insert(edit_boxes.begin(), movie.audio_track.edit_boxes().begin(), movie.audio_track.edit_boxes().end());
          audio_settings_initialized = true;
        }
        for (const auto& sample: movie.audio_track) {
          uint64_t dts = (dts_offset.audio < 0.0) ? sample.pts : sample.dts + (uint64_t)round(dts_offset.audio * movie.audio_track.settings().timescale);
          CHECK(audio_samples.size() == 0 || dts > audio_prev_dts);
          audio_samples.push_back(decode::Sample(sample.pts, dts, sample.keyframe, sample.type, sample.nal));
          audio_prev_dts = dts;
        }
      }
      if (movie.video_track.count()) {
        if (!video_settings_initialized) {
          video_settings = movie.video_track.settings();
          edit_boxes.insert(edit_boxes.begin(), movie.video_track.edit_boxes().begin(), movie.video_track.edit_boxes().end());
          video_settings_initialized = true;
        }
        for (const auto& sample: movie.video_track) {
          CHECK(dts_offset.video >= 0);
          uint64_t dts = sample.dts + (uint64_t)round(dts_offset.video * movie.video_track.settings().timescale);
          CHECK(video_samples.size() == 0 || dts > video_prev_dts);
          video_samples.push_back(decode::Sample(sample.pts, dts, sample.keyframe, sample.type, sample.nal));
          video_prev_dts = dts;
        }
      }
    }

    // Mux
    auto audio_track = functional::Audio<encode::Sample>(functional::Audio<decode::Sample>(audio_samples, audio_settings), encode::Sample::Convert);
    auto video_track = functional::Video<encode::Sample>(functional::Video<decode::Sample>(video_samples, video_settings), encode::Sample::Convert);
    mux::MP4 mp4_encoder(audio_track, video_track, edit_boxes);
    const string abs_dst = vireo::common::Path::MakeAbsolute(argv[first_chunk_arg + num_chunks]);
    util::save(abs_dst, mp4_encoder());
  } __catch (std::exception& e) {
    cerr << "Error unchunking: " << e.what() << endl;
    return 1;
  }

  cout << "success" << endl;
  return 0;
}
