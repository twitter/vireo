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
#include "vireo/common/editbox.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/constants.h"
#include "vireo/decode/audio.h"
#include "vireo/decode/video.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/aac.h"
#include "vireo/encode/h264.h"
#include "vireo/encode/util.h"
#include "vireo/encode/vorbis.h"
#include "vireo/encode/vp8.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp2ts.h"
#include "vireo/mux/mp4.h"
#include "vireo/mux/webm.h"
#include "vireo/util/util.h"
#include "vireo/tests/test_common.h"
#include "vireo/transform/trim.h"

using std::ifstream;
using std::ofstream;
using std::set;

using namespace vireo;

static const int kMaxIterations = 10000;
static const float kH264DefaultCRF = 28.0;
static const int kH264DefaultOptimization = 3;
static const int kVP8DefaultQuantizer = 25;
static const int kVP8DefaultOptimization = 0;
static const int kDefaultAudioBitrateInKb = 48;
static const int kMaxThreads = 64;

void print_usage(const string name) {
  const int opt_len = 20;
  const int desc_len = 90;

  stringstream h264_optimization_info; h264_optimization_info << encode::kH264MinOptimization << " fastest, " << encode::kH264MaxOptimization << " slowest (H.264)";
  stringstream vp8_optimization_info; vp8_optimization_info << encode::kVP8MinOptimization << " fastest, " << encode::kVP8MaxOptimization << " slowest (VP8)";
  stringstream optimization_info; optimization_info << "video encoding optimization (" << h264_optimization_info.str() << " / " << vp8_optimization_info.str() << ")";
  stringstream optimization_defaults; optimization_defaults << "(default: " << kH264DefaultOptimization << " (H.264) / " << kVP8DefaultOptimization << " (VP8))";
  stringstream crf_info; crf_info << std::fixed << std::setprecision(1) << "H.264 constant rate factor (" << encode::kH264MinCRF << " to " << encode::kH264MaxCRF << ", " << encode::kH264MinCRF << " best)";
  stringstream crf_defaults; crf_defaults << std::fixed << std::setprecision(1) << "(default: " << kH264DefaultCRF << ")";
  stringstream quantizer_info; quantizer_info << "VP8 quantizer (" << encode::kVP8MinQuantizer << " to " << encode::kVP8MaxQuantizer << ", " << encode::kVP8MinQuantizer << " best)";
  stringstream quantizer_defaults; quantizer_defaults << "(default: " << kVP8DefaultQuantizer << ")";
  stringstream audio_bitrate_defaults; audio_bitrate_defaults << "(default: " << kDefaultAudioBitrateInKb << " Kbps)";

  cout << "Usage: " << name << " [options] infile outfile" << endl;
  cout << "\nOptions:" << endl;
  cout << std::left << std::setw(opt_len) << "-i, -iterations:"   << std::left << std::setw(desc_len) << "iteration count of transcoding (for profiling)" << "(default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "-s, -start:"        << std::left << std::setw(desc_len) << "start time in milliseconds" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-d, -duration:"     << std::left << std::setw(desc_len) << "duration in milliseconds" << "(default: video track duration)" << endl;
  cout << std::left << std::setw(opt_len) << "-h, -height:"       << std::left << std::setw(desc_len) << "output height" << "(default: infile height)" << endl;
  cout << std::left << std::setw(opt_len) << "--square:"          << std::left << std::setw(desc_len) << "crop to 1:1 aspect ratio" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "-o, -optimization:" << std::left << std::setw(desc_len) << optimization_info.str() << optimization_defaults.str() << endl;
  cout << std::left << std::setw(opt_len) << "-r, -crf:"          << std::left << std::setw(desc_len) << crf_info.str() << crf_defaults.str() << endl;
  cout << std::left << std::setw(opt_len) << "-rc_method:"        << std::left << std::setw(desc_len) << "video rate control method: " << "(CRF: 0, CBR:1, ABR: 2)" << endl;
  cout << std::left << std::setw(opt_len) << "-fps:"              << std::left << std::setw(desc_len) << "video framerate: " << "(default: input video's fps)" << endl;
  cout << std::left << std::setw(opt_len) << "-q, -quantizer:"    << std::left << std::setw(desc_len) << quantizer_info.str() << quantizer_defaults.str() << endl;
  cout << std::left << std::setw(opt_len) << "-vbitrate:"         << std::left << std::setw(desc_len) << "max video bitrate" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-vmaxbitrate:"      << std::left << std::setw(desc_len) << "max video max bitrate" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-dthreads:"         << std::left << std::setw(desc_len) << "H.264 decoder thread count" << "(default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "-ethreads:"         << std::left << std::setw(desc_len) << "H.264 encoder thread count" << "(default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "--vonly:"           << std::left << std::setw(desc_len) << "transcode only video" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "-abitrate:"         << std::left << std::setw(desc_len) << "audio bitrate" << audio_bitrate_defaults.str() << endl;
  cout << std::left << std::setw(opt_len) << "--aonly:"           << std::left << std::setw(desc_len) << "transcode only audio" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "-bframes:"          << std::left << std::setw(desc_len) << "H.264 number of b frames" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "--dashdata:"        << std::left << std::setw(desc_len) << "transcode dash data" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "--dashinit:"        << std::left << std::setw(desc_len) << "transcode dash initializer" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "--samplesonly:"     << std::left << std::setw(desc_len) << "transcode mp4 in samples only mode" << "(default: false)" << endl;
  cout << std::left << std::setw(opt_len) << "-vprofile:"         << std::left << std::setw(desc_len) << "video profile to be used for transcoding, baseline, main or high" << "(default: baseline)" << endl;
  cout << std::left << std::setw(opt_len) << "-refs:"             << std::left << std::setw(desc_len) << "number of references" << "(default: 3)" << endl;
  cout << std::left << std::setw(opt_len) << "--mixed_refs:"      << std::left << std::setw(desc_len) << "allow each mb partition to have its own reference number" << "(default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "-rc_look_ahead:"    << std::left << std::setw(desc_len) << "number of frames used mb-tree ratecontrol and vbv-lookahead" << "(default: 40)" << endl;
  cout << std::left << std::setw(opt_len) << "-aq_mode:"          << std::left << std::setw(desc_len) << "adaptive quantization mode" << "(none: 0, variance: 1, auto variance: 2, auto variance biased: 3, default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "--2nd_pass:"        << std::left << std::setw(desc_len) << "start 2nd pass for dual pass encoding" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-trellis:"          << std::left << std::setw(desc_len) << "performs trellis quantization" << "(default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "-qp_min:"           << std::left << std::setw(desc_len) << "minimum quantizer" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-keyint_max:"       << std::left << std::setw(desc_len) << "maximum interval between IDR-frames" << "(default: 1<<30)" << endl;
  cout << std::left << std::setw(opt_len) << "-keyint_min:"       << std::left << std::setw(desc_len) << "minimum interval between IDR-frames" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "--b_rc_mbtree:"     << std::left << std::setw(desc_len) << "macroblock tree ratecontrol" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-stats_log_path:"   << std::left << std::setw(desc_len) << "input/output path for stats file" << "(default: \"\")" << endl;
  cout << std::left << std::setw(opt_len) << "--rc_b_mb_tree:"    << std::left << std::setw(desc_len) << "enable mb_tree rate control" << "(default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-pyramid_mode:"     << std::left << std::setw(desc_len) << "allow the use of B-frames as references for other frames" << "(none: 0, strcit: 1, normal: 2, default: 0)" << endl;
  cout << std::left << std::setw(opt_len) << "-me_method:"        << std::left << std::setw(desc_len) << "motion estimation method" << "(DIA: 0, HEX: 1, UMH: 2, ESA: 3, TESA: 4, default: 1)" << endl;
  cout << std::left << std::setw(opt_len) << "-subpel_refine:"    << std::left << std::setw(desc_len) << "subpixel motion estimation quality" << "(default: 4)" << endl;
}


struct Config {
  int iterations = 1;
  int start = 0;
  int duration = numeric_limits<int>::max();
  uint16_t height = 0;
  bool square = false;
  int optimization = -1; // -1 indicates not set by user. default value is populated later
  float crf = kH264DefaultCRF;
  int quantizer = kVP8DefaultQuantizer;
  int video_bitrate = 0;
  int max_video_bitrate = 0;
  int buffer_size = 0;
  float buffer_init = 0;
  int decoder_threads = 1;
  int encoder_threads = 1;
  bool video_only = false;
  int audio_bitrate = kDefaultAudioBitrateInKb * 1024;
  bool audio_only = false;
  int bframes = 0;
  encode::PyramidMode pyramid_mode = encode::PyramidMode::Normal;
  bool dash_data = false;
  bool dash_init = false;
  bool samples_only = false;
  encode::VideoProfileType vprofile = encode::VideoProfileType::Baseline;

  string infile = "";
  string outfile = "";
  FileType outfile_type = UnknownFileType;
  encode::RCMethod rc_method = encode::RCMethod::CRF;
  uint32_t frame_references = 3;
  bool mixed_refs = true;
  uint32_t rc_look_ahead =40;
  bool is_second_pass = false;
  encode::AdaptiveQuantizationMode aq_mode = encode::AdaptiveQuantizationMode::Variance;
  uint32_t trellis = 1;
  uint32_t qp_min = 0;
  uint32_t keyint_max = encode::kDefaultH264KeyintMax;
  uint32_t keyint_min = encode::kDefaultH264KeyintMin;
  bool rc_b_mb_tree = false;
  string stats_log_path = "";
  int fps = -1;
  encode::MotionEstimationMethod me_method = encode::MotionEstimationMethod::Hexagon;
  uint32_t subpel_refine = 4;
};

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
    } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "-start") == 0) {
      int arg_start = atoi(argv[++i]);
      if (arg_start < 0) {
        cerr << "start cannot be non-negative" << endl;
        return 1;
      }
      config.start = (int)arg_start;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "-duration") == 0) {
      int arg_duration = atoi(argv[++i]);
      if (arg_duration <= 0) {
        cerr << "duration must be positive" << endl;
        return 1;
      }
      config.duration = (int)arg_duration;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "-height") == 0) {
      int arg_height = atoi(argv[++i]);
      if (arg_height < 0 || arg_height > 4096) {
        cerr << "height has to be positive and less than 4096" << endl;
        return 1;
      }
      config.height = (uint16_t)arg_height;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--square") == 0) {
      config.square = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "-optimization") == 0) {
      int arg_optimization = atoi(argv[++i]);
      if (arg_optimization < 0) {
        cerr << "optimization level has to be non-negative" << endl;
        return 1;
      }
      config.optimization = arg_optimization;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "-crf") == 0) {
      float arg_crf = atof(argv[++i]);
      if (arg_crf < encode::kH264MinCRF || arg_crf > encode::kH264MaxCRF) {
        cerr << "crf has to be between " << encode::kH264MinCRF << " - " << encode::kH264MaxCRF << endl;
        return 1;
      }
      config.crf = arg_crf;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "-quantizer") == 0) {
      int arg_quantizer = atoi(argv[++i]);
      if (arg_quantizer < encode::kVP8MinQuantizer || arg_quantizer > encode::kVP8MaxQuantizer) {
        cerr << "quantizer has to be between " << encode::kVP8MinQuantizer << " - " << encode::kVP8MaxQuantizer << endl;
        return 1;
      }
      config.quantizer = arg_quantizer;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-vbitrate") == 0) {
      int arg_video_bitrate = atoi(argv[++i]);
      if (arg_video_bitrate < 0) {
        cerr << "video bitrate has to be non-negative" << endl;
        return 1;
      }
      config.video_bitrate = (int)arg_video_bitrate;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-vmaxbitrate") == 0) {
      int arg_max_video_bitrate = atoi(argv[++i]);
      if (arg_max_video_bitrate < 0) {
        cerr << "video max bitrate has to be non-negative" << endl;
        return 1;
      }
      config.max_video_bitrate = (int)arg_max_video_bitrate;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-dthreads") == 0) {
      int arg_decoder_threads = atoi(argv[++i]);
      if (arg_decoder_threads < 0 || arg_decoder_threads > kMaxThreads) {
        cerr << "decoder thread count has to be between 1 and " << kMaxThreads << endl;
        return 1;
      }
      config.decoder_threads = (int)arg_decoder_threads;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-ethreads") == 0) {
      int arg_encoder_threads = atoi(argv[++i]);
      if (arg_encoder_threads < 0 || arg_encoder_threads > kMaxThreads) {
        cerr << "encoder thread count has to be between 1 and " << kMaxThreads << endl;
        return 1;
      }
      config.encoder_threads = (int)arg_encoder_threads;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--vonly") == 0) {
      config.video_only = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-abitrate") == 0) {
      int arg_audio_bitrate = atoi(argv[++i]);
      if (arg_audio_bitrate < 0) {
        cerr << "audio bitrate has to be non-negative" << endl;
        return 1;
      }
      config.audio_bitrate = (int)arg_audio_bitrate;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--aonly") == 0) {
      config.audio_only = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-bframes") == 0) {
      config.bframes = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--dashdata") == 0) {
      config.dash_data = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--dashinit") == 0) {
      config.dash_init = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--samplesonly") == 0) {
      config.samples_only = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-vprofile") == 0) {
      if (strcmp(argv[++i], "baseline") == 0) {
        config.vprofile = vireo::encode::VideoProfileType::Baseline;
      } else if (strcmp(argv[i], "main") == 0){
        config.vprofile = vireo::encode::VideoProfileType::Main;
      } else if (strcmp(argv[i], "high") == 0){
        config.vprofile = vireo::encode::VideoProfileType::High;
      } else {
        cerr << "only baseline, main or high profile is supported" << endl;
        return 1;
      }
    } else if (strcmp(argv[i], "-rc_method") == 0) {
      config.rc_method = (vireo::encode::RCMethod)atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-refs") == 0) {
      config.frame_references = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--mixed_refs") == 0) {
      config.mixed_refs = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-rc_look_ahead") == 0) {
      config.rc_look_ahead = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--2nd_pass") == 0) {
      config.is_second_pass = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-aq_mode") == 0) {
      auto mode = atoi(argv[++i]);
      config.aq_mode = encode::AdaptiveQuantizationMode(mode);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-trellis") == 0) {
      config.trellis = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-qp_min") == 0) {
      config.qp_min = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-keyint_max") == 0) {
      config.keyint_max = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-keyint_min") == 0) {
      config.keyint_min = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--rc_b_mb_tree") == 0) {
      config.rc_b_mb_tree = (bool)atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-stats_log_path") == 0) {
      config.stats_log_path = argv[++i];
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-fps") == 0) {
      config.fps = atoi(argv[++i]);
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-pyramid_mode") == 0) {
      config.pyramid_mode = encode::PyramidMode(atoi(argv[++i]));
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-me_method") == 0) {
      config.me_method = encode::MotionEstimationMethod(atoi(argv[++i]));
      last_arg = i + 1;
    } else if (strcmp(argv[i], "-subpel_refine") == 0) {
      config.subpel_refine = atoi(argv[++i]);
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
  for (auto ext_file_type: ext_file_types) {
    auto ext = ext_file_type.ext;
    auto file_type = ext_file_type.outfile_type;
    if (config.outfile.find(ext) != string::npos) {
      config.outfile_type = file_type;
      break;
    }
  }
  if (config.outfile_type == UnknownFileType) {
    cerr << "Output content type is unknown" << endl;
    return 1;
  }

  auto max_optimization = (config.outfile_type == MP4 || config.outfile_type == MP2TS) ? encode::kH264MaxOptimization : encode::kVP8MaxOptimization;
  auto default_optimization = (config.outfile_type == MP4 || config.outfile_type == MP2TS) ? kH264DefaultOptimization : kVP8DefaultOptimization;
  if (config.optimization == -1) {
    config.optimization = default_optimization;
  } else if (config.optimization > max_optimization) {
    cerr << "optimization level has to be between 0 and " << max_optimization << endl;
    return 1;
  }

  return 0;
}

struct FirstPtsAndTimescale {
  int64_t first_pts = -1;
  uint32_t timescale = 0;
};

bool include_pts(const uint64_t pts,
                 const uint32_t timescale,
                 const vector<common::EditBox>& edit_boxes,
                 const uint32_t start,
                 const uint32_t duration,
                 FirstPtsAndTimescale& first_pts_and_timescale) {
  auto new_pts = common::EditBox::RealPts(edit_boxes, pts);
  auto time = 1000.0f * new_pts / timescale;
  if (new_pts >= 0 && time >= start && time < (start + duration)) {
    auto first_pts = first_pts_and_timescale.first_pts;
    if (first_pts == -1) {
      first_pts_and_timescale.first_pts = new_pts;
      first_pts_and_timescale.timescale = timescale;
    }
    int64_t scaled_first_pts = first_pts_and_timescale.first_pts * timescale / first_pts_and_timescale.timescale;
    return ((int64_t)new_pts - scaled_first_pts) >= 0;
  }
  return false;
};

struct Resolution {
  const uint16_t width;
  const uint16_t height;
};

Resolution out_resolution(uint16_t in_width, uint16_t in_height, settings::Video::Orientation in_orientation, uint16_t out_height, bool square) {
  const uint16_t min_dim = min(in_width, in_height);
  uint16_t w = (out_height == 0) ? in_width : common::round_divide((uint32_t)in_width, (uint32_t)out_height, (uint32_t)min_dim);
  uint16_t h = (out_height == 0) ? in_height : common::round_divide((uint32_t)in_height, (uint32_t)out_height, (uint32_t)min_dim);
  if (square) {
    w = min(w, h);
    h = w;
  }
  if (in_orientation % 2) {
    uint16_t tmp = w;
    w = h;
    h = tmp;
  }
  return { w, h };
};

functional::Video<encode::Sample> transcode(const functional::Video<decode::Sample> track,
                                            const float fps,
                                            const vector<common::EditBox> &edit_boxes,
                                            const Config config,
                                            FirstPtsAndTimescale &first_pts_and_timescale,
                                            const bool print_info) {
  settings::Video video_settings = track.settings();
  THROW_IF(video_settings.codec != settings::Video::Codec::H264, Unsupported);

  const uint16_t in_width = video_settings.width;
  const uint16_t in_height = video_settings.height;
  const settings::Video::Orientation in_orientation = video_settings.orientation;
  auto resolution = out_resolution(in_width, in_height, in_orientation, config.height, config.square);
  const uint16_t out_width = resolution.width;
  const uint16_t out_height = resolution.height;

  if (print_info) {
    cout << "Video resolution " << out_width << "x" << out_height;
    if (out_width != in_width || out_height != in_height) {
      cout << ", resized from " << in_width << "x" << in_height;
    }
    cout << endl << "video framerate = " << fps << "fps";
    cout << endl << "Optimization = " << config.optimization;
    if (config.outfile_type == MP4 || config.outfile_type == MP2TS) {
      if (config.rc_method == vireo::encode::RCMethod::CRF) {
        cout << ", CRF = " << config.crf  << ", number of b-frames = " << config.bframes;
      } else {
        cout << ", bitrate = " << config.video_bitrate;
      }
    } else {
      cout << ", Quantizer = " << config.quantizer;
    }

    if (config.max_video_bitrate) {
      cout << ", max bitrate = " << config.max_video_bitrate;
    }
    cout << endl << "Threads = " << config.decoder_threads << " (decoder)";
    if (config.outfile_type == MP4 || config.outfile_type == MP2TS) {
      cout << ", " << config.encoder_threads;
    } else {
      cout << ", 1";
    }
    cout << " (encoder)" << endl;
    cout << "Video Profile = " << vireo::encode::kVideoProfileTypeToString[config.vprofile] << endl;
    cout << "Output type = " << kFileTypeToString[config.outfile_type];
    if (config.dash_init) {
      cout << ", dash_init" << endl;
    } else if (config.dash_data) {
      cout << ", dash_data" << endl;
    } else {
      cout << endl;
    }
  }

  auto crop_scale_rotate = [](frame::Frame frame, uint16_t in_width, uint16_t in_height, settings::Video::Orientation orientation, uint16_t out_width, uint16_t out_height) -> frame::Frame {
    return (frame::Frame){ frame.pts, [in_width, in_height, orientation, out_width, out_height, yuv_func = frame.yuv]() {
      // crop - if necessary
      const bool square = (out_width == out_height);
      const uint16_t min_dim = min(in_width, in_height);
      const uint16_t crop_x_offset = square ? (in_width - min_dim) / 2 : 0;
      const uint16_t crop_y_offset = square ? (in_height - min_dim) / 2 : 0;
      auto yuv_cropped_func = (crop_x_offset == 0 && crop_y_offset == 0) ? yuv_func : [crop_x_offset, crop_y_offset, min_dim, &yuv_func](){
        return yuv_func().crop(crop_x_offset, crop_y_offset, min_dim, min_dim);
      };
      const uint16_t cropped_width = in_width - 2 * crop_x_offset;
      const uint16_t cropped_height = in_height - 2 * crop_y_offset;
      // scale - if necessary
      const bool is_portrait = orientation % 2;
      const uint16_t real_height = is_portrait ? cropped_width : cropped_height;
      auto yuv_scaled_func = (real_height == out_height) ? yuv_cropped_func : [real_height, out_height, &yuv_cropped_func](){
        return yuv_cropped_func().scale(out_height, real_height);
      };
      // rotate - if necessary
      auto yuv_rotated_func = (orientation == settings::Video::Landscape) ? yuv_scaled_func : [orientation, &yuv_scaled_func](){
        return yuv_scaled_func().rotate((frame::Rotation)orientation);
      };
      return yuv_rotated_func();
    }};
  };

  auto decoder = decode::Video(track, config.decoder_threads).filter(
    [&edit_boxes, timescale = video_settings.timescale, start = config.start, duration = config.duration, &first_pts_and_timescale](const frame::Frame& frame) {
      return include_pts(frame.pts, timescale, edit_boxes, start, duration, first_pts_and_timescale);
    }
  ).transform<frame::Frame>(
    [&edit_boxes, timescale = video_settings.timescale, &first_pts_and_timescale, &crop_scale_rotate, in_width, in_height, in_orientation, out_width, out_height](const frame::Frame& frame) {
      int64_t first_pts = first_pts_and_timescale.first_pts;
      CHECK(first_pts >= 0);
      int64_t scaled_first_pts = first_pts * timescale / first_pts_and_timescale.timescale;
      return crop_scale_rotate(frame.adjust_pts(edit_boxes).shift_pts(-scaled_first_pts), in_width, in_height, in_orientation, out_width, out_height);
    }
  );

  auto output_video_settings = settings::Settings<SampleType::Video>(decoder.settings().codec, out_width, out_height, video_settings.timescale, settings::Video::Landscape, decoder.settings().sps_pps);

  if (config.outfile_type == MP4 || config.outfile_type == MP2TS) {
    auto computation = encode::H264Params::ComputationalParams(config.optimization, config.encoder_threads);
    auto rc = encode::H264Params::RateControlParams(config.rc_method, config.crf, config.max_video_bitrate, config.video_bitrate, config.buffer_size, config.buffer_init, config.rc_look_ahead, config.is_second_pass, config.rc_b_mb_tree, config.aq_mode, config.qp_min, config.stats_log_path, config.mixed_refs, config.trellis, config.me_method, config.subpel_refine);
    auto gop = encode::H264Params::GopParams(config.bframes, config.pyramid_mode, config.keyint_max, config.keyint_min, config.frame_references);
    encode::H264Params params(computation, rc, gop, config.vprofile, fps);
    return encode::H264(functional::Video<frame::Frame>(decoder, output_video_settings), params);
  } else {
    return encode::VP8(functional::Video<frame::Frame>(decoder, output_video_settings), config.quantizer, config.optimization, fps, config.max_video_bitrate);
  }
};

functional::Audio<encode::Sample> transcode(const functional::Audio<decode::Sample> track,
                                            const vector<common::EditBox> &edit_boxes,
                                            const Config config,
                                            FirstPtsAndTimescale &first_pts_and_timescale,
                                            const bool print_info) {
  settings::Audio audio_settings = track.settings();

  if (print_info) {
    cout << "Audio channels = " << (int)audio_settings.channels << ", bitrate = " << (config.audio_bitrate / 1024.0f) << " Kbps" << endl;
  }

  auto decoder = decode::Audio(track).filter(
    [&edit_boxes, timescale = audio_settings.timescale, start = config.start, duration = config.duration, &first_pts_and_timescale](const sound::Sound& sound) {
      return include_pts(sound.pts, timescale, edit_boxes, start, duration, first_pts_and_timescale);
    }
  ).transform<sound::Sound>(
    [&edit_boxes, timescale = audio_settings.timescale, first_pts_and_timescale](const sound::Sound& sound) {
      int64_t first_pts = first_pts_and_timescale.first_pts;
      CHECK(first_pts >= 0);
      int64_t scaled_first_pts = first_pts * timescale / first_pts_and_timescale.timescale;
      return sound.adjust_pts(edit_boxes).shift_pts(-scaled_first_pts);
    }
  );

  if (config.outfile_type == MP4 || config.outfile_type == MP2TS) {
    return encode::AAC(decoder, audio_settings.channels, config.audio_bitrate);
  } else {
    return encode::Vorbis(decoder, audio_settings.channels, config.audio_bitrate);
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

    // Read input file
    demux::Movie movie(config.infile);

    // Process arguments
    if (config.video_only && config.audio_only) {
      cerr << "Only allow one of the --aonly --vonly parameters" << endl;
      return 1;
    } else if (config.video_only && movie.video_track.settings().timescale == 0) {
      cerr << "File does not contain a valid video track" << endl;
      return 1;
    } else if (config.audio_only && (movie.audio_track.settings().timescale == 0 || movie.audio_track.settings().sample_rate == 0)) {
      cerr << "File does not contain a valid audio track" << endl;
      return 1;
    } else if (config.dash_data && config.dash_init) {
      cerr << "Only allow one of the --dashdata --dashinit parameters" << endl;
      return 1;
    } else if (movie.video_track.settings().timescale == 0 && (movie.audio_track.settings().timescale == 0 || movie.audio_track.settings().sample_rate == 0)) {
      cerr << "File does not contain any audio / video tracks" << endl;
      return 1;
    }

    bool transcode_video = (config.audio_only || (movie.video_track.duration() == 0)) ? false : true;
    bool transcode_audio = (config.video_only || (movie.audio_track.duration() == 0)) ? false : true;
    THROW_IF(!transcode_video && !transcode_audio, Invalid);

    auto input_duration = transcode_video ? movie.video_track.duration() : movie.audio_track.duration();
    auto timescale = transcode_video ? movie.video_track.settings().timescale : movie.audio_track.settings().timescale;
    int input_duration_in_ms = 1000.0f * input_duration / timescale;
    int input_start_in_ms = 0;
    if (input_duration_in_ms) {
      auto input_start = transcode_video ? movie.video_track(0).pts : movie.audio_track(0).pts;
      input_start_in_ms = 1000.0f * input_start / timescale;
      THROW_IF(input_start_in_ms < 0, Unsupported);
    }
    int input_end_in_ms = input_start_in_ms + input_duration_in_ms;

    int end = (int)min(config.start + (uint64_t)config.duration, (uint64_t)numeric_limits<int>::max());
    config.start = max(config.start, input_start_in_ms);
    config.duration = max(min(end, input_end_in_ms) - config.start, 0);
    if (config.duration == 0) {
      cout << "No video content in the given time range" << endl;
      return 1;
    }

    // Main
    cout << "Transcoding " << (transcode_video ? (transcode_audio ? "video with audio" : "video") : "audio");
    cout << " of duration " << config.duration << " ms, starting from " << config.start << " ms" << endl;

    uint32_t i = 0;
    cout << Profile::Function("Transcoding", [&]{
      // Keep track of the first pts of the encoded tracks (could be either audio or video)
      FirstPtsAndTimescale first_pts_and_timescale;

      // Get output video track
      auto output_video_track = functional::Video<encode::Sample>();
      auto fps = (config.fps == -1) ? movie.video_track.fps() : config.fps;
      if (transcode_video) {
        output_video_track = transcode(movie.video_track, fps, movie.video_track.edit_boxes(), config, first_pts_and_timescale, i == 0);
      }

      // Get output audio track
      auto output_audio_track = functional::Audio<encode::Sample>();
      if (transcode_audio) {
        output_audio_track = transcode(movie.audio_track, movie.audio_track.edit_boxes(), config, first_pts_and_timescale, i == 0);
      }

      // Get output caption track
      auto output_caption_track = functional::Caption<encode::Sample>();
      if (transcode_video) {
        auto trimmed_caption = transform::Trim<SampleType::Caption>(movie.caption_track, movie.caption_track.edit_boxes(), config.start, config.duration);
        output_caption_track = functional::Caption<encode::Sample>(trimmed_caption.track, encode::Sample::Convert);
      }

      // Create necessary encoder
      functional::Function<common::Data32> encoder;
      if (config.outfile_type == MP4) {
        FileFormat format = FileFormat::Regular;
        if (config.dash_data) {
          format = FileFormat::DashData;
        } else if (config.dash_init) {
          format = FileFormat::DashInitializer;
        } else if (config.samples_only) {
          format = FileFormat::SamplesOnly;
        }
        encoder = mux::MP4(output_audio_track, output_video_track, output_caption_track, format);
      } else if (config.outfile_type == MP2TS) {
        encoder = mux::MP2TS(output_audio_track, output_video_track, output_caption_track);
      } else {
        encoder = mux::WebM(output_audio_track, output_video_track);
      }

      // Start encoding and save the output file once
      const string abs_dst = common::Path::MakeAbsolute(config.outfile);
      if (i == 0) {
        util::save(abs_dst, encoder());
      } else {
        encoder();
      }
      ++i;
    }, config.iterations) << endl;
  } __catch (std::exception& e) {
    cerr << "Error transcoding movie: " << e.what() << endl;
    return 1;
  }
  return 0;
}
