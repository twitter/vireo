//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <vector>

#include "vireo/base_h.h"
#include "vireo/common/editbox.h"
#include "vireo/decode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace transform {

template <int Type>
class PUBLIC Trim final {
  std::shared_ptr<struct _Trim> _this = nullptr;
public:
  Trim(const functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type>& track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms);
  Trim(const functional::Media<functional::Function<decode::Sample, uint32_t>, decode::Sample, uint32_t, Type>& track, const uint64_t start_ms, uint64_t duration_ms) : Trim(track, vector<common::EditBox>(), start_ms, duration_ms) {}
  Trim(const Trim& trim);
  DISALLOW_ASSIGN(Trim<Type>);

  class Track final : public functional::Media<Track, decode::Sample, uint32_t, Type> {
    std::shared_ptr<_Trim> _this;
    Track(const std::shared_ptr<_Trim>& _this);
    friend class Trim;
  public:
    Track(const Track& track);
    DISALLOW_ASSIGN(Track);
    auto duration() const -> uint64_t;
    auto edit_boxes() const -> const vector<common::EditBox>&;
    auto operator()(uint32_t index) const -> decode::Sample;
  } track;
};

}}
