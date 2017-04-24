//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <iomanip>
#include <fcntl.h>
#include <fstream>

#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/decode/video.h"
#include "vireo/settings/settings.h"
#include "vireo/util/util.h"

#define USE_STDIN 1

const static uint32_t kMaxFramesToScan = 10;

const static float kMaxAspectRatioTolerence = 0.01f;

const static uint8_t kMinWhiteColorY = 230;
const static uint8_t kWhiteColorTolerenceUV = 15;

const static float kWhiteOuterFrameRadius = 0.485f;  // everything outside this band is expected to be white color
const static float kCircleBorderRadius = 0.47f;  // between this and white outer frame we expect to see at least one non-white pixel

using namespace vireo;

void print_usage(const string name) {
  cout << "Usage: " << name << " < infile" << endl;
}

int false_with_message(const string message) {
  cout << "NOT: " << message << endl;
  return 1;
}

int report_spectacle() {
  cout << "SPECTACLE" << endl;
  return 0;
}

bool is_pixel_white(const frame::YUV& yuv, uint16_t x, uint16_t y) {
  const auto& y_plane = yuv.plane(frame::Y);
  const auto& u_plane = yuv.plane(frame::U);
  const auto& v_plane = yuv.plane(frame::V);

  uint16_t uv_x = x / yuv.uv_ratio().first;
  uint16_t uv_y = y / yuv.uv_ratio().second;

  if (y_plane(y)(x) < kMinWhiteColorY ||
      abs((int)u_plane(uv_y)(uv_x) - 128) >= kWhiteColorTolerenceUV ||
      abs((int)v_plane(uv_y)(uv_x) - 128) >= kWhiteColorTolerenceUV) {
    return false;
  } else {
    return true;
  }
};

float smallest_circle_radius_enclosing_non_white_pixels(const frame::YUV& yuv) {
  float width = yuv.width();
  float height = yuv.height();
  float min_dim = min(width, height);

  // scan from UL corner
  uint32_t ul = 0;
  for (ul = 0; ul < min_dim; ++ul) {
    if (!is_pixel_white(yuv, ul, ul)) {
      break;
    }
  }
  // scan from UR corner
  uint32_t ur = 0;
  for (ur = 0; ur < min_dim; ++ur) {
    if (!is_pixel_white(yuv, width - ur - 1, ur)) {
      break;
    }
  }
  // scan from BL corner
  uint32_t bl = 0;
  for (bl = 0; bl < min_dim; ++bl) {
    if (!is_pixel_white(yuv, bl, height - bl - 1)) {
      break;
    }
  }
  // scan from BR corner
  uint32_t br = 0;
  for (br = 0; br < min_dim; ++br) {
    if (!is_pixel_white(yuv, width - br - 1, height - br - 1)) {
      break;
    }
  }
  // return the radius of largest circle with a white outer frame
  uint32_t min_corner = min(ul, min(ur, min(bl, br)));
  float x = (width / 2) - min_corner;
  float y = (height / 2) - min_corner;
  return sqrtf((x * x) + (y * y)) / min_dim;
};

auto looks_like_a_spectacle_circle(const frame::YUV& yuv) -> bool {
  float width = yuv.width();
  float height = yuv.height();

  bool non_white_pixel_found_around_border = false;
  for (uint32_t x = 0; x < width; ++x) {
    for (uint32_t y = 0; y < height; ++y) {
      float xf = fabsf(x / width - 0.5f);
      float yf = fabsf(y / height - 0.5f);
      float radius = sqrtf((xf * xf) + (yf * yf));
      if (radius > kCircleBorderRadius && radius <= kWhiteOuterFrameRadius) {
        // check for at least one non-white pixel
        if (!non_white_pixel_found_around_border && !is_pixel_white(yuv, x, y)) {
          non_white_pixel_found_around_border = true;
        }
      } else if (radius > kWhiteOuterFrameRadius) {
        // check for all white pixels
        if (!is_pixel_white(yuv, x, y)) {
          return false;
        }
      }
    }
  }
  return non_white_pixel_found_around_border;
};

int main(int argc, const char* argv[]) {
  const string name = common::Path::Filename(argv[0]);
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0) {
      print_usage(name);
      return 0;
    } else {
#if (USE_STDIN)
      cerr << "Invalid argument: " << argv[i] << " (--help for usage)" << endl;
      return 1;
#endif
    }
  }

  __try {
    // Read stdin into buffer
    static const size_t kSize_Default = 512 * 1024;
    common::Data32 tmp_buffer(new uint8_t[65536], 65536, [](uint8_t* p) { delete[] p; });
    unique_ptr<common::Data32> buffer;
#if (USE_STDIN)
    int fd = fcntl(STDIN_FILENO, F_DUPFD, 0);
#else
    int fd = open(argv[1], O_RDONLY);
#endif
    for (uint16_t i = 0, max_i = 10000; i < max_i; ++i) {
      size_t read_size = read((int)fd, (void*)tmp_buffer.data(), (size_t)tmp_buffer.capacity());
      if (!read_size) {
        break;
      }
      tmp_buffer.set_bounds(0, (uint32_t)read_size);
      if (!buffer.get()) {
        buffer.reset(new common::Data32(new uint8_t[kSize_Default], kSize_Default, [](uint8_t* p) { delete[] p; }));
        buffer->set_bounds(0, 0);
      } else if (read_size + buffer->b() > buffer->capacity()) {
        uint32_t new_capacity = buffer->capacity() + ((buffer->b() + (uint32_t)read_size + kSize_Default - 1) / kSize_Default) * kSize_Default;
        auto new_buffer = new common::Data32(new uint8_t[new_capacity], new_capacity, [](uint8_t* p) { delete[] p; });
        new_buffer->copy(*buffer);
        buffer.reset(new_buffer);
      }
      CHECK(buffer->a() + tmp_buffer.count() <= buffer->capacity());
      buffer->set_bounds(buffer->b(), buffer->b());
      buffer->copy(tmp_buffer);
      buffer->set_bounds(0, buffer->b());
    };
    close(fd);
    THROW_IF(!buffer.get() || !buffer->count(), Invalid);

    // Demux video files
    demux::Movie movie(move(*buffer));

    // Spectacles should have 1:1 aspect ratio
    auto settings = movie.video_track.settings();
    float aspect_ratio = (float)settings.width / (float)settings.height;
    if (aspect_ratio < 1.0f - kMaxAspectRatioTolerence || aspect_ratio > 1.0f + kMaxAspectRatioTolerence) {
      return false_with_message("wrong aspect ratio");
    }

    // Check only Intra frames for fast performance
    decode::Video decoder(movie.video_track.filter([](decode::Sample& sample){ return sample.keyframe; }).filter_index([](uint32_t index){ return index < kMaxFramesToScan; }));

    for (const auto& frame: decoder) {
      const auto& yuv = frame.yuv();
      float radius = smallest_circle_radius_enclosing_non_white_pixels(yuv);
      if (radius < kCircleBorderRadius || radius > kWhiteOuterFrameRadius) {
        return false_with_message("radius of detected circle is out of expected values");
      }
      if (!looks_like_a_spectacle_circle(yuv)) {
        return false_with_message("pixels don't lie");
      }
      return report_spectacle();
    }
  } __catch (std::exception& e) {
    cout << "ERROR: " << string(e.what()) << endl;
    return 1;
  }
  return 0;
}
