//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <iomanip>

#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/settings/settings.h"
#include "vireo/util/util.h"

using namespace vireo;

void print_usage(const string name) {
  cout << "Usage: " << name << " ref test" << endl;
}

struct Config {
  string ref = "";
  string test = "";
};

int parse_arguments(int argc, const char* argv[], Config& config) {
  if (argc != 3) {
    cerr << "Need to specify ref and test files" << endl;
    return 1;
  }
  config.ref = argv[1];
  config.test = argv[2];
  return 0;
}

template<class T>
void print_diff(string error, string ref, string test, T ref_val, T test_val) {
  cout << error;
  cout << ": ";
  cout << ref << " = " << ref_val;
  cout << ", ";
  cout << test << " = " << test_val;
  cout << endl;
}

int main(int argc, const char* argv[]) {
  // Parse arguments
  if (argc < 3) {
    print_usage(common::Path::Filename(argv[0]));
    return 1;
  }
  Config config;
  if (parse_arguments(argc, argv, config)) {
    return 1;
  }

  __try {
    // Demux video files
    demux::Movie movie1(common::Path::MakeAbsolute(config.ref));
    demux::Movie movie2(common::Path::MakeAbsolute(config.test));

    // Compare file types
    if (movie1.file_type() != movie2.file_type()) {
      print_diff("File types do not match", config.ref, config.test,
                 kFileTypeToString[movie1.file_type()],
                 kFileTypeToString[movie2.file_type()]);
      return 1;
    }

    // Compare video settings
    const auto video_settings1 = movie1.video_track.settings();
    const auto video_settings2 = movie2.video_track.settings();
    if (video_settings1.codec != video_settings2.codec) {
      print_diff("Video codecs do not match", config.ref, config.test,
                 settings::kVideoCodecToString[video_settings1.codec],
                 settings::kVideoCodecToString[video_settings2.codec]);
      return 1;
    }
    if (video_settings1.width != video_settings2.width ||
        video_settings1.height != video_settings2.height) {
      stringstream res1; res1 << video_settings1.width << "x" << video_settings1.height;
      stringstream res2; res2 << video_settings2.width << "x" << video_settings2.height;
      print_diff("Video resolutions do not match", config.ref, config.test, res1.str(), res2.str());
      return 1;
    }
    if (video_settings1.timescale != video_settings2.timescale) {
      print_diff("Timescales of videos do not match", config.ref, config.test,
                 video_settings1.timescale,
                 video_settings2.timescale);
      return 1;
    }
    if (video_settings1.orientation != video_settings2.orientation) {
      print_diff("Orientations of videos do not match", config.ref, config.test,
                 settings::kOrientationToString[video_settings1.orientation],
                 settings::kOrientationToString[video_settings2.orientation]);
      return 1;
    }
    if (video_settings1.codec == settings::Video::Codec::H264) {
      if (video_settings1.sps_pps.nalu_length_size != video_settings2.sps_pps.nalu_length_size ||
          video_settings1.sps_pps.sps != video_settings2.sps_pps.sps ||
          video_settings1.sps_pps.pps != video_settings2.sps_pps.pps) {
        cout << "SPS and PPS videos do not match" << endl;
        return 1;
      }
    }

    // Compare audio settings
    const auto audio_settings1 = movie1.audio_track.settings();
    const auto audio_settings2 = movie2.audio_track.settings();

    if (audio_settings1.channels != audio_settings2.channels) {
      print_diff("Audio channels do not match", config.ref, config.test,
                 (uint16_t)audio_settings1.channels,
                 (uint16_t)audio_settings2.channels);
      return 1;
    }
    if (audio_settings1.timescale != audio_settings2.timescale) {
      print_diff("Audio timescales do not match", config.ref, config.test,
                 audio_settings1.timescale,
                 audio_settings2.timescale);
      return 1;
    }
    if (audio_settings1.sample_rate != audio_settings2.sample_rate) {
      print_diff("Audio frequencies do not match", config.ref, config.test,
                 audio_settings1.sample_rate,
                 audio_settings2.sample_rate);
      return 1;
    }
    if (audio_settings1.codec != audio_settings2.codec) {
      print_diff("Audio codecs do not match", config.ref, config.test,
                 settings::kAudioCodecToString[audio_settings1.codec],
                 settings::kAudioCodecToString[audio_settings2.codec]);
      return 1;
    }

    // Compare video sample count
    if (movie1.video_track.count() != movie2.video_track.count()) {
      print_diff("Number of video samples do not match", config.ref, config.test,
                 movie1.video_track.count(),
                 movie2.video_track.count());
      return 1;
    }

    // Compare audio sample count
    if (movie1.audio_track.count() != movie2.audio_track.count()) {
      print_diff("Number of audio samples do not match", config.ref, config.test,
                 movie1.audio_track.count(),
                 movie2.audio_track.count());
      return 1;
    }

    // Create a list of ordered samples
    vector<decode::Sample> samples1;
    vector<decode::Sample> samples2;
    for (const auto& sample: movie1.video_track) {
      samples1.push_back(sample);
    }
    for (const auto& sample: movie1.audio_track) {
      samples1.push_back(sample);
    }
    for (const auto& sample: movie2.video_track) {
      samples2.push_back(sample);
    }
    for (const auto& sample: movie2.audio_track) {
      samples2.push_back(sample);
    }
    auto sample_compare = [](const decode::Sample& a, const decode::Sample& b) {
      return a.byte_range.pos < b.byte_range.pos;
    };
    sort(samples1.begin(), samples1.end(), sample_compare);
    sort(samples2.begin(), samples2.end(), sample_compare);

    // Compare sample order and samples
    for (uint32_t i = 0; i < samples1.size(); ++i) {
      const auto& sample1 = samples1[i];
      const auto& sample2 = samples2[i];
      if (sample1 != sample2) {
        cout << "Sample " << i << " in the files do not match" << endl;
        return 1;
      }
    }

    // Compare sample payloads - do separately to fail fast for metadata mismatches
    for (uint32_t i = 0; i < samples1.size(); ++i) {
      const auto& nal1 = samples1[i].nal();
      const auto& nal2 = samples2[i].nal();
      if (nal1 != nal2) {
        cout << "Payloads of sample " << i << " do not match" << endl;
        return 1;
      }
    }
    cout << "Files are functionally identical" << endl;
  } __catch (std::exception& e) {
    cout << "Unsupported video files, comparing binaries" << endl;
    __try {
      auto ref = common::Data32(common::Path::MakeAbsolute(config.ref));
      auto test = common::Data32(common::Path::MakeAbsolute(config.test));
      if (ref != test) {
        cout << "Binary files differ" << endl;
        return 1;
      }
    } __catch(std::exception& e) {
      cerr << "Error comparing files: " << e.what() << endl;
      return 1;
    }
    cout << "Files are identical" << endl;
  }
  return 0;
}
