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

#include "imagecore/imagecore.h"
#include "imagecore/formats/reader.h"
#include "imagecore/image/image.h"
#undef LOCAL
#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/frame/util.h"
#include "vireo/internal/decode/image.h"

namespace vireo {
namespace internal {
namespace decode {

// needed by multi-frame images
const static uint32_t kImageMinDelayMs = 20;
const static uint32_t kImageMaxDelayMs = 5000;
const static uint32_t kImageTimeScaleMs = 1000;
const static uint32_t kImageLastFrameDurationMs = 1;
const static uint32_t kImageDefaultDelayMs = 100;

struct _Image {
  unique_ptr<frame::RGB> rgb;
  unique_ptr<frame::YUV> yuv;
  unique_ptr<imagecore::Image> decoded_frame;
  bool is_yuv; // note: assuming only yuv or rgb is possible
  unique_ptr<imagecore::ImageReader> reader;
  unique_ptr<imagecore::ImageReader::MemoryStorage> storage;
  functional::Video<Sample> track;
  common::Data32 data_in;

  // needed by multi-frame images
  int64_t last_decoded_index = -1;
  uint64_t last_frames_pts[2]; // pts values for last-1 and last frames
  uint32_t output_num_frames;

  _Image(const functional::Video<Sample>& track) : track(track) {
    CHECK(track.count() && track(0).keyframe);
    data_in = track(0).nal();
    storage.reset(new imagecore::ImageReader::MemoryStorage((void*)data_in.data(), (uint64_t)data_in.capacity()));
    reader.reset(imagecore::ImageReader::create(storage.get()));
    CHECK(reader.get());
    const auto& settings = track.settings();
    THROW_IF(settings.width != reader->getOrientedWidth(), Invalid);
    THROW_IF(settings.height != reader->getOrientedHeight(), Invalid);
    THROW_IF(settings.timescale != kImageTimeScaleMs, Unsupported);
    THROW_IF(settings.orientation != settings::Video::Orientation::Landscape, Invalid);

    uint32_t stored_width = reader->getWidth();
    uint32_t stored_height = reader->getHeight();
    uint32_t input_num_frames = reader->getNumFrames();
    CHECK(input_num_frames == track.count());
    bool is_multi_frame = input_num_frames > 1;
    if (is_multi_frame) {
      uint32_t delay_ms = 0;
      uint32_t index = 0;
      for (index = 0; index < input_num_frames - 1; ++index) {
        CHECK(reader->advanceFrame());
      }
      delay_ms = reader->getFrameDelayMs();
      if (delay_ms < kImageMinDelayMs) {
        delay_ms = kImageDefaultDelayMs;
      }
      delay_ms = std::min(delay_ms, kImageMaxDelayMs);
      // Repeat last frame twice with kImageLastFrameDurationMs duration, so substract 2 * kImageLastFrameDurationMs here.
      // We repeat the last frame as a workaround for players that don't respect the specified duration of the last frame,
      // and end the video as soon as it's presented.
      delay_ms -= 2 * kImageLastFrameDurationMs;
      last_frames_pts[0] = track(track.b() - 1).pts + delay_ms;
      last_frames_pts[1] = last_frames_pts[0] + kImageLastFrameDurationMs;
      CHECK(reader->seekToFirstFrame());
    }
    output_num_frames = (is_multi_frame ? input_num_frames + 2 : input_num_frames); // +2 to repeat the last frame twice for multi-frame images

    is_yuv = reader->getNativeColorModel() == imagecore::kColorModel_YUV_420;
    if (is_yuv) {
      yuv.reset(new frame::YUV(stored_width, stored_height, 2, 2));
      decoded_frame.reset((imagecore::Image*)as_imagecore(*yuv));
    } else {
      CHECK(reader->getNativeColorModel() == imagecore::kColorModel_RGBA || reader->getNativeColorModel() == imagecore::kColorModel_RGBX);
      rgb.reset(new frame::RGB(stored_width, stored_height, 4));
      decoded_frame.reset((imagecore::Image*)as_imagecore(*rgb));
    }
  }

  auto decode_frame(uint32_t index) -> void {
    auto decode_frames = [&](uint32_t start, uint32_t end) -> void {
      for (uint32_t i = start; i <= end; i++) {
        reader->readImage(decoded_frame.get());
        if (end < track.count() - 1) {
          // Last frame is repeated. Do not advanceFrame for the last frame.
          CHECK(reader->advanceFrame());
        }
      }
      last_decoded_index = index;
    };

    int64_t idx = (int64_t)index;
    if (idx > last_decoded_index) {
      CHECK(last_decoded_index >= -1);
      decode_frames((uint32_t)(last_decoded_index + 1), index);
    } else if (idx < last_decoded_index) {
      CHECK(reader->seekToFirstFrame());
      decode_frames(0, index);
    }
  };
};

Image::Image(const functional::Video<Sample>& track) {
  const auto& settings = track.settings();
  THROW_IF(settings.codec != settings::Video::Codec::JPG &&
           settings.codec != settings::Video::Codec::PNG &&
           settings.codec != settings::Video::Codec::GIF &&
           settings.codec != settings::Video::Codec::BMP &&
           settings.codec != settings::Video::Codec::WebP &&
           settings.codec != settings::Video::Codec::TIFF, Unsupported);
  if (settings.width || settings.height) {
    THROW_IF(!security::valid_dimensions(settings.width, settings.height), Unsafe);
  }
  _this = make_shared<_Image>(track);
  _settings = settings;
  set_bounds(0, _this->output_num_frames);
}

Image::Image(const Image& image)
  : functional::DirectVideo<Image, frame::Frame>(image.a(), image.b(), image.settings()), _this(image._this) {}

auto Image::operator()(uint32_t index) const -> frame::Frame {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  CHECK(_this);

  frame::Frame frame;
  if (index < _this->track.count()) {
    frame.pts = _this->track(index).pts;
  } else {
    uint32_t last_frames_index = index - _this->track.count();
    frame.pts = _this->last_frames_pts[last_frames_index];
  }

  auto direction = [](imagecore::EImageOrientation orientation) -> frame::Rotation {
    switch (orientation) {
      case imagecore::kImageOrientation_Down:
        return frame::Rotation::Down;
      case imagecore::kImageOrientation_Left:
        return frame::Rotation::Right;
      case imagecore::kImageOrientation_Right:
        return frame::Rotation::Left;
      default:
        CHECK(0);
    }
  };

  frame.yuv = [_this = _this, index, direction]() -> frame::YUV {
    CHECK(_this);
    _this->decode_frame(index);
    CHECK(_this->reader);
    auto orientation = _this->reader->getOrientation();
    bool rotate = orientation != imagecore::kImageOrientation_Up;
    if (_this->is_yuv) {
      CHECK(_this->yuv);
      return rotate ? (*_this->yuv).rotate(direction(orientation)) : *_this->yuv;
    } else {
      CHECK(_this->rgb);
      return rotate ? _this->rgb->yuv(2, 2).rotate(direction(orientation)) : _this->rgb->yuv(2, 2);
    }
  };

  frame.rgb = [_this = _this, index, direction]() -> frame::RGB {
    CHECK(_this);
    _this->decode_frame(index);
    CHECK(_this->reader);
    auto orientation = _this->reader->getOrientation();
    bool rotate = orientation != imagecore::kImageOrientation_Up;
    if (_this->is_yuv) {
      CHECK(_this->yuv);
      return rotate ? _this->yuv->rgb(4).rotate(direction(orientation)) : _this->yuv->rgb(4);
    } else {
      CHECK(_this->rgb);
      return rotate ? (*_this->rgb).rotate(direction(orientation)) : *_this->rgb;
    }
  };

  return move(frame);
}

}}}
