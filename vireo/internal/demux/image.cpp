//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "imagecore/formats/reader.h"
#include "imagecore/imagecore.h"
#undef LOCAL
#include "vireo/common/reader.h"
#include "vireo/internal/demux/image.h"
#include "vireo/util/ftyp.h"

namespace vireo {
namespace internal {
namespace demux {

// needed by multi-frame images
const static uint32_t kImageMinDelayMs = 20;
const static uint32_t kImageMaxDelayMs = 5000;
const static uint32_t kImageDefaultDelayMs = 100;
const static uint32_t kImageTimeScaleMs = 1000;

const static uint32_t kSignatureMaxSize = 8;

struct ImageCoreStorage : public imagecore::ImageReader::Storage {
  uint32_t offset = 0;
  common::Reader reader;
  common::Data32 simple_cache = common::Data32();  // avoid requesting small chunks of data from Reader
  ImageCoreStorage(common::Reader&& reader) : reader(move(reader)) {}
  ~ImageCoreStorage() {}
  uint64_t read(void* destBuffer, uint64_t numBytes) {
    if (offset >= reader.size()) {
      return 0;
    }
    THROW_IF(numBytes > std::numeric_limits<uint32_t>::max(), Overflow);
    const uint32_t read_size = std::min((uint32_t)numBytes, reader.size() - offset);
    if (read_size) {
      const static uint32_t kMaxCacheSize = 1024;
      if (read_size < kMaxCacheSize) {
        if (read_size > simple_cache.count()) {
          simple_cache = move(reader.read(offset, min(kMaxCacheSize, reader.size() - offset)));
        }
        memcpy(destBuffer, simple_cache.data() + simple_cache.a(), read_size);
        simple_cache.set_bounds(simple_cache.a() + read_size, simple_cache.b());
      } else {
        auto data = reader.read(offset, read_size);
        CHECK(data.count() == read_size);
        memcpy(destBuffer, data.data(), read_size);
        simple_cache.set_bounds(0, 0);
      }
      offset += read_size;
    }
    return (int)read_size;
  };
  bool seek(int64_t pos, SeekMode mode) {
    if (mode == SeekMode::kSeek_Set) {
      CHECK(offset >= 0);
      offset = (uint32_t)pos;
    } else if (mode == SeekMode::kSeek_Current) {
      CHECK((int64_t)offset + pos >= 0);
      offset = (uint32_t)((int64_t)offset + pos);
    } else if (mode == SeekMode::kSeek_End) {
      CHECK((int64_t)reader.size() + pos >= 0);
      offset = (uint32_t)((int64_t)reader.size() + pos);
    }
    simple_cache.set_bounds(0, 0);
    return std::min(offset, reader.size());
  };
  uint64_t tell() {
    return offset;
  }
  bool canSeek() {
    return true;
  }
  bool peekSignature(uint8_t* signature) {
    if (reader.size() >= kSignatureMaxSize) {
      auto data = reader.read(0, kSignatureMaxSize);
      CHECK(data.count() == kSignatureMaxSize);
      memcpy(signature, data.data(), data.count());
      return true;
    }
    return false;
  };
  bool asFile(FILE*& file) {
    return false;
  };
  bool asBuffer(uint8_t*& buffer, uint64_t& length) {
    return false;
  }
};

struct _Image {
  ImageCoreStorage storage;
  uint16_t width;
  uint16_t height;
  uint32_t num_frames;
  uint64_t duration = 0;
  vector<uint64_t> pts;
  bool initialized = false;

  _Image(common::Reader&& reader) : storage(move(reader)) {}

  static auto FtypToCodec(common::Data32 data) -> settings::Video::Codec {
    if (util::FtypUtil::Matches(kJPGFtyp, data)) {
      return settings::Video::Codec::JPG;
    } else if (util::FtypUtil::Matches(kPNGFtyp, data)) {
      return settings::Video::Codec::PNG;
    } else if (util::FtypUtil::Matches(kGIFFtyp, data)) {
      return settings::Video::Codec::GIF;
    } else if (util::FtypUtil::Matches(kBMPFtyp, data)) {
      return settings::Video::Codec::BMP;
    } else if (util::FtypUtil::Matches(kWebPFtyp, data)) {
      return settings::Video::Codec::WebP;
    } else if (util::FtypUtil::Matches(kTIFFFtyps, data)) {
      return settings::Video::Codec::TIFF;
    }
    return settings::Video::Codec::Unknown;
  };

  bool finish_initialization() {
    unique_ptr<imagecore::ImageReader> reader;
    reader.reset(imagecore::ImageReader::create(&storage));
    CHECK(reader.get());

    width = reader->getOrientedWidth();
    height = reader->getOrientedHeight();
    num_frames = reader->getNumFrames();
    CHECK(num_frames >= 1);
    bool is_multi_frame = num_frames > 1;
    for (uint32_t index = 0; index < num_frames; ++index) {
      pts.push_back(duration);
      if (is_multi_frame) {
        uint32_t delay_ms = reader->getFrameDelayMs();
        if (delay_ms < kImageMinDelayMs) {
          delay_ms = kImageDefaultDelayMs;
        }
        delay_ms = std::min(delay_ms, kImageMaxDelayMs);
        duration += delay_ms;
        CHECK(reader->advanceFrame());
      }
    }
    if (is_multi_frame) {
      CHECK(reader->seekToFirstFrame());
    }
    initialized = true;
    return true;
  }
};

Image::Image(common::Reader&& reader) : _this(make_shared<_Image>(move(reader))), track(_this) {
  if (_this->finish_initialization()) {
    track.set_bounds(0, _this->num_frames);
    track._settings = (settings::Video) {
      _this->FtypToCodec(_this->storage.reader.read(0, kSignatureMaxSize)),
      _this->width,
      _this->height,
      kImageTimeScaleMs,
      settings::Video::Orientation::Landscape,
      header::SPS_PPS(common::Data16(), common::Data16(), 2)
    };
  } else {
    THROW_IF(true, Uninitialized);
  }
}

Image::Image(Image&& image) : track(_this) {
  CHECK(image._this);
  _this = image._this;
  image._this = nullptr;
}

Image::Track::Track(const std::shared_ptr<_Image>& _this) : _this(_this) {}

Image::Track::Track(const Track& track)
  : functional::DirectVideo<Track, Sample>(track.a(), track.b()), _this(track._this) {}

auto Image::Track::duration() const -> uint64_t {
  CHECK(_this);
  return _this->duration;
}

auto Image::Track::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0.0f;
}

auto Image::Track::operator()(uint32_t index) const -> Sample {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  CHECK(_this);
  uint64_t pts = _this->pts[index];
  uint64_t dts = _this->pts[index];
  SampleType type = SampleType::Video;
  bool keyframe = index == 0;
  auto nal = [_this = _this, keyframe]() -> common::Data32 {
    if (keyframe) {
      return _this->storage.reader.read(0, _this->storage.reader.size());
    } else {
      return common::Data32();
    }
  };
  return Sample(pts, dts, keyframe, type, nal);
}

}}}
