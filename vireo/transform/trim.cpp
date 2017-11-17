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

#include <algorithm>

#include "vireo/base_cpp.h"
#include "vireo/common/editbox.h"
#include "vireo/common/math.h"
#include "vireo/functional/media.hpp"
#include "vireo/transform/trim.h"

namespace vireo {
namespace transform {

struct _Trim {
  SampleType type;
  uint32_t timescale;
  uint64_t duration = 0;
  vector<decode::Sample> out_samples;
  vector<common::EditBox> out_edit_boxes;

  // Calculates the track offset from empty edit box and removes it
  static auto ExtractOffset(vector<common::EditBox>& edit_boxes) -> const uint64_t {
    THROW_IF(!common::EditBox::Valid(edit_boxes), Invalid);  // ensures that the empty edit box is the first (if exists)
    uint64_t offset = 0;
    if (edit_boxes.size()) {
      auto first_edit_box = edit_boxes[0];
      const int64_t start_pts = first_edit_box.start_pts;
      if (start_pts == EMPTY_EDIT_BOX) {
        offset = first_edit_box.duration_pts;
        edit_boxes.erase(edit_boxes.begin());  // remove the empty edit box
      }
    }
    return offset;
  };

  // Takes in original edit boxes and returns new edit boxes according to the trim boundaries
  static auto TrimEditBoxes(vector<common::EditBox> in_edit_boxes, SampleType type, uint64_t start_pts, uint64_t duration_pts) -> const vector<common::EditBox> {
    THROW_IF(!common::EditBox::Valid(in_edit_boxes), Invalid);
    vector<common::EditBox> out_edit_boxes;

    // handle the empty edit box
    uint64_t track_offset = ExtractOffset(in_edit_boxes);  // removes the empty edit box
    const uint64_t end_pts = start_pts + duration_pts;
    if (start_pts >= track_offset) {
      // trimmed track falls beyond the empty edit box
      start_pts -= track_offset;
      track_offset = 0;
    } else if (end_pts > track_offset) {
      // trimmed track contains portion of the empty edit box
      track_offset -= start_pts;
      start_pts = 0;
      duration_pts -= track_offset;
    } else {
      // trimmed track falls completely within empty edit box, empty vector signifies no track
      return out_edit_boxes;
    }

    // handle the rest of the edit boxes
    if (in_edit_boxes.empty()) {
      THROW_IF(start_pts > std::numeric_limits<int64_t>::max(), Overflow);
      out_edit_boxes.push_back({ (int64_t)start_pts, duration_pts, 1.0f, type });
    } else {
      uint64_t offset = start_pts;
      uint64_t remaining = duration_pts;
      for (auto edit_box: in_edit_boxes) {
        THROW_IF(edit_box.type != type, Invalid);
        const int64_t in_start_pts = edit_box.start_pts;
        THROW_IF(in_start_pts == EMPTY_EDIT_BOX, Invalid);  // should be handled prior to this
        const uint64_t in_duration_pts = edit_box.duration_pts;
        if (offset >= in_duration_pts) {
          // throw away edit boxes that are out of bounds and adjust start pts offset
          offset -= in_duration_pts;
        } else {
          // prune input edit box and save
          THROW_IF(in_start_pts + offset > std::numeric_limits<int64_t>::max(), Overflow);
          const int64_t out_start_pts = in_start_pts + offset;
          const uint64_t out_duration_pts = min(in_duration_pts - offset, remaining);
          out_edit_boxes.push_back({ out_start_pts, out_duration_pts, 1.0f, type });

          // adjust start pts and duration pts for remaining edit boxes
          offset = 0;
          remaining -= out_duration_pts;
          if (remaining == 0) {
            break;
          }
        }
      }
    }
    if (track_offset) {
      out_edit_boxes.insert(out_edit_boxes.begin(), { -1, track_offset, 1.0f, type });
    }
    return out_edit_boxes;
  }

  struct GOP {
    int64_t start_keyframe_index;
    int64_t start_index;
    int64_t end_index;
    bool    valid;

    // GOP(s) needed to play the samples between (start_pts, start_pts + duration_pts)
    GOP(const vector<decode::Sample>& samples, const uint64_t start_pts, const uint64_t duration_pts) {
      start_keyframe_index = -1;
      start_index = -1;
      end_index = -1;
      valid = false;

      uint64_t end_pts = start_pts + duration_pts;
      uint32_t index = 0;
      for (const auto& sample: samples) {
        if (sample.keyframe) {
          if (start_keyframe_index < 0) {
            // if no keyframe is found yet, mark this keyframe index
            THROW_IF(start_index != -1, Invalid);
            start_keyframe_index = index;
          } else if (start_index < 0 && sample.pts <= start_pts) {
            // if we find another keyframe closer to start_pts we update the keyframe index
            // do not update the keyframe index after we already found the first sample of the GOP (start_index >= 0)
            start_keyframe_index = index;
          }
        }
        if (start_index < 0 && sample.pts >= start_pts) {
          // found the first sample of the GOP, mark this index
          start_index = index;
        }
        if (sample.pts < end_pts) {
          // found another sample within the GOP, update end_index
          end_index = index;
        }
        ++index;
      }
      if (start_keyframe_index >= 0 && start_index >= start_keyframe_index && end_index >= start_index) {
        valid = true;
      }
    }
  };

  // Calculate the track duration from input samples
  auto CalculateDuration(const vector<decode::Sample>& samples) -> uint64_t {
    THROW_IF(!samples.size(), InvalidArguments);
    vector<uint64_t> dts_offsets;
    uint64_t duration = 0;
    uint64_t prev_dts = 0;
    bool first = true;
    for (const auto& sample: samples) {
      if (first) {
        first = false;
        prev_dts = sample.dts;
        continue;
      }
      THROW_IF(sample.dts < prev_dts, Invalid);
      uint64_t dts_offset = sample.dts - prev_dts;
      duration += dts_offset;
      dts_offsets.push_back(dts_offset);
      prev_dts = sample.dts;
    }
    duration += common::median(dts_offsets);
    THROW_IF(duration == 0, Invalid);
    return duration;
  }

  auto trim(const vector<decode::Sample>& in_samples, const vector<common::EditBox>& in_edit_boxes, uint64_t start_ms, uint64_t duration_ms) -> void {
    THROW_IF(!in_samples.size(), InvalidArguments);

    const uint64_t start_pts = (start_ms * timescale) / 1000;
    uint64_t duration_pts = (duration_ms * timescale + 999) / 1000;
    if (in_edit_boxes.empty()) {  // calculate input duration from samples: needed only if no common::EditBoxes, otherwise it is determined by input common::EditBoxes
      duration_pts = min(duration_pts, CalculateDuration(in_samples));
    }
    auto trimmed_edit_boxes = TrimEditBoxes(in_edit_boxes, type, start_pts, duration_pts);
    uint64_t min_start_pts = numeric_limits<uint64_t>::max();
    uint64_t max_end_pts = 0;
    for (const auto& edit_box: trimmed_edit_boxes) {
      THROW_IF(edit_box.type != type, Invalid);
      const int64_t start_pts = edit_box.start_pts;
      if (start_pts != EMPTY_EDIT_BOX) {
        const uint64_t end_pts = start_pts + edit_box.duration_pts;
        min_start_pts = min((uint64_t)start_pts, min_start_pts);
        max_end_pts = max(end_pts, max_end_pts);
      }
    }
    if (min_start_pts < max_end_pts) {
      auto gop = GOP(in_samples, min_start_pts, max_end_pts - min_start_pts);
      if (gop.valid) {  // found samples that fall within the trim boundaries
        const int64_t first_dts = in_samples[(int)gop.start_keyframe_index].dts;
        THROW_IF(first_dts > min_start_pts, Unsupported);
        for (auto edit_box: trimmed_edit_boxes) {
          out_edit_boxes.push_back({ (int64_t)(edit_box.start_pts - first_dts), edit_box.duration_pts, 1.0f, type });
        }
        for (uint32_t index = (uint32_t)gop.start_keyframe_index; index <= (uint32_t)gop.end_index; ++index) {
          auto& sample = in_samples[index];
          THROW_IF(sample.type != type, Invalid);
          out_samples.push_back((decode::Sample){ sample.pts - first_dts, sample.dts - first_dts, sample.keyframe, sample.type, sample.nal });
        }
        duration = CalculateDuration(out_samples);  // consistent with the l-smash behavior, duration is reported as the total duration of all samples, irrespective of the contained edit boxes
      }
    }
  }

  _Trim(const functional::Video<decode::Sample>& track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) {
    type = SampleType::Video;
    timescale = track.settings().timescale;
    if (track.count()) {
      vector<decode::Sample> samples;
      for (auto sample: track) {
        samples.push_back(sample);
      }
      trim(samples, edit_boxes, start_ms, duration_ms);
    } else {
      THROW_IF(track.settings() != settings::Video::None, InvalidArguments);
      THROW_IF(edit_boxes.size(), InvalidArguments);
    }
  }

  _Trim(const functional::Audio<decode::Sample>& track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) {
    type = SampleType::Audio;
    timescale = track.settings().timescale;
    if (track.count()) {
      vector<decode::Sample> samples;
      for (auto sample: track) {
        samples.push_back(sample);
      }
      trim(samples, edit_boxes, start_ms, duration_ms);
    } else {
      THROW_IF(track.settings() != settings::Audio::None, InvalidArguments);
      THROW_IF(edit_boxes.size(), InvalidArguments);
    }
  }

  _Trim(const functional::Caption<decode::Sample>& track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) {
    type = SampleType::Caption;
    timescale = track.settings().timescale;
    if (track.count()) {
      vector<decode::Sample> samples;
      for (auto sample: track) {
        samples.push_back(sample);
      }
      trim(samples, edit_boxes, start_ms, duration_ms);
    } else {
      THROW_IF(track.settings() != settings::Caption::None, InvalidArguments);
      THROW_IF(edit_boxes.size(), InvalidArguments);
    }
  }
};

template <int Type>
Trim<Type>::Track::Track(const std::shared_ptr<_Trim>& _trim_this)
  : _this(_trim_this) {
}

template <int Type>
auto Trim<Type>::Track::duration() const -> uint64_t {
  return _this->duration;
}

template <int Type> auto Trim<Type>::Track::edit_boxes() const -> const vector<common::EditBox>& {
  return _this->out_edit_boxes;
}

template <int Type>
auto Trim<Type>::Track::operator()(uint32_t index) const -> decode::Sample {
  THROW_IF(index < this->a() || index >= this->b(), OutOfRange);
  CHECK(index < _this->out_samples.size());
  return _this->out_samples[index];
}

template <int Type>
Trim<Type>::Trim(const Trim& trim) : _this(trim._this), track(_this) {}

template <>
Trim<SampleType::Video>::Trim(const functional::Video<decode::Sample>& in_track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) : _this(make_shared<_Trim>(in_track, edit_boxes, start_ms, duration_ms)), track(_this) {
  track._settings = _this->out_samples.size() ? in_track.settings() : settings::Video::None;
  track.set_bounds(0, (uint32_t)_this->out_samples.size());
}

template <>
Trim<SampleType::Video>::Track::Track(const Track& in_track)
  : functional::DirectVideo<Track, decode::Sample>(in_track.a(), in_track.b()), _this(in_track._this) {}

template Trim<SampleType::Video>::Track::Track(const std::shared_ptr<_Trim>& _trim_this);
template Trim<SampleType::Video>::Trim(const Trim& trim);
template auto Trim<SampleType::Video>::Track::operator()(uint32_t index) const -> decode::Sample;
template auto Trim<SampleType::Video>::Track::edit_boxes() const -> const vector<common::EditBox>&;
template auto Trim<SampleType::Video>::Track::duration() const -> uint64_t;

template <>
Trim<SampleType::Audio>::Trim(const functional::Audio<decode::Sample>& in_track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) :_this(make_shared<_Trim>(in_track, edit_boxes, start_ms, duration_ms)), track(_this) {
  track._settings = _this->out_samples.size() ? in_track.settings() : settings::Audio::None;
  track.set_bounds(0, (uint32_t)_this->out_samples.size());
}

template <>
Trim<SampleType::Audio>::Track::Track(const Track& in_track)
  : functional::DirectAudio<Track, decode::Sample>(in_track.a(), in_track.b()), _this(in_track._this) {}

template Trim<SampleType::Audio>::Track::Track(const std::shared_ptr<_Trim>& _trim_this);
template auto Trim<SampleType::Audio>::Track::duration() const -> uint64_t;
template auto Trim<SampleType::Audio>::Track::edit_boxes() const -> const vector<common::EditBox>&;
template auto Trim<SampleType::Audio>::Track::operator()(uint32_t index) const -> decode::Sample;
template Trim<SampleType::Audio>::Trim(const Trim& trim);

template <>
Trim<SampleType::Caption>::Trim(const functional::Caption<decode::Sample>& in_track, vector<common::EditBox> edit_boxes, const uint64_t start_ms, uint64_t duration_ms) : _this(make_shared<_Trim>(in_track, edit_boxes, start_ms, duration_ms)), track(_this) {
  track._settings = _this->out_samples.size() ? in_track.settings() : settings::Caption::None;
  track.set_bounds(0, (uint32_t)_this->out_samples.size());
}

template <>
Trim<SampleType::Caption>::Track::Track(const Track& in_track)
: functional::DirectCaption<Track, decode::Sample>(in_track.a(), in_track.b()), _this(in_track._this) {}

template Trim<SampleType::Caption>::Track::Track(const std::shared_ptr<_Trim>& _trim_this);
template Trim<SampleType::Caption>::Trim(const Trim& trim);
template auto Trim<SampleType::Caption>::Track::operator()(uint32_t index) const -> decode::Sample;
template auto Trim<SampleType::Caption>::Track::edit_boxes() const -> const vector<common::EditBox>&;
template auto Trim<SampleType::Caption>::Track::duration() const -> uint64_t;

}}
