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

extern "C" {
#include "libavformat/avformat.h"
}
#include "fdk-aac/aacenc_lib.h"
#include "fdk-aac/FDK_audio.h"
#include "vireo/base_cpp.h"
#include "vireo/common/ref.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/aac.h"
#include "vireo/error/error.h"
#include "vireo/util/util.h"

namespace vireo {
namespace encode {

struct _AAC {
  constexpr static const int32_t kMaxBufferSize = numeric_limits<uint16_t>::max();
  unique_ptr<AACENCODER, function<void(AACENCODER*)>> aacEncoder = { nullptr, [](AACENCODER* p) {
    if(p != nullptr) {
      aacEncClose(&p);
    }
  }};
  common::Data32 encoded_buffer = { (uint8_t*)calloc(kMaxBufferSize, sizeof(uint8_t)), kMaxBufferSize, [](uint8_t* p) { free(p); } };
  functional::Audio<sound::Sound> sounds;
  _AAC(uint32_t timescale, uint32_t sample_rate, uint8_t channels, uint32_t bitrate) {
    AACENCODER* encoder;
    THROW_IF(aacEncOpen(&encoder, 0, channels) != AACENC_OK, InvalidArguments);
    aacEncoder.reset(encoder);
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_SAMPLERATE, sample_rate) != AACENC_OK, InvalidArguments);
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_BITRATE, bitrate) != AACENC_OK, InvalidArguments);
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_CHANNELMODE, channels) != AACENC_OK, InvalidArguments);
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_CHANNELORDER, 1) != AACENC_OK, InvalidArguments);  // 1: WAVE file format channel ordering
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_TRANSMUX, 0) != AACENC_OK, InvalidArguments);  // 0: raw access units
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_SIGNALING_MODE, 2) != AACENC_OK, InvalidArguments); // 2: Explicit hierarchical signaling (default for MPEG-4 based AOT's and for all transport formats excluding ADIF and ADTS)
    THROW_IF(aacEncoder_SetParam(aacEncoder.get(), AACENC_AOT, 2) != AACENC_OK, InvalidArguments);  // MPEG-4 AAC Low Complexity
    THROW_IF(aacEncEncode(aacEncoder.get(), nullptr, nullptr, nullptr, nullptr) != AACENC_OK, InvalidArguments);
  }
};

AAC::AAC(const functional::Audio<sound::Sound>& sounds, uint8_t channels, uint32_t bitrate)
  : functional::DirectAudio<AAC, Sample>(sounds.a(), sounds.b()), _this(new _AAC(sounds.settings().timescale, sounds.settings().sample_rate, channels, bitrate)) {
  THROW_IF(sounds.count() >= security::kMaxSampleCount, Unsafe);
  THROW_IF(channels != 1 && channels != 2, Unsupported);
  THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), sounds.settings().sample_rate) == kSampleRate.end(), InvalidArguments);
  _this->sounds = sounds;
  _settings = sounds.settings();
  _settings.codec = settings::Audio::Codec::AAC_LC;
  _settings.channels = channels;
  _settings.bitrate = bitrate;
}

AAC::AAC(const AAC& aac)
  : functional::DirectAudio<AAC, Sample>(aac.a(), aac.b(), aac.settings()), _this(aac._this) {
}

auto AAC::operator()(uint32_t index) const -> Sample {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->sounds.count(), OutOfRange);

  const sound::Sound& sound = _this->sounds(index);

  auto pcm = [&]() {
    const auto pcm = sound.pcm();  // 'const' so don't move it
    CHECK(pcm.channels() == 1 || pcm.channels() == 2);
    THROW_IF(pcm.channels() < _settings.channels, Unsupported);
    if (pcm.channels() != _settings.channels) {  // Mismatch between MP4 and actual samples.
      return pcm.mix(1);
    } else {
      return pcm;
    }
  }();

  CHECK(pcm.channels() == _settings.channels);
  THROW_IF(pcm.size() != AUDIO_FRAME_SIZE, Unsupported);

  const auto& buffer = pcm.samples();
  CHECK(buffer.count() == _settings.channels * pcm.size());

  AACENC_BufDesc in_buffer_desc = { 0 };
  in_buffer_desc.numBufs = 1;
  in_buffer_desc.bufs = (void**)util::get_addr(buffer.data());
  INT in_buffer_size = buffer.count() * sizeof(int16_t);
  in_buffer_desc.bufSizes = &in_buffer_size;
  INT in_element_size = sizeof(int16_t);
  in_buffer_desc.bufElSizes = &in_element_size;
  INT in_identifier = IN_AUDIO_DATA;
  in_buffer_desc.bufferIdentifiers = &in_identifier;

  AACENC_BufDesc out_buffer_desc = { 0 };
  out_buffer_desc.numBufs = 1;
  out_buffer_desc.bufs = (void**)util::get_addr(_this->encoded_buffer.data());
  INT out_buffer_size = _this->encoded_buffer.capacity();
  out_buffer_desc.bufSizes = &out_buffer_size;
  INT out_element_size = sizeof(uint8_t);
  out_buffer_desc.bufElSizes = &out_element_size;
  int32_t out_identifier = OUT_BITSTREAM_DATA;
  out_buffer_desc.bufferIdentifiers = &out_identifier;

  AACENC_InArgs in_args = { 0 };
  in_args.numInSamples = buffer.count();
  AACENC_OutArgs out_args = { 0 };
  CHECK(aacEncEncode(_this->aacEncoder.get(), &in_buffer_desc, &out_buffer_desc, &in_args, &out_args) == AACENC_OK);

  _this->encoded_buffer.set_bounds(0, out_args.numOutBytes);
  return Sample(sound.pts, sound.pts, true, vireo::SampleType::Audio, _this->encoded_buffer);
}

}}
