//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"
#include "vireo/header/header.h"
#include "vireo/settings/settings.h"
#include "vireo/sound/sound.h"

namespace vireo {
namespace encode {

class PUBLIC AAC final : public functional::DirectAudio<AAC, Sample> {
  std::shared_ptr<struct _AAC> _this;
public:
  AAC(const functional::Audio<sound::Sound>& sounds, uint8_t channels, uint32_t bitrate);
  AAC(const AAC& aac);
  DISALLOW_ASSIGN(AAC);
  auto operator()(uint32_t sample) const -> Sample;
};

}}
