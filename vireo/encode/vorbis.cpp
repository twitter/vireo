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

#include "vorbis/vorbisenc.h"
#include "vireo/base_cpp.h"
#include "vireo/common/ref.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/vorbis.h"
#include "vireo/error/error.h"
#include "vireo/sound/sound.h"
#include "vireo/util/util.h"

namespace vireo {
namespace encode {

struct MaxBitrate {
  uint32_t min_sample_rate;
  uint32_t mono;  // 0: invalid
  uint32_t stereo;  // 0: invalid
};
static const std::vector<MaxBitrate> kMaxBitrates = {  // matched from libvorbis
  {  8000,  42000,  32000 },  // setup_8.h : rate_mapping_8_uncoupled, rate_mapping_8
  {  9000,  50000,  44000 },  // setup_11.h : rate_mapping_11_uncoupled, rate_mapping_11
  { 15000, 100000,  86000 },  // setup_16.h : rate_mapping_16_uncoupled, rate_mapping_16
  { 19000,  90000,  86000 },  // setup_22.h : rate_mapping_22_uncoupled, rate_mapping_22
  { 26000, 190000, 190000 },  // setup_32.h : rate_mapping_32_uncoupled, rate_mapping_32
  { 40000, 240000, 250000 },  // setup_44u.h : rate_mapping_44_un ; setup_44.h : rate_mapping_44_stereo
};

struct _Vorbis {
  constexpr static const int32_t kMaxBufferSize = numeric_limits<uint16_t>::max();

  vorbis_info settings;
  vorbis_comment comment;
  vorbis_dsp_state dsp_state;
  vorbis_block block;

  functional::Audio<sound::Sound> sounds;
  uint32_t sample_rate;
  uint8_t channels;
  uint32_t bitrate;
  queue<Sample> samples;
  int64_t last_sample = -1;
  int64_t last_pcm  = -1;

  _Vorbis(const uint32_t sample_rate, const uint8_t channels, const uint32_t bitrate)
    : sample_rate(sample_rate), channels(channels), bitrate(bitrate) {
    init();
  }
  ~_Vorbis() {
    deinit();
  }
  void init() {
    vorbis_info_init(&settings);
    THROW_IF(vorbis_encode_setup_managed(&settings, channels, sample_rate, -1, bitrate, -1) != 0, Invalid);
    THROW_IF(vorbis_encode_ctl(&settings, OV_ECTL_RATEMANAGE2_SET, NULL) != 0, Invalid);
    THROW_IF(vorbis_encode_setup_init(&settings) != 0, Invalid);
    THROW_IF(vorbis_analysis_init(&dsp_state, &settings) != 0, Invalid);
    vorbis_comment_init(&comment);
    vorbis_block_init(&dsp_state, &block);
  }
  void deinit() {
    vorbis_block_clear(&block);
    vorbis_dsp_clear(&dsp_state);
    vorbis_comment_clear(&comment);
    vorbis_info_clear(&settings);
  }
  void reset() {
    last_sample = -1;
    last_pcm = -1;
    deinit();
    init();
  }
  auto encode_pcm(uint32_t pcm_index, queue<Sample>& encoded_samples) -> void {  // pass encoded_samples as input to avoid deep copy of Data objects
    THROW_IF(pcm_index >= sounds.count(), OutOfRange);
    THROW_IF(pcm_index - last_pcm != 1, Unsupported);

    const sound::Sound& sound = sounds(pcm_index);

    auto pcm = [&]() {
      const auto pcm = sound.pcm();  // 'const' so don't move it
      CHECK((pcm.channels() == 1 || pcm.channels() == 2) && pcm.channels() >= channels);
      if (pcm.channels() != channels) {  // Mismatch between MP4 and actual samples.
        return pcm.mix(1);
      } else {
        return pcm;
      }
    }();

    CHECK(pcm.channels() == channels);
    CHECK(pcm.size() == AUDIO_FRAME_SIZE || pcm.size() == AUDIO_FRAME_SIZE * 2);
    CHECK(pcm.samples().count() == channels * pcm.size());

    const int16_t* data = pcm.samples().data();
    const float max = (float)numeric_limits<int16_t>::max();
    float **pcm_buffer = vorbis_analysis_buffer(&dsp_state, pcm.samples().count());
    THROW_IF(!pcm_buffer, OutOfMemory);
    for (auto i = 0; i < pcm.size(); ++i) {
      for (auto c = 0; c < pcm.channels(); ++c) {
        pcm_buffer[c][i] = (float)data[c] / (max + 1);
      }
      data += pcm.channels();
    }
    CHECK(vorbis_analysis_wrote(&dsp_state, pcm.size()) == 0);

    ogg_packet packet;
    int ret = 0;
    while ((ret = vorbis_analysis_blockout(&dsp_state, &block)) == 1) {
      THROW_IF(vorbis_analysis(&block, NULL) < 0, Invalid);
      THROW_IF(vorbis_bitrate_addblock(&block) < 0, Invalid);

      int ret_ = 0;
      while ((ret_ = vorbis_bitrate_flushpacket(&dsp_state, &packet)) == 1) {
        THROW_IF(packet.bytes == 0, Invalid);
        auto data = common::Data32(packet.packet, (uint32_t)packet.bytes, NULL);
        encoded_samples.push(Sample((uint64_t)packet.granulepos, (uint64_t)packet.granulepos, true, SampleType::Audio, data));
      }
      THROW_IF(ret_ < 0, Invalid);
    }
    THROW_IF(ret < 0, Invalid);
    last_pcm = pcm_index;
  }
};

Vorbis::Vorbis(const functional::Audio<sound::Sound>& sounds, uint8_t channels, uint32_t bitrate) {
  THROW_IF(sounds.count() >= security::kMaxSampleCount, Unsafe);
  THROW_IF(channels != 1 && channels != 2, InvalidArguments);
  THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), sounds.settings().sample_rate) == kSampleRate.end(), InvalidArguments);

  // Adjust bitrate if requested bitrate is larger than the allowed maximum
  for (uint32_t i = 0; i < kMaxBitrates.size(); ++i) {
    if (sounds.settings().sample_rate < kMaxBitrates[i].min_sample_rate) {
      THROW_IF(i == 0, Unsupported);
      uint32_t max_bitrate = (channels == 1) ? kMaxBitrates[i - 1].mono : kMaxBitrates[i - 1].stereo;
      bitrate = min(bitrate, max_bitrate);
      break;
    }
  }

  _this.reset(new _Vorbis(sounds.settings().sample_rate, channels, bitrate));
  _this->sounds = sounds;
  uint32_t index = 0;
  for (auto sound: _this->sounds) {
    queue<Sample> samples;
    _this->encode_pcm((uint32_t)(index++), samples);
    set_bounds(0, b() + (int)samples.size());
  }

  // Update settings
  _settings = _this->sounds.settings();
  _settings.codec = settings::Audio::Codec::Vorbis;
  _settings.channels = channels;
  _settings.bitrate = bitrate;
}

Vorbis::Vorbis(const Vorbis& vorbis)
  : functional::DirectAudio<Vorbis, Sample>(vorbis.a(), vorbis.b()), _this(vorbis._this) {
}

auto Vorbis::operator()(uint32_t index) const -> Sample {
  THROW_IF(index >= count(), OutOfRange);

  if (index == 0) {
    _this->reset();
  }
  THROW_IF(index - _this->last_sample != 1, InvalidArguments);

  while (_this->samples.empty()) {
    _this->encode_pcm((uint32_t)(_this->last_pcm + 1), _this->samples);
  }
  THROW_IF(_this->samples.empty(), Invalid);
  _this->last_sample = index;
  auto sample = _this->samples.front();
  _this->samples.pop();
  return sample;
}

}}
