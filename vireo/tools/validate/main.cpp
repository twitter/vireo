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
#include <fcntl.h>
#include <fstream>
#include <future>

#include "vireo/base_cpp.h"
#include "vireo/common/editbox.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/decode/audio.h"
#include "vireo/decode/video.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/types.h"
#include "vireo/error/error.h"
#include "vireo/version.h"

#include "version.h"

using namespace vireo;

using std::cin;
using std::ofstream;
using std::set;
using std::vector;

#define USE_STDIN 1

static const uint32_t kMaxSampleCount = 0x4000;

int main(int argc, const char* argv[]) {
  int threads = 0;
  int last_arg = 1;
  const string name = common::Path::Filename(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-threads") == 0) {
      int arg_threads = atoi(argv[++i]);
      if (arg_threads < 0 || arg_threads > 64) {
        cerr << "Invalid number of threads" << endl;
        return 1;
      }
      threads = (uint16_t)arg_threads;
      last_arg = i + 1;
    } else if (strcmp(argv[i], "--version") == 0) {
      cout << name << " version " << VALIDATE_VERSION << " (based on vireo " << VIREO_VERSION << ")" << endl;
      return 0;
    } else if (strcmp(argv[i], "--help") == 0) {
      cout << "Usage: " << name << " [options] < infile" << endl;
      cout << "\nOptions:" << endl;
      cout << "-threads:\tnumber of threads (default: 0)" << endl;
      cout << "--help:\t\tshow usage" << endl;
      cout << "--version:\tshow version" << endl;
      return 0;
    } else {
#if (USE_STDIN)
      cerr << "Invalid argument: " << argv[i] << " (--help for usage)" << endl;
      return 1;
#endif
    }
  }

  __try {
    // Read stdin into buffer
    static const size_t kSize_Default = 512 * 1024;
    common::Data32 tmp_buffer(new uint8_t[65536], 65536, [](uint8_t* p) { delete[] p; });
    unique_ptr<common::Data32> buffer;
#if (USE_STDIN)
    int fd = fcntl(STDIN_FILENO,  F_DUPFD, 0);
#else
    int fd = open(argv[last_arg], O_RDONLY);
#endif
    for (uint16_t i = 0, max_i = 10000; i < max_i; ++i) {
      size_t read_size = read((int)fd, (void*)tmp_buffer.data(), (size_t)tmp_buffer.capacity());
      if (!read_size) {
        break;
      }
      tmp_buffer.set_bounds(0, (uint32_t)read_size);
      if (!buffer.get()) {
        buffer.reset(new common::Data32(new uint8_t[kSize_Default], kSize_Default, [](uint8_t* p) { delete[] p; }));
        buffer->set_bounds(0, 0);
      } else if (read_size + buffer->b() > buffer->capacity()) {
        uint32_t new_capacity = buffer->capacity() + ((buffer->b() + (uint32_t)read_size + kSize_Default - 1) / kSize_Default) * kSize_Default;
        auto new_buffer = new common::Data32(new uint8_t[new_capacity], new_capacity, [](uint8_t* p) { delete[] p; });
        new_buffer->copy(*buffer);
        buffer.reset(new_buffer);
      }
      CHECK(buffer->a() + tmp_buffer.count() <= buffer->capacity());
      buffer->set_bounds(buffer->b(), buffer->b());
      buffer->copy(tmp_buffer);
      buffer->set_bounds(0, buffer->b());
    };
    close(fd);
    THROW_IF(!buffer.get() || !buffer->count(), Invalid);

    vireo::demux::Movie movie(move(*buffer));
    THROW_IF(movie.file_type() != FileType::MP4 && movie.file_type() != FileType::MP2TS, Unsupported);

    THROW_IF(movie.video_track.count() >= kMaxSampleCount, Unsafe);
    THROW_IF(movie.audio_track.count() >= kMaxSampleCount, Unsafe);

    const auto& video_settings = movie.video_track.settings();
    const auto& audio_settings = movie.audio_track.settings();

    THROW_IF(video_settings.codec == settings::Video::Codec::VP8, Unsupported);
    THROW_IF(video_settings.codec == settings::Video::Codec::MPEG4, Unsupported);
    THROW_IF(video_settings.codec == settings::Video::Codec::ProRes, Unsupported);
    THROW_IF(video_settings.codec != settings::Video::Codec::H264, Unsupported);

    if (movie.audio_track.count()) {
      THROW_IF(audio_settings.codec == settings::Audio::Codec::AAC_Main, Unsupported);
      THROW_IF(audio_settings.codec == settings::Audio::Codec::Vorbis, Unsupported);
      THROW_IF(settings::Audio::IsPCM(audio_settings.codec), Unsupported);
      THROW_IF(!settings::Audio::IsAAC(audio_settings.codec), Unsupported);
    }

    THROW_IF(!movie.video_track.count(), Invalid);
    THROW_IF(!movie.video_track(0).keyframe, Invalid);

    THROW_IF(!common::EditBox::Valid(movie.video_track.edit_boxes()), Unsupported);
    THROW_IF(!common::EditBox::Valid(movie.audio_track.edit_boxes()), Unsupported);

    tuple<settings::Video, settings::Audio> metadata = make_tuple(video_settings, audio_settings);

    // Cache all nal units
    vector<encode::Sample> video_samples;
    vector<encode::Sample> audio_samples;
    for (const auto& sample: movie.video_track) {
      if (!video_samples.empty()) {
        THROW_IF(sample.dts <= video_samples.back().dts, Invalid, "Non-increasing DTS values in video track (" << sample.dts << " >= " << video_samples.back().dts << ")");
      }
      video_samples.push_back((encode::Sample) {
        sample.pts,
        sample.dts,
        sample.keyframe,
        sample.type,
        sample.nal()
      });
    }
    for (const auto& sample: movie.audio_track) {
      if (!audio_samples.empty()) {
        THROW_IF(sample.dts <= audio_samples.back().dts, Invalid, "Non-increasing DTS values in audio track (" << sample.dts << " >= " << audio_samples.back().dts << ")");
        THROW_IF(sample.pts <= audio_samples.back().pts, Invalid, "Non-increasing PTS values in audio track (" << sample.pts << " >= " << audio_samples.back().pts << ")");
      }
      audio_samples.push_back((encode::Sample) {
        sample.pts,
        sample.dts,
        sample.keyframe,
        sample.type,
        sample.nal()
      });
    }

    // Video decode setup
    std::atomic<uint16_t> total_decoded_frames(0);
    auto decode_frames = [&video_samples, &metadata, &total_decoded_frames](uint64_t start_dts, uint64_t end_dts){
      vector<decode::Sample> samples_to_decode;
      for (const auto& sample: video_samples) {
        if (sample.dts < start_dts) {
          continue;
        }
        if (sample.dts > end_dts) {
          THROW_IF(!sample.keyframe, Invalid);
          break;
        }
        samples_to_decode.push_back((decode::Sample){ sample.pts, sample.dts, sample.keyframe, sample.type, [&]() { return sample.nal; } });
      }

      // we need to decode every GOP individually to make sure we can decode after chunking
      auto decode_gop = [&samples_to_decode, &metadata, &total_decoded_frames](uint32_t start_index, uint32_t end_index) {
        const auto& video_settings = get<0>(metadata);
        THROW_IF(start_index >= end_index, InvalidArguments);
        THROW_IF(end_index > samples_to_decode.size(), InvalidArguments);
        const uint32_t gop_size = end_index - start_index;
        THROW_IF(gop_size > security::kMaxGOPSize, Unsafe,
                 "GOP is too large (frames [" << start_index << ", " << end_index << ") - max allowed = " << security::kMaxGOPSize << ")");
        auto from = samples_to_decode.begin() + start_index;
        auto until = samples_to_decode.begin() + end_index;
        vector<decode::Sample> gop(from, until);
        functional::Video<decode::Sample> video_track(gop, video_settings);
        decode::Video video_decoder(video_track);
        for (auto frame: video_decoder) {
          frame.yuv();
          total_decoded_frames.fetch_add(1);
        }
      };

      uint32_t start_index = 0;
      for (uint32_t index = 1; index <= samples_to_decode.size(); ++index) {
        if (index == samples_to_decode.size() || samples_to_decode[index].keyframe) {
          decode_gop(start_index, index);
          start_index = index;
        }
      }
    };

    // Decode all audio samples
    if (audio_samples.size()) {
      vector<decode::Sample> samples_to_decode;
      for (const auto& sample: audio_samples) {
        samples_to_decode.push_back((decode::Sample){ sample.pts, sample.dts, sample.keyframe, sample.type, [&]() { return sample.nal; } });
      }
      const auto& audio_settings = get<1>(metadata);
      functional::Audio<decode::Sample> audio_track(samples_to_decode, audio_settings);
      decode::Audio audio_decoder(audio_track);
      for (auto sound: audio_decoder) {
        sound.pcm();
      }
    }

    // Decode all video samples
    if (video_samples.size()) {
      const uint64_t final_dts = video_samples.back().dts;
      if (threads == 0) {
        decode_frames(0, final_dts);
      } else {
        vector<std::future<void>> futures;
        const uint64_t thread_duration = common::round_divide(final_dts, (uint64_t)1, (uint64_t)threads);
        uint64_t start_dts = 0;
        uint64_t end_dts = thread_duration;
        uint64_t prev_dts = 0;
        for (const auto& sample: video_samples) {
          if (sample.dts == final_dts) {
            futures.push_back(std::async(std::launch::async, decode_frames, start_dts, final_dts));
          } else if (sample.keyframe && sample.dts > end_dts) {
            futures.push_back(std::async(std::launch::async, decode_frames, start_dts, prev_dts));
            start_dts = sample.dts;
            end_dts = start_dts + thread_duration;
          }
          prev_dts = sample.dts;
        }
        for (auto& running_future: futures) {
          if (running_future.valid()) {
            running_future.get();
          }
        }
      }
      CHECK(total_decoded_frames.load() == video_samples.size());
    }

    cout << "success" << endl;
  } __catch (std::exception& e) {
    string err = string(e.what());
    cout << "fail: " << err << endl;
    if (err.find("!intra_decode_refresh") != std::string::npos) {  // TODO: remove once MEDIASERV-4386 is resolved
      return 2;  // return a special error code for !intra_decode_refresh to track occurence of l-smash bug (MEDIASERV-4386)
    }
    return 1;
  }
  return 0;
}
