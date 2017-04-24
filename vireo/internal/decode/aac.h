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

class PUBLIC AAC final : public functional::DirectAudio<AAC, sound::Sound> {
  std::shared_ptr<struct _AAC> _this;
public:
  AAC(const functional::Audio<Sample>& track);
  AAC(const AAC& aac);
  DISALLOW_ASSIGN(AAC);
  auto operator()(uint32_t sample) const -> sound::Sound;
};

}}}
