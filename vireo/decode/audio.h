//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"
#include "vireo/sound/sound.h"

namespace vireo {
namespace decode {

class PUBLIC Audio final : public functional::DirectAudio<Audio, sound::Sound> {
  std::shared_ptr<struct _Audio> _this;
public:
  Audio(const functional::Audio<Sample>& track);
  Audio(const Audio& audio);
  DISALLOW_ASSIGN(Audio);
  auto operator()(uint32_t index) const -> sound::Sound;
};

}}
