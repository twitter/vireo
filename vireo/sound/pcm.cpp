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
#include "vireo/common/ref.h"
#include "vireo/constants.h"
#include "vireo/error/error.h"
#include "vireo/sound/pcm.h"

namespace vireo {
namespace sound {

struct _PCM : common::Ref<_PCM> {
  uint16_t size;
  uint8_t channels;
  common::Sample16 samples;
  _PCM(common::Sample16&& samples)
    : samples(move(samples)) {}
};

PCM::PCM(uint16_t size, uint8_t channels, common::Sample16&& samples) {
  _this = new _PCM(move(samples));
  _this->size = size;
  _this->channels = channels;
}

PCM::PCM(PCM&& sound)
  : PCM{sound.size(), sound.channels(), move(sound._this->samples)} {
}

PCM::PCM(const PCM& sound) {
  _this = sound._this->ref();
}

PCM::~PCM() {
  _this->unref();
}

auto PCM::size() const -> uint16_t {
  return _this->size;
}

auto PCM::channels() const -> uint8_t {
  return _this->channels;
}

auto PCM::samples() const -> common::Sample16& {
  return _this->samples;
}

auto PCM::mix(uint8_t channels) const -> PCM {
  THROW_IF(channels != 1 || this->channels() != 2, InvalidArguments);
  common::Sample16 result((const int16_t*)calloc(_this->size, sizeof(int16_t)), _this->size, [](int16_t* p) {
    free((void*)p);
  });
  int16_t* dst = (int16_t*)result.data();
  const int16_t* src = (const int16_t*)_this->samples.data();
  const int32_t max = numeric_limits<int16_t>::max();
  const int32_t min = numeric_limits<int16_t>::min();
  for (uint16_t i = 0; i < _this->size; ++i) {
    *dst++ = std::max(std::min((int32_t)src[0] + src[1], max), min);
    src += 2;
  }
  return PCM(_this->size, channels, move(result));
}

auto PCM::downsample(uint8_t factor) const -> PCM {
  THROW_IF(SBR_FACTOR != 2, Unsupported);
  THROW_IF(factor != SBR_FACTOR, InvalidArguments);
  THROW_IF(_this->size % factor != 0, Invalid);
  common::Sample16 result((const int16_t*)calloc(_this->size * _this->channels / factor, sizeof(int16_t)), _this->size * _this->channels / factor, [](int16_t* p) {
    free((void*)p);
  });
  int16_t* dst = (int16_t*)result.data();
  const int16_t* src = (const int16_t*)_this->samples.data();
  const int32_t max = numeric_limits<int16_t>::max();
  const int32_t min = numeric_limits<int16_t>::min();
  for (uint16_t i = 0; i < _this->size / SBR_FACTOR; ++i) {
    for (uint8_t c = 0; c < _this->channels; ++c) {
      *dst++ = std::max(std::min(((int32_t)src[c] + src[c + _this->channels]) / SBR_FACTOR, max), min);
    }
    src += SBR_FACTOR * _this->channels;
  }
  return PCM(_this->size / factor, _this->channels, move(result));
}

}}
