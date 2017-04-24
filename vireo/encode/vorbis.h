//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"
#include "vireo/sound/sound.h"

namespace vireo {
namespace encode {

class PUBLIC Vorbis final : public functional::DirectAudio<Vorbis, Sample> {
  std::shared_ptr<struct _Vorbis> _this;
public:
  Vorbis(const functional::Audio<sound::Sound>& sounds, uint8_t channels, uint32_t bitrate);
  Vorbis(const Vorbis& vorbis);
  DISALLOW_ASSIGN(Vorbis);
  auto operator()(uint32_t index) const -> Sample;
};

}}
