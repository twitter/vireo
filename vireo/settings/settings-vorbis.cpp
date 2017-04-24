//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/settings/settings.h"
#include "vireo/version.h"
#include "vorbis/codec.h"
#include "vorbis/vorbisenc.h"

namespace vireo {
namespace settings {

auto webm_export_vorbis_codec_private(settings::Audio& audio_settings) -> common::Data16 {
  vorbis_info settings;
  vorbis_comment comment;
  vorbis_dsp_state dsp_state;

  ogg_packet header_id;       // Identification header
  ogg_packet header_comments; // Comment        header
  ogg_packet header_codec;    // Codec setup    header

  vorbis_info_init(&settings);
  THROW_IF(vorbis_encode_setup_managed(&settings, audio_settings.channels, audio_settings.sample_rate, -1, audio_settings.bitrate, -1) != 0, Invalid);
  THROW_IF(vorbis_encode_ctl(&settings, OV_ECTL_RATEMANAGE2_SET, NULL) != 0, Invalid);
  THROW_IF(vorbis_encode_setup_init(&settings) != 0, Invalid);
  THROW_IF(vorbis_analysis_init(&dsp_state, &settings) != 0, Invalid);
  vorbis_comment_init(&comment);
  char vireo_version[64];
  snprintf(vireo_version, 64, "Vireo Ears v%s", VIREO_VERSION);
  vorbis_comment_add_tag(&comment, "ENCODER", vireo_version);
  THROW_IF(vorbis_analysis_headerout(&dsp_state, &comment, &header_id, &header_comments, &header_codec) != 0, Invalid);

  auto id = common::Data16(header_id.packet, header_id.bytes, [](uint8_t* p){});
  auto comments = common::Data16(header_comments.packet, header_comments.bytes, [](uint8_t* p){});
  auto codec = common::Data16(header_codec.packet, header_codec.bytes, [](uint8_t* p){});

  const uint16_t length = 1 + (id.count() / 255) + 1 + (comments.count() / 255) + 1 + id.count() + comments.count() + codec.count();
  common::Data16 codec_private(new uint8_t[length], length, [](uint8_t* p) { delete[] p; });

  auto xiph_lace = [&codec_private](uint16_t value) {
    uint8_t* bytes = (uint8_t*)codec_private.data() + codec_private.a();
    uint16_t i = 0;
    while (value > 255) {
      bytes[i] = 255;
      ++i;
      value -= 255;
    }
    bytes[i] = value;
    codec_private.set_bounds(codec_private.a() + i + 1, codec_private.b());
  };

  uint8_t* bytes = (uint8_t*)codec_private.data();
  bytes[0] = 2;
  codec_private.set_bounds(1, codec_private.b());
  xiph_lace(id.count());
  xiph_lace(comments.count());
  codec_private.copy(id);
  codec_private.set_bounds(codec_private.b(), codec_private.b());
  codec_private.copy(comments);
  codec_private.set_bounds(codec_private.b(), codec_private.b());
  codec_private.copy(codec);
  codec_private.set_bounds(0, codec_private.b());

  vorbis_dsp_clear(&dsp_state);
  vorbis_comment_clear(&comment);
  vorbis_info_clear(&settings);

  return codec_private;
}

}}
