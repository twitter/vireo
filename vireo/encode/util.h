//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/util/util.h"
#include "vireo/decode/types.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace encode {

const static char* kVideoProfileTypeToString[] = { "constrained baseline", "baseline", "main", "high" };


template <class Lambda>
void order_samples(const uint32_t audio_timescale, const functional::Audio<Sample>& audio,
                   const uint32_t video_timescale, const functional::Video<Sample>& video, const Lambda& lambda) {
  uint32_t audio_sample_index = 0;
  vector<Sample> a_sample_cache;
  for (auto v_sample: video) {
    CHECK(v_sample.type == SampleType::Video);
    const auto v_dts = (float)v_sample.dts / video_timescale;

    while (audio_sample_index < audio.count()) {
      if (a_sample_cache.empty()) {
        a_sample_cache.push_back(audio(audio_sample_index));
        CHECK(a_sample_cache.back().type == SampleType::Audio);
      }
      const auto a_dts = (float)a_sample_cache.front().dts / audio_timescale;
      if (a_dts < v_dts) {
        lambda(a_sample_cache.front());
        a_sample_cache.pop_back();
        audio_sample_index++;
      } else {
        break;
      }
    }
    lambda(v_sample);
  }
  while (audio_sample_index < audio.count()) {
    if (!a_sample_cache.empty()) {
      lambda(a_sample_cache.front());
      a_sample_cache.pop_back();
    } else {
      lambda(audio(audio_sample_index));
    }
    audio_sample_index++;
  }
}

}}
