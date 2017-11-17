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
#include <set>

#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/constants.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/util.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp2ts.h"
#include "vireo/mux/mp4.h"
#include "vireo/mux/webm.h"
#include "vireo/util/util.h"
#include "vireo/tests/test_common.h"

using std::ifstream;
using std::ofstream;
using std::set;

using namespace vireo;

static const int kMaxIterations = 10000;

struct Config {
  int iterations = 1;
  int start_gop = 0;
  int num_gops = numeric_limits<int>::max();
  FileFormat file_format = FileFormat::Regular;
  bool video_only = false;
  bool audio_only = false;
  string infile = "";
  string outfile = "";
  FileType outfile_type = UnknownFileType;
} defaults;

void print_usage(const string name) {
  const int opt_len = 20;
  const int desc_len = 90;

  stringstream file_format_options; file_format_options << "file type (" << FileFormat::Regular << ": regular, " << FileFormat::DashInitializer << ": dash init, " << FileFormat::DashData << ": dash data, " << FileFormat::HeaderOnly << ": header only, " << FileFormat::SamplesOnly << ": samples only)";

  cout << "Usage: " << name << " [options] infile outfile" << endl;
  cout << "\nOptions:" << endl;
  cout << std::left << std::setw(opt_len) << "-i, -iterations:"  << std::left << std::setw(desc_len) << "iteration count (for profiling)"    << "(default: " << defaults.iterations << ")" << endl;
  cout << std::left << std::setw(opt_len) << "-s, -start_gop:"   << std::left << std::setw(desc_len) << "start GOP (when video exists)"      << "(default: " << defaults.start_gop << ")" << endl;
  cout << std::left << std::setw(opt_len) << "-n, -num_gops:"    << std::left << std::setw(desc_len) << "number of GOPs (when video exists)" << "(default: all GOPs)" << endl;
  cout << std::left << std::setw(opt_len) << "-t, -type:"        << std::left << std::setw(desc_len) << file_format_options.str()            << "(default: " << defaults.file_format << ")" << endl;
  cout << std::left << std::setw(opt_len) << "--vonly:"          << std::left << std::setw(desc_len) << "remux only video"                   << "(default: " << (defaults.video_only ? "true" : "false") << ")" << endl;
  cout << std::left << std::setw(opt_len) << "--aonly:"          << std::left << std::setw(desc_len) << "remux only audio"                   << "(default: " << (defaults.audio_only ? "true" : "false") << ")" << endl;
}

int parse_arguments(int argc, const char* argv[], Config& config) {
  int last_arg = 1;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "-iterations") == 0) {
      int arg_iterations = atoi(argv[++i]);
      if (arg_iterations < 1 || arg_iterations > kMaxIterations) {
        cerr << "iterations must be between 1 and " << kMaxIterations << endl;
        return 1;
      }
      config.iterations = (int)arg_iterations;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-start_gop") == 0) {
      int arg_start_gop = atoi(argv[++i]);
      if (arg_start_gop < 0) {
        cerr << "start gop cannot be non-negative" << endl;
        return 1;
      }
      config.start_gop = (int)arg_start_gop;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "-num_gops") == 0) {
      int arg_num_gops = atoi(argv[++i]);
      if (arg_num_gops <= 0) {
        cerr << "num gops must be positive" << endl;
        return 1;
      }
      config.num_gops = (int)arg_num_gops;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "-type") == 0) {
      FileFormat arg_file_format = (FileFormat)atoi(argv[++i]);
      if (arg_file_format < FileFormat::Regular || arg_file_format > FileFormat::SamplesOnly) {
        stringstream choices; choices << FileFormat::Regular << ": regular, " << FileFormat::DashInitializer << ": dash init, " << FileFormat::DashData << ": dash data, " << FileFormat::HeaderOnly << ": header only, " << FileFormat::SamplesOnly << ": samples only";
        cerr << "file type has to be one of the following choices => " << choices.str() << endl;
        return 1;
      }
      config.file_format = arg_file_format;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--vonly") == 0) {
      config.video_only = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--aonly") == 0) {
      config.audio_only = true;
      last_arg = i + 1;
    }
  }
  if (last_arg + 1 >= argc) {
    cerr << "Need to specify infile and outfile" << endl;
    return 1;
  }
  config.infile = common::Path::MakeAbsolute(argv[last_arg]);
  config.outfile = common::Path::MakeAbsolute(argv[last_arg + 1]);

  struct ExtFileTypePair {
    string ext;
    FileType outfile_type;
  };
  const vector<ExtFileTypePair> ext_file_types = { {".mp4", MP4}, {".m4a", MP4}, {".m4v", MP4}, {".mov", MP4}, {".ts", MP2TS}, {".webm", WebM} };

  // Get output file format
  for (auto ext_file_type: ext_file_types) {
    auto ext = ext_file_type.ext;
    auto out_file_type = ext_file_type.outfile_type;
    if (config.outfile.find(ext) != string::npos) {
        config.outfile_type = out_file_type;
        break;
    }
  }
  if (config.outfile_type == UnknownFileType) {
    cerr << "Output content type is unknown" << endl;
    return 1;
  }

  return 0;
}

static inline auto no_data_sample_convert(decode::Sample sample) -> encode::Sample {
  const uint32_t size = sample.byte_range.available ? sample.byte_range.size : sample.nal().count();
  return encode::Sample(sample.pts, sample.dts, sample.keyframe, sample.type, common::Data32(size));
};

template <int Type>
functional::Media<functional::Function<encode::Sample, uint32_t>, encode::Sample, uint32_t, Type> remux(const functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type> track, const Config config, const int64_t start_dts, const int64_t end_dts, const bool print_info) {
  if (print_info) {
    cout << ((Type == SampleType::Video) ? "video" : "audio") << " samples with dts from " << start_dts << " to " << end_dts << endl;
  }
  if (config.file_format == FileFormat::HeaderOnly) {
    return functional::Media<functional::Function<encode::Sample, uint32_t>, encode::Sample, uint32_t, Type>(track, no_data_sample_convert).filter([start_dts, end_dts](const encode::Sample& sample) { return (sample.dts >= start_dts && sample.dts < end_dts); });
  } else {
    return functional::Media<functional::Function<encode::Sample, uint32_t>, encode::Sample, uint32_t, Type>(track, encode::Sample::Convert).filter([start_dts, end_dts](const encode::Sample& sample) { return (sample.dts >= start_dts && sample.dts < end_dts); });
  }
};

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

    // Read input file
    demux::Movie movie(config.infile);

    // Check input / output file format combinations to do remux
    auto infile_type = movie.file_type();
    if (infile_type != config.outfile_type && (infile_type == WebM || config.outfile_type == WebM)) {
      cerr << "Cannot remux from " << kFileTypeToString[infile_type] << " to " << kFileTypeToString[config.outfile_type] << endl;
      return 1;
    }

    // Process arguments
    if (config.video_only && config.audio_only) {
      cerr << "Cannot use video and audio only flags at the same time" << endl;
    } else if (config.video_only && movie.video_track.count() == 0) {
      cerr << "File does not contain a valid video track" << endl;
      return 1;
    } else if (config.audio_only && movie.audio_track.count() == 0) {
      cerr << "File does not contain a valid audio track" << endl;
      return 1;
    } else if (movie.video_track.count() == 0 && movie.audio_track.count() == 0) {
      cerr << "File does not contain any audio / video tracks" << endl;
      return 1;
    }
    bool remux_audio = (config.video_only || (movie.audio_track.count() == 0)) ? false : true;
    bool remux_video = (config.audio_only || (movie.video_track.count() == 0)) ? false : true;

    // Process GOP boundaries / arguments
    struct dts_pair {
      int64_t video;
      int64_t audio;
    };
    vector<dts_pair> dts_at_gop_boundaries;
    if (movie.video_track.count()) {
      for (const auto& sample: movie.video_track) {
        if (sample.keyframe) {
          int64_t video_dts = sample.dts;
          int64_t audio_dts = remux_audio ? common::round_divide((uint64_t)video_dts, (uint64_t)movie.audio_track.settings().timescale, (uint64_t)movie.video_track.settings().timescale) : 0;
          dts_at_gop_boundaries.push_back({ video_dts, audio_dts });
        }
      }
    } else {
      dts_at_gop_boundaries.push_back({ 0, movie.audio_track(0).dts });
    }
    int total_gops = (int)dts_at_gop_boundaries.size();
    dts_at_gop_boundaries.push_back({ numeric_limits<int64_t>::max(), numeric_limits<int64_t>::max() });

    if (config.start_gop >= total_gops) {
      cerr << "start gop has to be between 0 and " << total_gops << endl;
      return 1;
    }
    config.num_gops = min(config.num_gops, (int)total_gops - config.start_gop);

    // Main
    uint32_t i = 0;
    cout << Profile::Function("Remuxing", [&]{
      // Get start / end dts for video track
      auto start_dts_pair = dts_at_gop_boundaries[config.start_gop];
      auto end_dts_pair = dts_at_gop_boundaries[config.start_gop + config.num_gops];

      // Get output video track
      auto output_video_track = functional::Video<encode::Sample>();
      if (remux_video) {
        output_video_track = remux<SampleType::Video>(movie.video_track, config, start_dts_pair.video, end_dts_pair.video, i == 0);
      }

      // Get output audio track
      auto output_audio_track = functional::Audio<encode::Sample>();
      if (remux_audio) {
        output_audio_track = remux<SampleType::Audio>(movie.audio_track, config, start_dts_pair.audio, end_dts_pair.audio, i == 0);
      }

      // Get output caption track
      auto output_caption_track = functional::Caption<encode::Sample>();
      if (remux_video) {
        output_caption_track = remux<SampleType::Caption>(movie.caption_track, config, start_dts_pair.video, end_dts_pair.video, i == 0);
      }

      // Add edit boxes only if remuxing input file without any modifications
      vector<common::EditBox> edit_boxes;
      if (config.file_format == FileFormat::Regular && config.num_gops == total_gops) {
        if (remux_audio) {
          edit_boxes = movie.audio_track.edit_boxes();
        }
        if (remux_video) {
          edit_boxes.insert(edit_boxes.begin(), movie.video_track.edit_boxes().begin(), movie.video_track.edit_boxes().end());
        }
      }

      // Create muxer
      functional::Function<common::Data32> muxer;
      if (config.outfile_type == MP4) {
        muxer = mux::MP4(output_audio_track, output_video_track, output_caption_track, edit_boxes, config.file_format);
      } else if (config.outfile_type == MP2TS) {
        muxer = mux::MP2TS(output_audio_track, output_video_track, output_caption_track);
      } else {
        muxer = mux::WebM(output_audio_track, output_video_track);
      }

      // Save the output file once
      if (i == 0) {
        util::save(common::Path::MakeAbsolute(config.outfile), muxer());
      } else {
        muxer();
      }
      ++i;
    }, config.iterations) << endl;
  } __catch (std::exception& e) {
    cerr << "Error remuxing movie: " << e.what() << endl;
    return 1;
  }
  return 0;
}
