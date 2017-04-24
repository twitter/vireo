//  Copyright (c) 2015 Twitter, Inc. All rights reserved.

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/decode/audio.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/aac.h"
#include "vireo/internal/decode/pcm.h"
#include "vireo/sound/sound.h"
#include "vireo/util/util.h"

namespace vireo {
namespace decode {

struct _Audio {
  functional::Audio<sound::Sound> track;
};

Audio::Audio(const functional::Audio<Sample>& track) : functional::DirectAudio<Audio, sound::Sound>(), _this(new _Audio) {
  const auto& settings = track.settings();
  THROW_IF(!settings::Audio::IsAAC(settings.codec) && !settings::Audio::IsPCM(settings.codec), Unsupported);
  if (settings::Audio::IsAAC(settings.codec)) {
    _this->track = functional::Audio<sound::Sound>(internal::decode::AAC(move(track)));
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
