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

#include <iomanip>

#include "vireo/base_cpp.h"
#include "vireo/common/editbox.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/decode/video.h"
#include "vireo/demux/movie.h"
#include "vireo/util/util.h"

using namespace vireo;

void print_usage(const string name) {
  cout << "Usage: " << name << " [options] ref test" << endl;
  cout << endl << "Options:" << endl;
  cout << std::left << std::setw(20) << "--verbose:" << std::left << std::setw(30) << "enable verbose output" << "(default: false)" << endl;
}

struct Config {
  string ref = "";
  string test = "";
  bool verbose = false;
};

int parse_arguments(int argc, const char* argv[], Config& config) {
  int last_arg = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--verbose") == 0) {
      config.verbose = true;
      last_arg = i + 1;
    }
  }
  if (last_arg + 1 >= argc) {
    cerr << "Need to specify ref and test files" << endl;
    return 1;
  }
  config.ref = common::Path::MakeAbsolute(argv[last_arg]);
  config.test = common::Path::MakeAbsolute(argv[last_arg + 1]);
  return 0;
}

string psnr_string(double psnr) {
  stringstream ss;
  ss << "PSNR = " << std::fixed << std::showpoint << std::setw(5) << std::setprecision(3) << psnr << " dB";
  return ss.str();
}

double calculate_psnr(double mse) {
  return 10 * log10(255.0 * 255.0 / mse);
}

string calculate_and_get_psnr_string(double mse) {
  if (mse) {
    double psnr = calculate_psnr(mse);
    return psnr_string(psnr);
  } else {
    return "Exact match!";
  }
}

int main(int argc, const char* argv[]) {
  __try {
    // Parse arguments
    if (argc < 3) {
      print_usage(common::Path::Filename(argv[0]));
      return 1;
    }
    Config config;
    if (parse_arguments(argc, argv, config)) {
      return 1;
    }

    // Read input files
    demux::Movie movie1(config.ref);
    demux::Movie movie2(config.test);

    const auto width1 = movie1.video_track.settings().width;
    const auto height1 = movie1.video_track.settings().height;
    const auto width2 = movie2.video_track.settings().width;
    const auto height2 = movie2.video_track.settings().height;
    const bool stretch = (width1 != width2) || (height1 != height2);

    // Decode frames and compare
    decode::Video decoder1(movie1.video_track);
    decode::Video decoder2(movie2.video_track);

    // Filter visible frames
    auto samples1 = movie1.video_track.filter([&edit_boxes = movie1.video_track.edit_boxes()](decode::Sample& sample){ return common::EditBox::Plays(edit_boxes, sample.pts); });
    auto samples2 = movie2.video_track.filter([&edit_boxes = movie2.video_track.edit_boxes()](decode::Sample& sample){ return common::EditBox::Plays(edit_boxes, sample.pts); });
    auto frames1 = decoder1.filter([&edit_boxes = movie1.video_track.edit_boxes()](frame::Frame& frm){ return common::EditBox::Plays(edit_boxes, frm.pts); });
    auto frames2 = decoder2.filter([&edit_boxes = movie2.video_track.edit_boxes()](frame::Frame& frm){ return common::EditBox::Plays(edit_boxes, frm.pts); });

    // Create a mapping between frames1 to frames2 according to PTS
    unordered_map<int, uint32_t> time_to_index2;
    vector<pair<uint32_t, uint32_t>> index1_to_index2;
    const int scale = 100;  // compare time up to two decimal points
    for (uint32_t index2 = 0; index2 < frames2.count(); ++index2) {
      const auto pts = common::EditBox::RealPts(movie2.video_track.edit_boxes(), frames2(index2).pts);
      const int time = (int)((float)pts * scale / frames2.settings().timescale);
      THROW_IF(time_to_index2.find(time) != time_to_index2.end(), Invalid);
      time_to_index2[time] = index2;
    }
    for (uint32_t index1 = 0; index1 < frames1.count(); ++index1) {
      const auto pts = common::EditBox::RealPts(movie1.video_track.edit_boxes(), frames1(index1).pts);
      const int time = (int)((float)pts * scale / frames1.settings().timescale);
      auto index2_search = time_to_index2.find(time);
      if (index2_search != time_to_index2.end()) {
        index1_to_index2.push_back(make_pair(index1, index2_search->second));
      }
    }

    // Check if there are frames that match between videos
    if (index1_to_index2.size() == 0) {
      cerr << "Videos do not contain any matching frames! Cannot compute PSNR" << endl;
      return 1;
    }
    cout << "Calculating PSNR over " << index1_to_index2.size() << " frames" << endl;
    if (index1_to_index2.size() < frames1.count()) {
      cout << "Warning: " << (frames1.count() - index1_to_index2.size()) << " frames of reference video not used" << endl;
    }
    if (index1_to_index2.size() < frames2.count()) {
      cout << "Warning: " << (frames2.count() - index1_to_index2.size()) << " frames of test video not used" << endl;
    }

    // Calculate PSNR
    double total_sse = 0.0;
    double total_num_pixels = 0.0;
    for (uint32_t i = 0; i < index1_to_index2.size(); ++i) {
      const uint32_t index1 = index1_to_index2[i].first;
      const uint32_t index2 = index1_to_index2[i].second;

      uint64_t sse_per_frame = 0;
      uint64_t num_pixels_per_frame = 0;
      if (samples1(index1).nal() != samples2(index2).nal()) {
        const auto& ref_frame = frames1(index1).yuv();
        const auto& test_frame = stretch ? frames2(index2).yuv().stretch(width1, width2, height1, height2) : frames2(index2).yuv();

        for (auto p: enumeration::Enum<frame::PlaneIndex>(frame::Y, frame::V)) {
          const auto& ref_plane = ref_frame.plane(p);
          const auto& test_plane = test_frame.plane(p);

          const auto& ref_bytes = ref_plane.bytes();
          for (uint32_t j = ref_bytes.a(); j < ref_bytes.b(); ++j) {
            uint32_t r = j % ref_plane.row();
            uint32_t h = j / ref_plane.row();
            if (r < ref_plane.width()) {
              auto diff = ((int16_t)ref_bytes(j) - (int16_t)test_plane(h)(r));
              sse_per_frame += diff * diff;
            }
          }
          num_pixels_per_frame += (ref_plane.width() * ref_plane.height());
        }
        THROW_IF(num_pixels_per_frame == 0, Invalid);
      }
      if (config.verbose) {
        cout << "FRAME " << std::setw(5) << i << " : ";
        double mse_per_frame = sse_per_frame ? (double)sse_per_frame / num_pixels_per_frame : 0;
        cout << calculate_and_get_psnr_string(mse_per_frame) << endl;
      } else {
        cout << "PROCESSING " << std::setw(5) << i << " / " << index1_to_index2.size();
        cout.flush();
        cout << "\r";
      }

      total_sse += sse_per_frame;
      total_num_pixels += num_pixels_per_frame;
    }

    if (config.verbose) {
      cout << "------------------------------" << endl;
      cout << "AVERAGE";
      cout << "    ";
      cout << " : ";
    }
    double mse = total_sse ? total_sse / total_num_pixels : 0;
    cout << calculate_and_get_psnr_string(mse);
    if (!config.verbose) {
      for (auto i = 0; i < 40; ++i) {
        cout << " ";  // clear end of line
      }
    }
    cout << endl;
  } __catch (std::exception& e) {
    cerr << "Error calculating PSNR: " << e.what() << endl;
    return 1;
  }
  return 0;
}
