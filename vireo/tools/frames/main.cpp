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
#include "vireo/common/path.h"
#include "vireo/constants.h"
#include "vireo/demux/movie.h"
#include "vireo/util/util.h"

using namespace vireo;

using std::vector;

int main(int argc, const char* argv[]) {
  if (argc < 2) {
    const string name = common::Path::Filename(argv[0]);
    cout << "Usage: " << name << " [options] input" << endl;
    cout << "\nOptions:" << endl;
    cout << "--audio:\tprint audio samples (default: false)" << endl;
    cout << "--video:\tprint video samples (default: false)" << endl;
    cout << "--data:\tprint data samples (default: false)" << endl;
    cout << "--samples:\tprint ordered list of samples (default: false)" << endl;
    return 1;
  }

  // Parse arguments
  int last_arg = 1;
  bool audio = false;
  bool video = false;
  bool data = false;
  bool samples = false;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--video") == 0) {
      video = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--audio") == 0) {
      audio = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--data") == 0) {
      data = true;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--samples") == 0) {
      samples = true;
      last_arg = i + 1;
    }
  }
  if (last_arg >= argc) {
    cerr << "Need to specify input file" << endl;
    return 1;
  }
  if ((audio | video) & samples) {
    cerr << "--samples cannot be used with --video or --audio" << endl;
    return 1;
  }

  const string seperator = ", ";
  const uint32_t bitrate_width = 2;
  const uint32_t bitrate_precision = 2;
  const uint32_t time_width = 3;
  const uint32_t time_precision = 3;

  auto print_sample = [seperator, time_precision, time_width](const decode::Sample& sample, uint64_t index, float play_time = -1) -> void {
    const uint32_t index_width = 5;
    const uint32_t pts_dts_width = 8;
    const uint32_t pos_width = 10;
    const uint32_t size_width = 8;
    if (index && sample.keyframe && sample.type == SampleType::Video) {
      cout << endl;
    }
    cout << "    ";
    cout << "SAMPLE";
    cout << " ";
    cout << std::setw(index_width) << index;
    cout << " :: ";
    cout << (sample.type == SampleType::Audio ? "AUDIO" : (sample.type == SampleType::Data ? "DATA" : "VIDEO"));
    cout << " :: ";
    if (sample.byte_range.available) {
      cout << "POS = " << std::setw(pos_width) << sample.byte_range.pos << " bytes";
      cout << seperator;
      cout << "SIZE = " << std::setw(size_width) << sample.byte_range.size << " bytes";
      cout << seperator;
    }
    cout << "PTS = " << std::setw(pts_dts_width) << sample.pts;
    cout << seperator;
    cout << "DTS = " << std::setw(pts_dts_width) << sample.dts;
    if (play_time >= 0) {
      cout << seperator;
      cout << "TIME = " << std::fixed << std::showpoint << std::setw(time_width + time_precision) << std::setprecision(time_precision) << play_time << " s";
    }
    if (sample.keyframe && sample.type == SampleType::Video) {
      cout << " : KEY";
    } else if (!sample.keyframe && sample.type == SampleType::Audio) {
      cout << " : NOT KEY";
    }
    cout << endl;
  };

  auto print_edit_boxes = [&seperator](const vector<vireo::common::EditBox>& edit_boxes) -> void {
    const uint32_t index_width = 1;
    const uint32_t pts_dts_width = 8;
    cout << endl;
    uint64_t index = 0;
    for (auto edit_box: edit_boxes) {
      cout << "    ";
      cout << "EDIT BOX ";
      cout << std::setw(index_width) << index;
      cout << " :: ";
      cout << "START PTS = " << std::setw(pts_dts_width) << edit_box.start_pts;
      cout << seperator;
      cout << "DURATION PTS = " << std::setw(pts_dts_width) << edit_box.duration_pts;
      cout << endl;
      ++index;
    }
    if (index == 0) {
      cout << "    ";
      cout << "NO EDIT BOXES" << endl;
    }
    cout << endl;
  };

  auto print_ordered_samples = [&print_sample](vector<decode::Sample>& samples) -> void {
    sort(samples.begin(), samples.end(), [](const decode::Sample& a, const decode::Sample& b){
      return a.byte_range.pos < b.byte_range.pos;
    });
    uint32_t a_index = 0;
    uint32_t v_index = 0;
    for (const auto& sample: samples) {
      THROW_IF(sample.type == SampleType::Data, Unsupported, "not implemented");
      uint32_t index = (sample.type == SampleType::Video) ? v_index++ : a_index++;
      print_sample(sample, index);
    }
  };

  auto print_video_track = [&](const functional::Video<decode::Sample>& video_track,
                               const uint64_t duration,
                               const vector<common::EditBox>& edit_boxes) -> void {
    const auto& settings = video_track.settings();
    cout << "  ";
    cout << "VIDEO TRACK";
    cout << " ::: ";
    cout << "WIDTH = " << settings.width;
    cout << seperator;
    cout << "HEIGHT = " << settings.height;
    cout << seperator;
    cout << "PAR WIDTH = " << settings.par_width;
    cout << seperator;
    cout << "PAR HEIGHT = " << settings.par_height;
    cout << seperator;
    cout << "CODED WIDTH = " << settings.coded_width;
    cout << seperator;
    cout << "CODED HEIGHT = " << settings.coded_height;
    cout << seperator;
    cout << "ORIENTATION = " << settings::kOrientationToString[settings.orientation];
    cout << seperator;
    cout << "TIMESCALE = " << settings.timescale;
    cout << seperator;
    cout << "DURATION = " << duration;
    float duration_in_sec = (float)duration / settings.timescale;
    cout << " (" << std::fixed << std::showpoint << std::setw(time_width + time_precision) << std::setprecision(time_precision) << duration_in_sec << " s)";
    if (duration) {
      // Calculate average bitrate
      uint64_t bytes = 0;
      for (const auto& sample: video_track) {
        bytes += sample.byte_range.available ? sample.byte_range.size : sample.nal().count();
      }
      const float bitrate = (float)bytes * settings.timescale * CHAR_BIT / (1000.0f * duration);
      cout << seperator;
      cout << "BITRATE = " << std::fixed << std::showpoint << std::setw(bitrate_width + bitrate_precision) << std::setprecision(bitrate_precision) << bitrate << " kbps";
    }
    cout << seperator;
    cout << "CODEC = " << settings::kVideoCodecToString[settings.codec];
    cout << endl;

    // Edit boxes and PTS/DTS info
    if (video) {
      print_edit_boxes(edit_boxes);
      uint64_t index = 0;
      for (const auto& sample: video_track) {
        float play_time = (float)common::EditBox::RealPts(edit_boxes, sample.pts) / settings.timescale;
        print_sample(sample, index++, play_time);
      }
    }
  };

  auto print_audio_track = [&](const functional::Audio<decode::Sample>& audio_track,
                               const uint64_t duration,
                               const vector<common::EditBox>& edit_boxes) -> void {
    const auto& settings = audio_track.settings();
    cout << "  ";
    cout << "AUDIO TRACK";
    cout << " ::: ";
    cout << "TIMESCALE= " << settings.timescale;
    cout << seperator;
    cout << "SAMPLE RATE = " << settings.sample_rate;
    cout << seperator;
    cout << "CHANNELS = " << (uint32_t)settings.channels;
    cout << seperator;
    cout << "DURATION = " << duration;
    float duration_in_sec = (float)duration / settings.timescale;
    cout << " (" << std::fixed << std::showpoint << std::setw(time_width + time_precision) << std::setprecision(time_precision) << duration_in_sec << " s)";
    if (duration) {
      // Calculate average bitrate
      uint64_t bytes = 0;
      for (const auto& sample: audio_track) {
        bytes += sample.byte_range.available ? sample.byte_range.size : sample.nal().count();
      }
      const float bitrate = (float)bytes * settings.timescale * CHAR_BIT / (1000.0f * duration);
      cout << seperator;
      cout << "BITRATE = " << std::fixed << std::showpoint << std::setprecision(bitrate_precision) << bitrate << " kbps";
    }
    cout << seperator;
    cout << "CODEC = " << settings::kAudioCodecToString[settings.codec];
    cout << endl;

    // Edit boxes and PTS/DTS info
    if (audio) {
      print_edit_boxes(edit_boxes);
      uint64_t index = 0;
      for (const auto& sample: audio_track) {
        float play_time = (float)common::EditBox::RealPts(edit_boxes, sample.pts) / settings.timescale;
        print_sample(sample, index++, play_time);
      }
    }
  };

  auto print_data_track = [&](const functional::Data<decode::Sample>& data_track) -> void {
    const auto& settings = data_track.settings();
    cout << "   ";
    cout << "DATA TRACK";
    cout << " ::: ";
    cout << "NUM SAMPLES = " << data_track.count();
    cout << seperator;
    cout << "CODEC = " << settings::kDataCodecToString[settings.codec];
    cout << endl;

    // Edit boxes and PTS/DTS info
    if (data) {
      cout << endl;
      uint64_t index = 0;
      for (const auto& sample: data_track) {
        float play_time = (float)sample.pts / settings.timescale;
        print_sample(sample, index++, play_time);
      }
    }
  };

  const string filename = common::Path::MakeAbsolute(argv[last_arg]);

  try {
    vireo::demux::Movie movie(filename);

    // File name
    cout << "GENERAL INFO :::: ";
    cout << "FILENAME = " << filename;
    cout << seperator;
    cout << "FILE TYPE = " << kFileTypeToString[movie.file_type()];
    cout << endl;

    if (samples) {
      cout << endl;
      auto samples = vector<decode::Sample>();
      for (const auto& sample: movie.audio_track) {
        if (!sample.byte_range.available) {
          cerr << "sample order unknown" << endl;
          return 1;
        }
        samples.push_back(sample);
      }
      for (const auto& sample: movie.video_track) {
        if (!sample.byte_range.available) {
          cerr << "sample order unknown" << endl;
          return 1;
        }
        samples.push_back(sample);
      }
      print_ordered_samples(samples);
    } else {
      // Video track
      if (movie.video_track.settings().timescale) {
        if (video) {
          cout << endl;
        }
        print_video_track(movie.video_track, movie.video_track.duration(), movie.video_track.edit_boxes());
      }

      // Audio track
      if (movie.audio_track.settings().sample_rate) {
        if (video && movie.video_track.settings().timescale) {
          cout << endl;
        }
        if (audio) {
          cout << endl;
        }
        print_audio_track(movie.audio_track, movie.audio_track.duration(), movie.audio_track.edit_boxes());
      }

      // Data track
      if (movie.data_track.settings().timescale) {
        if ((video && movie.video_track.settings().timescale) || (audio && movie.audio_track.settings().sample_rate)) {
          cout << endl;
        }
        if (data) {
          cout << endl;
        }
        print_data_track(movie.data_track);
      }
    }
  } catch (std::exception& e) {
    cerr << "Error analyzing movie " << e.what() << endl;
    return 1;
  }
  return 0;
}
