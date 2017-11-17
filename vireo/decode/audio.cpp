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
#include "vireo/decode/audio.h"
#include "vireo/dependency.hpp"
#include "vireo/error/error.h"
#include "vireo/internal/decode/aac.h"
#include "vireo/internal/decode/pcm.h"
#include "vireo/sound/sound.h"
#include "vireo/util/util.h"

namespace vireo {
namespace decode {

struct _Audio {
  functional::Audio<sound::Sound> track;

  template<bool is_aac, typename std::enable_if<is_aac && has_aac_decoder::value>::type* = nullptr>
  void process(const functional::Audio<Sample>& audio_track) {
    track = internal::decode::AAC(move(audio_track));
  }
  template<bool is_aac, typename std::enable_if<is_aac && !has_aac_decoder::value>::type* = nullptr>
  void process(const functional::Audio<Sample>& audio_track) {
    THROW_IF(true, MissingDependency);
  }
};

Audio::Audio(const functional::Audio<Sample>& track) : functional::DirectAudio<Audio, sound::Sound>(), _this(new _Audio) {
  const auto& settings = track.settings();
  THROW_IF(!settings::Audio::IsAAC(settings.codec) && !settings::Audio::IsPCM(settings.codec), Unsupported);
  if (settings::Audio::IsAAC(settings.codec)) {
    _this->process<true>(track);
  } else {
    _this->track = functional::Audio<sound::Sound>(internal::decode::PCM(move(track)));
  }
  _settings = _this->track.settings();
  set_bounds(_this->track.a(), _this->track.b());
}

Audio::Audio(const Audio& audio)
  : functional::DirectAudio<Audio, sound::Sound>(audio.a(), audio.b(), audio.settings()), _this(audio._this) {}

auto Audio::operator()(uint32_t index) const -> sound::Sound {
  return _this->track(index);
}

}}
