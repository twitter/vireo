//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/common/math.h"
#include "vireo/header/header.h"
#include "vireo/types.h"

namespace vireo {
namespace settings {

template <int Type>
struct Settings {
  static_assert(Type == SampleType::Audio ||
                Type == SampleType::Video ||
                Type == SampleType::Data  ||
                Type == SampleType::Caption, "Undefined for this sample type");
};

template <>
struct Settings<SampleType::Video> {
  enum Codec {
    Unknown = 0,
    H264 = 1,
    VP8 = 2,
    JPG = 3,
    PNG = 4,
    MPEG4 = 5,
    ProRes = 6,
    GIF = 7,
    BMP = 8,
    WebP = 9,
    TIFF = 10,
  };
  Codec codec;
  uint16_t width; // display width
  uint16_t height; // display height
  uint32_t timescale;
  enum Orientation {
    Landscape = 0,
    Portrait = 1,
    LandscapeReverse = 2,
    PortraitReverse = 3,
    UnknownOrientation = 4
  };
  Orientation orientation;
  header::SPS_PPS sps_pps;
  uint16_t par_width;
  uint16_t par_height;
  uint16_t coded_width;
  uint16_t coded_height;

  Settings(Codec codec, uint16_t coded_width, uint16_t coded_height, uint16_t par_width, uint16_t par_height,  uint32_t timescale, Orientation orientation, header::SPS_PPS sps_pps)
    : codec(codec), timescale(timescale), orientation(orientation), sps_pps(sps_pps), par_width(par_width), par_height(par_height), coded_width(coded_width), coded_height(coded_height) {
    if (par_width > par_height) {
      width = coded_width;
      height = common::even_floor(coded_height * par_height / par_width);
    } else if (par_width < par_height) {
      width = common::even_floor(coded_width * par_width / par_height);
      height = coded_height;
    } else {
      width = coded_width;
      height = coded_height;
    }
  }
  Settings(Codec codec, uint16_t width, uint16_t height, uint32_t timescale, Orientation orientation, header::SPS_PPS sps_pps)
    : Settings(codec, width, height, 1, 1, timescale, orientation, sps_pps) {}

  auto operator==(const Settings& settings) const -> bool;
  auto operator!=(const Settings& settings) const -> bool;
  static auto IsImage(Codec codec) -> bool;
  auto to_square_pixel() const -> Settings<SampleType::Video>;
  static Settings None;
};

template <>
struct Settings<SampleType::Audio> {
  enum Codec {
    Unknown = 0,
    AAC_Main = 1,
    AAC_LC = 2,
    AAC_LC_SBR = 3,
    Vorbis = 4,
    PCM_S16LE = 5,
    PCM_S16BE = 6,
    PCM_S24LE = 7,
    PCM_S24BE = 8,
  };
  Codec codec;
  uint32_t timescale;
  uint32_t sample_rate;
  uint8_t channels;
  uint32_t bitrate;
  enum ExtraDataType { aac, adts, vorbis };
  auto as_extradata(ExtraDataType type) const -> common::Data16;
  auto operator==(const Settings& settings) const -> bool;
  auto operator!=(const Settings& settings) const -> bool;
  static auto IsAAC(Codec codec) -> bool;
  static auto IsPCM(Codec codec) -> bool;
  static Settings None;
};

template <>
struct Settings<SampleType::Data> {
  enum Codec {
    Unknown = 0,
    TimedID3 = 1,
  };
  Codec codec;
  uint32_t timescale;
  auto operator==(const Settings& settings) const -> bool;
  auto operator!=(const Settings& settings) const -> bool;
  static Settings None;
};

template <>
struct Settings<SampleType::Caption> {
  enum Codec {
    Unknown = 0
  };
  Codec codec;
  uint32_t timescale;
  auto operator==(const Settings& settings) const -> bool;
  auto operator!=(const Settings& settings) const -> bool;
  static Settings None;
};

typedef Settings<SampleType::Video> Video;
typedef Settings<SampleType::Audio> Audio;
typedef Settings<SampleType::Data> Data;
typedef Settings<SampleType::Caption> Caption;

static const char* kVideoCodecToString[] = {
  "Unknown",
  "H.264",
  "VP8 [decode not supported]",
  "JPG",
  "PNG",
  "MPEG-4 Visual [not supported]",
  "Apple ProRes [not supported]",
  "GIF",
  "BMP",
  "WebP",
  "TIFF",
};

static const char* kOrientationToString[] = {
  "Landscape",
  "Portrait",
  "Landscape Reverse",
  "Portrait Reverse",
  "Unknown orientation",
};

static const char* kAudioCodecToString[] = {
  "Unknown",
  "AAC (Main)",
  "AAC (LC)" ,
  "AAC (LC-SBR)",
  "Vorbis [decode not supported]",
  "PCM (Signed 16-bit Little Endian)",
  "PCM (Signed 16-bit Big Endian)",
  "PCM (Signed 24-bit Little Endian)",
  "PCM (Signed 24-bit Big Endian)",
};

static const char* kDataCodecToString[] = {
  "Unknown",
  "Timed ID3",
};

}}
