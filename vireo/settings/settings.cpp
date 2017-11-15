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

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/security.h"
#include "vireo/dependency.hpp"
#include "vireo/error/error.h"
#include "vireo/settings/settings.h"

extern "C" {
#include "lsmash.h"
uint8_t* mp4a_export_AudioSpecificConfig(lsmash_mp4a_AudioObjectType aot,
                                         uint32_t sample_rate, uint32_t channels,
                                         lsmash_mp4a_aac_sbr_mode sbr_mode,
                                         uint8_t* exdata, uint32_t exdata_length, uint32_t* data_length);
}

namespace vireo {
namespace settings {

#if !defined(ANDROID) && !defined(IOS)
auto webm_export_vorbis_codec_private(settings::Audio& audio_settings) -> common::Data16;
#endif

static const header::SPS_PPS kNoSPSPPS = header::SPS_PPS(common::Data16(NULL, 0, NULL), common::Data16(NULL, 0, NULL), 4);
Video Video::None = { Video::Codec::Unknown, 0, 0, 0, Orientation::Landscape, kNoSPSPPS };
Audio Audio::None = { Audio::Codec::Unknown, 0, 0, 0 };
Data Data::None = { Data::Codec::Unknown, 0 };
Caption Caption::None = { Caption::Codec::Unknown };

auto Video::operator==(const Video& settings) const -> bool {
  return settings.codec == codec && settings.width == width && settings.height == height && settings.timescale == timescale
         && settings.orientation == orientation && settings.sps_pps.sps == sps_pps.sps && settings.sps_pps.pps == sps_pps.pps
         && settings.par_width == par_width && settings.par_height == par_height;
}

auto Video::operator!=(const Video& settings) const -> bool {
  return !(*this == settings);
}

auto Audio::operator==(const Audio& settings) const -> bool {
  return settings.codec == codec && settings.timescale == timescale && settings.sample_rate == sample_rate
         && settings.channels == channels && settings.bitrate == bitrate;
}

auto Audio::operator!=(const Audio& settings) const -> bool {
  return !(*this == settings);
}

auto Data::operator==(const Data& settings) const -> bool {
  return settings.codec == codec && settings.timescale == timescale;
}

auto Data::operator!=(const Data& settings) const -> bool {
  return !(*this == settings);
}

auto Caption::operator==(const Caption& settings) const -> bool {
  return settings.codec == codec;
}

auto Caption::operator!=(const Caption& settings) const -> bool {
  return !(*this == settings);
}

#if !defined(ANDROID) && !defined(IOS)
template <typename Audio::Codec codec, typename std::enable_if<codec == Audio::Codec::Vorbis && has_audio_decoder<Audio::Codec::Vorbis>::value>::type* = nullptr>
static auto as_extradata_private(settings::Audio& audio_settings) -> common::Data16 {
  return webm_export_vorbis_codec_private(audio_settings);
}
template <typename Audio::Codec codec, typename std::enable_if<codec == Audio::Codec::Vorbis && !has_audio_decoder<Audio::Codec::Vorbis>::value>::type* = nullptr>
static auto as_extradata_private(settings::Audio& audio_settings) -> common::Data16 {
  THROW_IF(true, MissingDependency);
}
#endif

auto Audio::as_extradata(ExtraDataType type) const -> common::Data16 {
  THROW_IF(type == adts, InvalidArguments);
#if defined(ANDROID) || defined(IOS)
  THROW_IF(type == vorbis, InvalidArguments);
#endif
  if (type == aac) {
    uint32_t length = 0;
    uint8_t* data = mp4a_export_AudioSpecificConfig(MP4A_AUDIO_OBJECT_TYPE_AAC_LC,
                                                    sample_rate,
                                                    channels,
                                                    codec == Codec::AAC_LC_SBR ? MP4A_AAC_SBR_BACKWARD_COMPATIBLE : MP4A_AAC_SBR_NOT_SPECIFIED,
                                                    nullptr, 0, &length);
    CHECK(data);
    CHECK((length == 2 && codec == Codec::AAC_LC) || (length == 5 && codec == Codec::AAC_LC_SBR));
    return common::Data16(data, length, [](const uint8_t* p) {
      lsmash_free((void*)p);
    });
#if !defined(ANDROID) && !defined(IOS)
  } else if (type == vorbis) {
    settings::Audio audio_settings = { codec, timescale, sample_rate, channels, bitrate };
    return as_extradata_private<settings::Audio::Vorbis>(audio_settings);
#endif
  } else {
    return common::Data16();
  }
}

auto Audio::IsAAC(Codec codec) -> bool {
  switch (codec) {
    case Codec::AAC_Main:
    case Codec::AAC_LC:
    case Codec::AAC_LC_SBR:
      return true;
    default:
      return false;
  }
}

auto Audio::IsPCM(Codec codec) -> bool {
  switch (codec) {
    case Codec::PCM_S16LE:
    case Codec::PCM_S16BE:
    case Codec::PCM_S24LE:
    case Codec::PCM_S24BE:
      return true;
    default:
      return false;
  }
}

auto Video::IsImage(Codec codec) -> bool {
  switch (codec) {
    case Codec::JPG:
    case Codec::PNG:
    case Codec::GIF:
    case Codec::BMP:
    case Codec::WebP:
    case Codec::TIFF:
      return true;
    default:
      return false;
  }
}

auto Video::to_square_pixel() const -> Video {
  auto new_settings = *this;
  new_settings.coded_width = new_settings.width;
  new_settings.coded_height = new_settings.height;
  new_settings.par_width = 1;
  new_settings.par_height = 1;
  return new_settings;
}

}}
