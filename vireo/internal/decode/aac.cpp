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
#include "fdk-aac/FDK_audio.h"
#include "fdk-aac/aacdecoder_lib.h"
#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/aac.h"
#include "vireo/sound/pcm.h"
#include "vireo/util/util.h"

namespace vireo {
namespace internal{
namespace decode {

struct _AAC {
  // size of 0x10000 = 65536 is based on this comment in fdk-aac/libMpegTPDec/src/tpdec_lib.cpp
  /* For packet based transport, pass input buffer to bitbuffer without copying the data.
  Unfortunately we do not know the actual buffer size. And the FDK bit buffer implementation
  needs a number 2^x. So we assume the maximum of 48 channels with 6144 bits per channel
  and round it up to the next power of 2 => 65536 bytes
  FDKinitBitStream(hBs, pBuffer, 0x10000, (*pBytesValid)<<3, BS_READER); */

  // note that numeric_limits<uint16_t>::max(); is 65535
  constexpr static const uint32_t kMaxBufferSize = 0x10000; // 65536
  unique_ptr<AAC_DECODER_INSTANCE, function<void(AAC_DECODER_INSTANCE*)>> decoder = { nullptr, [](AAC_DECODER_INSTANCE* p) {
    if(p != nullptr) {
      aacDecoder_Close(p);
    }
  }};
  // This scratch buffer is used to stage the data before it gets passed to the decoder.
  // It is needed because fdk-aac internal code sometimes reads up to 4 extra bytes past the end of the bitstream.
  common::Data32 scratch_buffer = { (uint8_t*)calloc(kMaxBufferSize, sizeof(uint8_t)), kMaxBufferSize, [](uint8_t* p) { free(p); } };
  common::Sample16 decoded_sample = { (int16_t*)calloc(kMaxBufferSize, sizeof(uint16_t)), kMaxBufferSize, [](int16_t* p) { free(p); } };
  settings::Audio audio_settings;
  functional::Audio<Sample> samples;
  int64_t last_index = -1;

  _AAC(const settings::Audio& audio_settings)
    : audio_settings(audio_settings) {
    THROW_IF(audio_settings.channels != 1 && audio_settings.channels != 2, Unsupported);
    THROW_IF(audio_settings.codec != settings::Audio::Codec::AAC_LC &&
             audio_settings.codec != settings::Audio::Codec::AAC_LC_SBR, Unsupported);
    THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), audio_settings.sample_rate) == kSampleRate.end(), Unsupported);
  }
  void init() {
    decoder.reset(aacDecoder_Open(TT_MP4_RAW, 1));
    CHECK(decoder);

    const auto extradata = audio_settings.as_extradata(settings::Audio::ExtraDataType::aac);
    // Copy to a padded size buffer to prevent uninitialized reads.
    const uint16_t padded_size = extradata.count() + 4;
    common::Data16 padded_buffer = { (uint8_t*)calloc(padded_size, sizeof(uint8_t)), padded_size, [](uint8_t* p) { free(p); } };
    padded_buffer.copy(extradata);
    const UINT size = extradata.count();
    CHECK(aacDecoder_ConfigRaw(decoder.get(), (UCHAR**)util::get_addr(padded_buffer.data()), &size) == AAC_DEC_OK);
    CHECK(aacDecoder_SetParam(decoder.get(), AAC_CONCEAL_METHOD, 1) == AAC_DEC_OK);
  }
  void reset() {
    last_index = -1;
    init();
  }
};

AAC::AAC(const functional::Audio<Sample>& track)
  : functional::DirectAudio<AAC, sound::Sound>(track.a(), track.b()), _this(new _AAC(track.settings())) {
  THROW_IF(track.count() >= security::kMaxSampleCount, Unsafe);
  _settings = (settings::Audio) {
    settings::Audio::Codec::Unknown,
    track.settings().timescale,
    track.settings().sample_rate,
    track.settings().channels,  // TODO: Fix channels to take SBR into account
    0,
  };
  _this->init();
  _this->samples = track;
}

AAC::AAC(const AAC& aac)
  : functional::DirectAudio<AAC, sound::Sound>(aac.a(), aac.b(), aac.settings()), _this(aac._this) {
}

auto AAC::operator()(uint32_t index) const -> sound::Sound {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->samples.count(), OutOfRange);

  const Sample& sample = _this->samples(index);
  sound::Sound sound;
  sound.pts = sample.pts;

  struct audio_info {
    uint16_t frame_size;
    uint8_t channels;
  };

  sound.pcm = [_this = _this, index]() -> sound::PCM {
    auto decode_sample = [&_this](uint32_t index) -> audio_info {
      const Sample& sample = _this->samples(index);

      const auto sample_data = sample.nal();

      THROW_IF(sample_data.count() + 4 > _AAC::kMaxBufferSize, Unsafe);
      _this->scratch_buffer.copy(sample_data);

      const UINT size = _this->scratch_buffer.count();
      UINT valid_bytes_left = size;
      CHECK(aacDecoder_Fill(_this->decoder.get(),
                            (UCHAR**)util::get_addr(_this->scratch_buffer.data()),
                            &size,
                            &valid_bytes_left) == AAC_DEC_OK);
      CHECK(valid_bytes_left == 0);
      THROW_IF(aacDecoder_DecodeFrame(_this->decoder.get(),
                                      (INT_PCM*)_this->decoded_sample.data(),
                                      _this->decoded_sample.capacity(), 0) != AAC_DEC_OK, Invalid);
      CStreamInfo* stream_info = aacDecoder_GetStreamInfo(_this->decoder.get());

      const uint8_t audio_object_type = stream_info->aot;
      const uint8_t extension_object_type = stream_info->extAot;
      const uint16_t frame_size = stream_info->frameSize;
      const uint8_t channels = stream_info->numChannels;
      const uint32_t sample_rate = stream_info->sampleRate;

      // MPEG-4 AAC Low Complexity only with optional SBR supported
      THROW_IF(audio_object_type != 2, Unsupported);
      if (extension_object_type != 5) {  // AAC-LC
        CHECK(frame_size == AUDIO_FRAME_SIZE);
        CHECK(channels == _this->audio_settings.channels);
        CHECK(sample_rate == _this->audio_settings.sample_rate);
      } else {  // AAC-LC SBR
        CHECK(frame_size == AUDIO_FRAME_SIZE * SBR_FACTOR);
//        CHECK(channels == _this->audio_settings.channels * SBR_FACTOR);
        CHECK(sample_rate == _this->audio_settings.sample_rate * SBR_FACTOR);
      }

      const uint16_t decoded_size = frame_size * channels;
      _this->decoded_sample.set_bounds(0, decoded_size);
      return {frame_size, channels};
    };

    if (index - _this->last_index != 1) {
      _this->reset();
      if (index > 0) {
        decode_sample(index - 1); // There is 1 sample dependency between samples
      }
    }
    const auto info = decode_sample(index);
    common::Sample16 decoded_sample_copy(_this->decoded_sample);
    _this->last_index = index;
    sound::PCM pcm(info.frame_size, info.channels, move(decoded_sample_copy));
    if (pcm.size() == AUDIO_FRAME_SIZE * SBR_FACTOR) {
      return pcm.downsample(2);
    } else {
      return move(pcm);
    }
  };
  return sound;
}

}}}
