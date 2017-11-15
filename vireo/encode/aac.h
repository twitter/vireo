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
