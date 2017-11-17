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
#include <fstream>

#include "version.h"
#include "vireo/base_cpp.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp4.h"
#include "vireo/util/util.h"
#include "vireo/transform/stitch.h"
#include "vireo/version.h"

using namespace vireo;

using std::ifstream;
using std::ofstream;
using std::vector;

int main(int argc, const char* argv[]) {
  // Usage
  auto print_usage = [](const string name) {
    const int opt_len = 30;
    cout << "Usage: " << name << " [options] infiles outfile" << endl;
    cout << "\nOptions:" << endl;
    cout << std::left << std::setw(opt_len) << "--disable_audio"            << "disable audio track (default: no)" << endl;
    cout << std::left << std::setw(opt_len) << "--help"                     << "show usage" << endl;
    cout << std::left << std::setw(opt_len) << "--version"                  << "show version" << endl;
  };

  const string name = common::Path::Filename(argv[0]);
  if (argc < 2) {
    print_usage(name);
    return 1;
  }

  // Parse arguments
  bool disable_audio = false;
  int last_arg = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--disable_audio") == 0) {
      disable_audio = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--version") == 0) {
      cout << name << " version " << STITCH_VERSION << " (based on vireo " << VIREO_VERSION << ")" << endl;
      return 0;
    } else if (strcmp(argv[i], "--help") == 0) {
      print_usage(name);
      return 0;
    }
  }
  if (last_arg + 1 >= argc) {
    cerr << "Need to specify infiles and outfile" << endl;
    return 1;
  }

  __try {
    // Create decoders
    vector<string> filenames;
    for (int i = last_arg; i < argc - 1; ++i) {
      filenames.push_back(argv[i]);
    }

    vector<functional::Audio<decode::Sample>> audios;
    vector<functional::Video<decode::Sample>> videos;
    vector<vector<common::EditBox>> edit_boxes_per_track;

    // Ensure videos are compatible for stitching
    auto demuxer_0 = demux::Movie(filenames[0]);
    for (auto filename: filenames) {
      auto demuxer = demux::Movie(filename);
      if (!demuxer.video_track.count()) {
        cerr << "Could not find video track: " << filename << endl;
        return 1;
      }
      if (!disable_audio && demuxer_0.audio_track.settings().timescale != demuxer.audio_track.settings().timescale) {
        cerr << "Audio timescale does not match: " << filenames[0] << " and " << filename << endl;
        cerr << "Use --disable_audio to disable stitching audio tracks" << endl;
        return 1;
      }
      if (!disable_audio && demuxer_0.audio_track.settings().sample_rate != demuxer.audio_track.settings().sample_rate) {
        cerr << "Audio sample rate does not match: " << filenames[0] << " and " << filename << endl;
        cerr << "Use --disable_audio to disable stitching audio tracks" << endl;
        return 1;
      }
      if (demuxer_0.video_track.settings().width != demuxer.video_track.settings().width ||
          demuxer_0.video_track.settings().height != demuxer.video_track.settings().height) {
        cerr << "Dimensions do not match: " << filenames[0] << " (" << demuxer_0.video_track.settings().width << ", " << demuxer_0.video_track.settings().height <<  ") and ";
        cerr << filename <<  " (" << demuxer.video_track.settings().width << ", " << demuxer.video_track.settings().height <<  ")" << endl;
        cerr << "Transcode the video to allow stitching" << endl;
        return 1;
      }
      if (demuxer_0.video_track.settings().sps_pps.pps != demuxer.video_track.settings().sps_pps.pps ||
          demuxer_0.video_track.settings().sps_pps.sps != demuxer.video_track.settings().sps_pps.sps) {
        cerr << "Incompatible SPS or PPS" << endl;
        return 1;
      }
      videos.push_back((functional::Video<decode::Sample>)demuxer.video_track);
      vector<common::EditBox> edit_boxes = demuxer.video_track.edit_boxes();
      if (!disable_audio) {
        audios.push_back((functional::Audio<decode::Sample>)demuxer.audio_track);
        edit_boxes.insert(edit_boxes.end(), demuxer.audio_track.edit_boxes().begin(), demuxer.audio_track.edit_boxes().end());
      }
      edit_boxes_per_track.push_back(edit_boxes);
    }

    // Stitch
    auto stitched = transform::Stitch(audios, videos, edit_boxes_per_track);
    vector<common::EditBox> edit_boxes = stitched.audio_track.edit_boxes();
    edit_boxes.insert(edit_boxes.end(), stitched.video_track.edit_boxes().begin(), stitched.video_track.edit_boxes().end());
    mux::MP4 muxer(functional::Audio<encode::Sample>(stitched.audio_track, encode::Sample::Convert),
                   functional::Video<encode::Sample>(stitched.video_track, encode::Sample::Convert),
                   edit_boxes);
    util::save(vireo::common::Path::MakeAbsolute(argv[argc - 1]), muxer());
  } __catch (std::exception& e) {
#if __EXCEPTIONS
    cerr << "Error stitching movie: " << e.what() << endl;
#else
    cerr << "Error stitching movie" << endl;
#endif
    return 1;
  }

  return 0;
}
