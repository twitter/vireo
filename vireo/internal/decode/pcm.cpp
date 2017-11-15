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

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/pcm.h"
#include "vireo/sound/pcm.h"
#include "vireo/util/util.h"

namespace vireo {
namespace internal {
namespace decode {

const static size_t kShortToByteRatio = sizeof(int16_t) / sizeof(uint8_t);
const static size_t k24BitToByteRatio = 24 / CHAR_BIT / sizeof(uint8_t);

union {
  uint32_t n;
  uint8_t p[4];
} const static kEndianTester { 0x01020304 };

const static bool IsLittleEndian() {
  return (kEndianTester.p[0] == 0x04) ? true : false;
};

struct _PCM {
  settings::Audio settings;
  functional::Audio<Sample> samples;
  vector<vector<uint32_t>> sound_to_sample_mapping;
  const size_t bytes_per_pcm_sample() {
    switch (settings.codec) {
      case settings::Audio::Codec::PCM_S16LE:
      case settings::Audio::Codec::PCM_S16BE:
        return kShortToByteRatio;
      case settings::Audio::Codec::PCM_S24LE:
      case settings::Audio::Codec::PCM_S24BE:
        return k24BitToByteRatio;
      default:
        CHECK(false);
    }
  };
  const inline uint32_t sound_size() {
    return AUDIO_FRAME_SIZE * settings.channels;
  };
  const inline uint32_t bytes_per_sound() {
    return sound_size() * (uint32_t)bytes_per_pcm_sample();
  };
  void copy_data_to_sound(const common::Data32& data, common::Sample16& sound) {
    const auto bytes_per_sample = bytes_per_pcm_sample();
    const uint32_t pcm_samples_to_copy = data.count() / bytes_per_sample;
    CHECK(data.count() % bytes_per_sample == 0);
    CHECK(IsLittleEndian());  // we convert all output to little endian, has to match the machine endianness!
    if (settings.codec == settings::Audio::Codec::PCM_S16LE) {  // optimization: no need to loop
      sound.copy(common::Sample16((int16_t*)(data.data() + data.a()), pcm_samples_to_copy, nullptr));
    } else {
      CHECK(pcm_samples_to_copy <= sound.count());
      int16_t* out = (int16_t*)sound.data() + sound.a();
      if (settings.codec == settings::Audio::Codec::PCM_S24LE) {
        for (uint32_t i = 0, j = 0; i < data.count(); i += bytes_per_sample, ++j) {
          out[j] = ((int16_t)data(data.a() + i + 2) << CHAR_BIT) + data(data.a() + i + 1);
        }
      } else if (settings.codec == settings::Audio::Codec::PCM_S16BE ||
                 settings.codec == settings::Audio::Codec::PCM_S24BE) {
        for (uint32_t i = 0, j = 0; i < data.count(); i += bytes_per_sample, ++j) {
          out[j] = ((int16_t)data(data.a() + i) << CHAR_BIT) + data(data.a() + i + 1);
        }
      } else {
        CHECK(false);
      }
    }
  };
  _PCM(const settings::Audio& settings) : settings(settings) {
    THROW_IF(settings.channels != 1 && settings.channels != 2, Unsupported);
    THROW_IF(!settings::Audio::IsPCM(settings.codec), Unsupported);
    THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), settings.sample_rate) == kSampleRate.end(), Unsupported);
  }
  void init(const functional::Audio<Sample>& _samples) {
    samples = _samples;
    uint32_t bytes_accumulated = 0;
    vector<uint32_t> indices;
    for (uint32_t index = 0; index < samples.count(); ++index) {
      const auto& sample = samples(index);
      uint32_t sample_size = sample.byte_range.available ? sample.byte_range.size : sample.nal().count();
      CHECK(sample_size <= bytes_per_sound());
      indices.push_back(index);
      bytes_accumulated += sample_size;
      THROW_IF(bytes_accumulated > bytes_per_sound(), Unsupported);  // splitting bytes of a sample into multiple sounds not supported
      if (bytes_accumulated == bytes_per_sound()) {  // enough bytes accumulated, create a sound
        sound_to_sample_mapping.push_back(indices);
        bytes_accumulated = 0;
        indices.clear();
      }
    }
    if (bytes_accumulated) {
      sound_to_sample_mapping.push_back(indices);
    }
  }
};

PCM::PCM(const functional::Audio<Sample>& track)
  : functional::DirectAudio<PCM, sound::Sound>(), _this(new _PCM(track.settings())) {
  THROW_IF(!track(0).keyframe, InvalidArguments);
  THROW_IF(track.count() >= security::kMaxSampleCount, Unsafe);
  _settings = (settings::Audio) {
    settings::Audio::Codec::Unknown,
    track.settings().timescale,
    track.settings().sample_rate,
    track.settings().channels,
    0,
  };
  _this->init(track);
  set_bounds(0, (uint32_t)_this->sound_to_sample_mapping.size());
}

PCM::PCM(const PCM& pcm)
  : functional::DirectAudio<PCM, sound::Sound>(pcm.a(), pcm.b(), pcm.settings()), _this(pcm._this) {}

auto PCM::operator()(uint32_t index) const -> sound::Sound {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->samples.count(), OutOfRange);
  THROW_IF(index >= _this->sound_to_sample_mapping.size(), OutOfRange);
  auto sample_indices = _this->sound_to_sample_mapping[index];
  CHECK(sample_indices.size());
  sound::Sound sound;
  sound.pts = _this->samples(sample_indices[0]).pts;
  sound.pcm = [_this = _this, index, sample_indices]() -> sound::PCM {
    auto sound = [&]() -> common::Sample16 {
      if (sample_indices.size() == 1 && _this->settings.codec == settings::Audio::Codec::PCM_S16LE) {
        // optimization: avoid memory copy
        const Sample& sample = _this->samples(sample_indices[0]);
        if (sample.byte_range.size == _this->bytes_per_sound()) {
          const auto sample_data = sample.nal();
          return move(common::Sample16((int16_t*)sample_data.data(), sample_data.count() / _this->bytes_per_pcm_sample(), nullptr));
        }
      }
      common::Sample16 sound = common::Sample16(new int16_t[_this->sound_size()], _this->sound_size(), [](int16_t* ptr){ delete[] ptr; });
      for (auto sample_index: sample_indices) {
        const Sample& sample = _this->samples(sample_index);
        common::Data32 data = sample.nal();
        _this->copy_data_to_sound(data, sound);
        sound.set_bounds(sound.a() + data.count() / _this->bytes_per_pcm_sample(), sound.capacity());
      }
      if (sound.count()) {  // fill remaining bytes with silence
        memset((void*)(sound.data() + sound.a()), 0, sound.count() * sizeof(int16_t));
      }
      sound.set_bounds(0, sound.capacity());
      return move(sound);
    }();
    sound::PCM pcm(sound.count() / _this->settings.channels, _this->settings.channels, move(sound));
    return move(pcm);
  };
  return sound;
}

}}}
