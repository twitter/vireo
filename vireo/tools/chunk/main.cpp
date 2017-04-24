//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <iomanip>
#include <fstream>
#include <future>
#include <set>
#include <vector>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/util.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp4.h"
#include "vireo/util/util.h"
#include "vireo/tests/test_common.h"

using std::ifstream;
using std::ofstream;
using std::set;
using std::vector;

using namespace vireo;

int main(int argc, const char* argv[]) {
  if (argc < 3) {
    const string name = common::Path::Filename(argv[0]);
    cout << "Usage: " << name << " [options] input.mp4 output_dir" << endl;
    cout << "\nOptions:" << endl;
    cout << "-i, -iterations:\titeration count (for profiling, default: 1)" << endl;
    return 1;
  }

  int iterations = 1;
  int last_arg = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-iterations") == 0) {
      int arg_iterations = atoi(argv[++i]);
      if (arg_iterations <= 0 || arg_iterations > 10000) {
        cerr << "Invalid iteration count" << endl;
        return 1;
      }
      iterations = (uint16_t)arg_iterations;
      last_arg = i + 1;
    }
  }

  stringstream s_src;
  s_src << common::Path::MakeAbsolute(argv[last_arg]);

  stringstream s_dst;
  s_dst << common::Path::MakeAbsolute(argv[last_arg + 1]);
  if (!common::Path::Exists(s_dst.str())) {
    if (common::Path::CreateFolder(s_dst.str())) {
      cout << "Error creating output folder: " << s_dst.str() << endl;
      return 1;
    }
  }

  __try {
    cout << "Writing chunks to " << s_dst.str() << "/..." << endl;
    cout << Profile::Function("Chunking", [&]{
      demux::Movie movie(s_src.str());
      if (movie.video_track.count()) {  // Video
        // setup mp4 encoder
        unique_ptr<mux::MP4> mp4_encoder;
        auto write_chunk = [&](uint32_t index, uint64_t start_pts, uint64_t end_pts){
          cout << index << ".mp4 (start time: " << (float)start_pts / movie.video_track.settings().timescale << "s, ";
          cout << "duration: " << (float)(end_pts - start_pts) / movie.video_track.settings().timescale << "s)" << endl;
          std::stringstream s_dst_chunk; s_dst_chunk << s_dst.str() << "/" << index << ".mp4";
          const string abs_dst_chunk = vireo::common::Path::MakeAbsolute(s_dst_chunk.str());
          util::save(abs_dst_chunk, (*mp4_encoder)());
        };

        // get
        vector<uint32_t> gop_boundaries;
        for (uint32_t i = 0; i < movie.video_track.count(); ++i) {
          if (movie.video_track(i).keyframe) {
            gop_boundaries.push_back(i);
          }
        }
        gop_boundaries.push_back(movie.video_track.count());

        uint32_t chunk_index = 0;
        for (auto it = gop_boundaries.begin(); it != gop_boundaries.end() - 1; ++it ) {
          auto next = it + 1;
          uint32_t start_idx = *it;
          uint32_t end_idx = (*next) - 1;
          uint64_t start_pts = movie.video_track(start_idx).pts;
          uint64_t end_pts = movie.video_track(end_idx).pts;
          auto video_track = movie.video_track.filter_index([start_idx, end_idx](uint32_t index) { return (index >= start_idx && index <= end_idx); });
          mp4_encoder.reset(new mux::MP4(functional::Video<encode::Sample>(video_track, encode::Sample::Convert)));
          write_chunk(chunk_index, start_pts, end_pts);
          ++chunk_index;
        }
      }
      if (movie.audio_track.count()) {  // Audio
        mux::MP4 mp4_encoder(functional::Audio<encode::Sample>(movie.audio_track, encode::Sample::Convert), functional::Video<encode::Sample>());
        cout << "audio.m4a (duration: " << (float)movie.audio_track.duration() / movie.audio_track.settings().timescale << "s)" << endl;
        std::stringstream s_dst_chunk; s_dst_chunk << s_dst.str() << "/audio.m4a";
        const string abs_dst_chunk = vireo::common::Path::MakeAbsolute(s_dst_chunk.str());
        util::save(abs_dst_chunk, mp4_encoder());
      }
    }, iterations) << endl;
    cout << "SUCCESS!" << endl;
  } __catch (std::exception& e) {
    cerr << "Error reading movie " << argv[last_arg] << ": " << e.what() << endl;
    return 1;
  }
  return 0;
}
