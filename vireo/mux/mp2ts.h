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

namespace vireo {
namespace mux {

class PUBLIC MP2TS final : public functional::Function<common::Data32> {
  std::shared_ptr<struct _MP2TS> _this;
public:
  MP2TS(const functional::Video<encode::Sample>& video) : MP2TS(functional::Audio<encode::Sample>(), video, functional::Caption<encode::Sample>()) {};
  MP2TS(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video) : MP2TS(audio, video, functional::Caption<encode::Sample>()) {};
  MP2TS(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption);
  MP2TS(MP2TS&& mp2ts);
  DISALLOW_COPY_AND_ASSIGN(MP2TS);
};

}}
