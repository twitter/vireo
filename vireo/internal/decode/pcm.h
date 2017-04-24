//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"
#include "vireo/sound/sound.h"

namespace vireo {
namespace internal {
namespace decode {

using namespace vireo::decode;

class PUBLIC PCM final : public functional::DirectAudio<PCM, sound::Sound> {
  std::shared_ptr<struct _PCM> _this;
public:
  PCM(const functional::Audio<Sample>& track);
  PCM(const PCM& pcm);
  DISALLOW_ASSIGN(PCM);
  auto operator()(uint32_t index) const -> sound::Sound;
};

}}}
