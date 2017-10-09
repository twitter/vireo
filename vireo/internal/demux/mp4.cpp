//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <functional>
#include <stdio.h>

extern "C" {
#include "lsmash.h"
#include "lsmash-h264.h"
}

#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/common/bitreader.h"
#include "vireo/common/math.h"
#include "vireo/common/reader.h"
#include "vireo/common/security.h"
#include "vireo/error/error.h"
#include "vireo/header/header.h"
#include "vireo/internal/decode/avcc.h"
#include "vireo/internal/decode/types.h"
#include "vireo/internal/demux/mp4.h"
#include "vireo/settings/settings.h"
#include "vireo/types.h"
#include "vireo/util/caption.h"

namespace vireo {
namespace internal {
namespace demux {

static const uint32_t kSizeBuffer = 512 * 1024;
const static uint8_t kNumTracks = 3;

struct _MP4 {
  common::Reader reader;
  unique_ptr<lsmash_root_t, decltype(&lsmash_destroy_root)> root = { nullptr, lsmash_destroy_root };
  unique_ptr<lsmash_file_parameters_t> file;
  uint8_t nalu_length_size = 0;
  struct {
    uint32_t timescale = 0;
  } movie;

  struct Track {
    unique_ptr<lsmash_media_ts_t, decltype(&lsmash_free)> timestamps = { nullptr, lsmash_free };
    unique_ptr<lsmash_summary_t, function<void(lsmash_summary_t* p)>> summary = { nullptr, [](lsmash_summary_t* p) {
      lsmash_cleanup_summary(p);
    }};
    uint32_t track_ID = 0;
    uint32_t timescale = 0;
    uint64_t duration = 0;
    uint64_t playback_duration = 0;  // after Edit Boxes are applied
    uint32_t sample_count = 0;
    vector<common::EditBox> edit_boxes;
  };

  class {
    Track _tracks[kNumTracks];
  public:
    auto operator()(const SampleType type) -> Track& {
      uint32_t i = type == SampleType::Caption ? 2 : type - SampleType::Video;
      THROW_IF(i >= kNumTracks, OutOfRange);
      return _tracks[i];
    };
  } tracks;

  struct {
    settings::Video::Codec codec = settings::Video::Codec::Unknown;
    uint16_t width = 0;
    uint16_t height = 0;
    settings::Video::Orientation orientation = settings::Video::Orientation::UnknownOrientation;
    unique_ptr<header::SPS_PPS> sps_pps = nullptr;
    vector<lsmash_media_ts_t> pts_sorted_timestamps;  // to detect open GOPs and only report IDR frames as keyframe (mitigation of l-smash bug)
    uint32_t first_keyframe_index = 0;  // to mark non-decodable non-IDR frames at the beginning (MEDIASERV-4818)
    uint16_t par_width = 0;
    uint16_t par_height = 0;
  } video;

  struct {
    settings::Audio::Codec codec = settings::Audio::Codec::Unknown;
    uint32_t sample_rate = 0;
    uint8_t channels = 0;
    vector<Sample> pcm_samples;  // used only when codec is one of the PCM codecs
  } audio;

  struct {
    settings::Caption::Codec codec = settings::Caption::Codec::Unknown;
  } caption;

  _MP4(common::Reader&& reader) : reader(move(reader)) {}

  void enforce_correct_pts(lsmash_media_ts_list_t& ts_list) {
    // sometimes l-smash calculates pts values incorrectly
    // pts is calculated from dts & offset as: pts = dts + offset
    // this happens when "ctts" box has the wrong version in MP4 headers
    // l-smash represents offset with uint32_t (version 0) instead of int32_t (version 1)
    lsmash_media_ts_t* timestamps = ts_list.timestamp;
    for (uint32_t i = 0; i < ts_list.sample_count; ++i) {
      int64_t offset = timestamps[i].cts - timestamps[i].dts;
      THROW_IF(offset < numeric_limits<int32_t>::min() ||
               offset > numeric_limits<uint32_t>::max(), Invalid);  // offset has to be either int32_t or uint32_t
      if (offset > ((int64_t)numeric_limits<uint32_t>::max() + numeric_limits<int16_t>::min())) {
        // casting to int32_t will overflow and wrap offset value to negative
        int32_t negative_offset = (int32_t)offset;
        timestamps[i].cts = timestamps[i].dts + negative_offset;
      }
    }
  }

  struct pixel_aspect_ratio {
    uint32_t x = 1;
    uint32_t y = 1;
  };

  struct transform_info {
    settings::Video::Orientation orientation = settings::Video::Orientation::UnknownOrientation;
    pixel_aspect_ratio par;
  };

  void parse_video_resolution(lsmash_track_parameters_t& track_param) {
    const SampleType type = SampleType::Video;

    // width / height
    lsmash_video_summary_t* video_summary = (lsmash_video_summary_t*)tracks(type).summary.get();
    video.width = video_summary->width;
    video.height = video_summary->height;
    THROW_IF(!security::valid_dimensions(video.width, video.height), Unsafe);

    // orientation + pixel aspect ratio
    auto get_transform_info = [](array<int32_t, 9> matrix) -> transform_info {
      THROW_IF(matrix[2] != 0x0, Unsupported);
      THROW_IF(matrix[5] != 0x0, Unsupported);
      THROW_IF(matrix[8] != 0x40000000, Unsupported);
      transform_info info;
      auto atan2_in_degree = [](int32_t y, int32_t x) -> int32_t {
        return (int32_t)(atan2((double)y, (double)x) * 180 / M_PI);
      };

      int32_t degree = atan2_in_degree(matrix[1], matrix[0]);
      if (degree % 90 == 0) {
        const int32_t multiplier = 0x10000;
        int32_t degree_check = numeric_limits<int32_t>::min();  // initialize to an invalid degree
        if (matrix[1] == 0x0 && matrix[3] == 0x0) {
          // Landscape / LandscapeReverse
          THROW_IF((matrix[0] == 0) || (matrix[0] % multiplier), Invalid);
          THROW_IF((matrix[4] == 0) || (matrix[4] % multiplier), Invalid);
          info.par.x = abs(matrix[0]) / multiplier;
          info.par.y = abs(matrix[4]) / multiplier;
          degree_check = atan2_in_degree(0, matrix[4]);  // matrix[0] & matrix[4] must have same signs
        } else if (matrix[0] == 0x0 && matrix[4] == 0x0) {
          // Portrait / PortraitReverse
          THROW_IF((matrix[1] == 0) || (matrix[1] % multiplier), Invalid);
          THROW_IF((matrix[3] == 0) || (matrix[3] % multiplier), Invalid);
          info.par.x = abs(matrix[1]) / multiplier;
          info.par.y = abs(matrix[3]) / multiplier;
          degree_check = atan2_in_degree(-matrix[3], 0);  // matrix[1] & matrix[3] must have opposite signs
        }
        THROW_IF(degree != degree_check, Invalid);
        while (degree < 0) {
          degree += 360;
        }
        info.orientation = (settings::Video::Orientation)round(degree / 90);
      }
      return info;
    };

    transform_info info = get_transform_info({
      track_param.matrix[0], track_param.matrix[1], track_param.matrix[2],
      track_param.matrix[3], track_param.matrix[4], track_param.matrix[5],
      track_param.matrix[6], track_param.matrix[7], track_param.matrix[8],
    });
    pixel_aspect_ratio par = info.par;
    if (video.sps_pps.get()) {
      pixel_aspect_ratio par_from_sps;
      if (parse_pixel_aspect_ratio(video.sps_pps->sps, par_from_sps)) {
        par = par_from_sps;
      }
    }
    video.par_width = (uint16_t)par.x;
    video.par_height = (uint16_t)par.y;

    THROW_IF(info.orientation == settings::Video::Orientation::UnknownOrientation, Unsupported);
    video.orientation = info.orientation;
  }

  bool parse_pixel_aspect_ratio(common::Data16& sps, pixel_aspect_ratio& par) {
    h264_info_t h264_info;
    if (h264_setup_parser(&h264_info, 1) != 0) {
      return false;
    }

    bool success = false;
    int ret = 0;
    if (sps.count()) {
      ret = h264_parse_sps(&h264_info, h264_info.buffer.rbsp, (uint8_t*)(sps.data() + 1), sps.count() - 1);
    }
    h264_cleanup_parser(&h264_info);
    if (ret == 0 && h264_info.sps.vui.sar_width != 0 && h264_info.sps.vui.sar_height != 0) {
      par.x = h264_info.sps.vui.sar_width;
      par.y = h264_info.sps.vui.sar_height;
      success = true;
    }
    return success;
  }

  void parse_video_codec_info(lsmash_track_parameters_t& track_param) {
    const SampleType type = SampleType::Video;
    lsmash_video_summary_t* video_summary = (lsmash_video_summary_t*)tracks(type).summary.get();
    if (lsmash_check_box_type_identical(video_summary->sample_type, ISOM_CODEC_TYPE_AVC1_VIDEO)) {  // H.264
      // SPS, PPS
      const uint32_t video_cs_count = lsmash_count_codec_specific_data(tracks(type).summary.get());
      THROW_IF(video_cs_count > 10, Unsafe);
      for (int i = 0; i < video_cs_count; ++i) {
        lsmash_codec_specific_t* cs = lsmash_get_codec_specific_data(tracks(type).summary.get(), i + 1);
        CHECK(cs);
        if (!(cs->format == LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED &&
              cs->size >= 8 &&
              (cs->data.unstructured[4] == 'a' &&
               cs->data.unstructured[5] == 'v' &&
               cs->data.unstructured[6] == 'c' &&
               cs->data.unstructured[7] == 'C'))) {
                continue;
              }
        const uint8_t* data = cs->data.unstructured;
        uint16_t offset = 8;
        THROW_IF(cs->size <= offset + 8, Invalid);
        THROW_IF(data[offset++] != 0x01, Invalid);
        offset += 3;  // data[offset]: Profile (100, 110, 122, 144 etc.), data[offset + 1] = Compatibility, data[offset + 2] = Level
        nalu_length_size = (data[offset++] & 0x03) + 1; // reserved (6 bits), NALU length size - 1 (2 bits)
        THROW_IF(nalu_length_size != 2 && nalu_length_size != 4, Unsupported);
        THROW_IF((data[offset++] & 0x1F) != 1, Invalid); // reserved (3 bits), num of SPS (5 bits)
        const uint16_t sps_size = data[offset] << 8 | data[offset + 1];
        THROW_IF(!sps_size, Invalid);
        THROW_IF(sps_size > security::kMaxHeaderSize, Unsafe);
        offset += 2;
        common::Data16 sps = common::Data16(&data[offset], sps_size, nullptr);
        THROW_IF(cs->size <= offset + sps_size + 2, Invalid);
        offset += sps_size;
        THROW_IF(data[offset++] != 1, Unsupported);  // num of SPS (8 bits)
        const uint16_t pps_size = data[offset] << 8 | data[offset + 1];
        THROW_IF(!pps_size, Invalid);
        THROW_IF(pps_size > security::kMaxHeaderSize, Unsafe);
        offset += 2;
        THROW_IF(offset + pps_size != cs->size && offset + pps_size + 4 != cs->size, Invalid);
        common::Data16 pps = common::Data16(&data[offset], pps_size, nullptr);
        if (offset + pps_size + 4 == cs->size) {  // some videos have extra 4 bytes
          offset += pps_size;
          // data[offset]     - reserved (6 bits), chroma_format           (2 bits), 1=YUV420
          // data[offset + 1] - reserved (5 bits), bit_depth_luma_minus8   (3 bits)
          // data[offset + 2] - reserved (5 bits), bit_depth_chroma_minus8 (3 bits)
          const uint8_t num_sps_ext = data[offset + 3];  // num of SPS Ext (8 bits)
          THROW_IF(num_sps_ext != 0, Unsupported);
        }
        video.sps_pps.reset(new header::SPS_PPS(sps, pps, nalu_length_size));
        video.codec = settings::Video::Codec::H264;
        break;  // we found the info we wanted, no need to check the rest of the codec specific data
      }
    } else {
      if (lsmash_check_box_type_identical(video_summary->sample_type, ISOM_CODEC_TYPE_MP4V_VIDEO)) {  // MPEG-4 Visual
        video.codec = settings::Video::Codec::MPEG4;
      } else if (lsmash_check_box_type_identical(video_summary->sample_type, QT_CODEC_TYPE_APCH_VIDEO) ||
                 lsmash_check_box_type_identical(video_summary->sample_type, QT_CODEC_TYPE_APCN_VIDEO) ||
                 lsmash_check_box_type_identical(video_summary->sample_type, QT_CODEC_TYPE_APCS_VIDEO) ||
                 lsmash_check_box_type_identical(video_summary->sample_type, QT_CODEC_TYPE_APCO_VIDEO) ||
                 lsmash_check_box_type_identical(video_summary->sample_type, QT_CODEC_TYPE_AP4H_VIDEO)) {  // Apple ProRes
        video.codec = settings::Video::Codec::ProRes;
      }
      video.sps_pps.reset(new header::SPS_PPS(common::Data16(), common::Data16(), 4));  // SPS / PPS does not exist for these codecs, mock it
    }
  }

  void parse_audio_codec_info(lsmash_track_parameters_t& track_param) {
    const SampleType type = SampleType::Audio;

    lsmash_audio_summary_t* audio_summary = (lsmash_audio_summary_t*)tracks(type).summary.get();

    audio.sample_rate = audio_summary->frequency;
    THROW_IF(find(kSampleRate.begin(), kSampleRate.end(), audio.sample_rate) == kSampleRate.end(), Unsupported);

    audio.channels = audio_summary->channels;
    THROW_IF(!audio.channels, Invalid);
    THROW_IF(audio.channels > 2, Unsupported);

    if (lsmash_check_box_type_identical(audio_summary->sample_type, ISOM_CODEC_TYPE_MP4A_AUDIO)) {  // AAC
      THROW_IF(audio_summary->sample_size != 16, Unsupported);
      THROW_IF(audio_summary->aot != MP4A_AUDIO_OBJECT_TYPE_NULL &&
               audio_summary->aot != MP4A_AUDIO_OBJECT_TYPE_AAC_LC, Invalid);
      audio.codec = settings::Audio::Codec::AAC_LC;  // assume AAC-LC - unless stated otherwise via codec specific data
    } else if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_SOWT_AUDIO)) {  // PCM 16-bit Little Endian
      THROW_IF(audio_summary->sample_size != 16, Invalid);
      audio.codec = settings::Audio::Codec::PCM_S16LE;
    } else if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_TWOS_AUDIO)) {  // PCM 16-bit Big Endian
      THROW_IF(audio_summary->sample_size != 16, Invalid);
      audio.codec = settings::Audio::Codec::PCM_S16BE;
    } else if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_IN24_AUDIO)) {  // PCM 24-bit
      THROW_IF(audio_summary->sample_size != 24, Invalid);
      audio.codec = settings::Audio::Codec::PCM_S24LE;  // assume Little Endian - unless stated otherwise via codec specific data
    } else if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_LPCM_AUDIO)) {  // PCM (various)
      THROW_IF(audio_summary->sample_size != 16, Unsupported);  // can technically be different but not tested
      audio.codec = settings::Audio::Codec::PCM_S16LE;  // assume Little Endian - unless stated otherwise via codec specific data
    }

    const uint32_t audio_cs_count = lsmash_count_codec_specific_data(tracks(type).summary.get());
    THROW_IF(audio_cs_count > 10, Unsafe);
    for (int i = 0; i < audio_cs_count; ++i) {
      lsmash_codec_specific_t* cs = lsmash_get_codec_specific_data(tracks(type).summary.get(), i + 1);
      CHECK(cs);
      if (cs->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG) {
        THROW_IF(audio_summary->aot != MP4A_AUDIO_OBJECT_TYPE_AAC_LC, Unsupported);
        THROW_IF(audio_summary->samples_in_frame != AUDIO_FRAME_SIZE, Unsupported);
        lsmash_mp4sys_decoder_parameters_t* params = (lsmash_mp4sys_decoder_parameters_t*)cs->data.structured;
        THROW_IF(params->objectTypeIndication != MP4SYS_OBJECT_TYPE_Audio_ISO_14496_3, Invalid);  // AAC

        uint8_t* payload;
        uint32_t payload_length;
        lsmash_get_mp4sys_decoder_specific_info(params, &payload, &payload_length);
        common::BitReader bit_reader(common::Data32(payload, payload_length, [](uint8_t* p) { free(p); }));

        // ISO/IEC 14496-3 - Section 1.6.2.1 - AudioSpecificConfig - payload in bit string format (packed bits)
        const uint8_t audio_object_type = bit_reader.read_bits(5);                          // audioObjectType             (5 bits)
        THROW_IF(audio_object_type != 2, Unsupported);                                      // 2=AAC-LC, 5=SBR
        audio.codec = settings::Audio::Codec::AAC_LC;
        const uint8_t sampling_frequency_index = bit_reader.read_bits(4);                   // samplingFrequencyIndex      (4 bits)
        THROW_IF(sampling_frequency_index == 0x0F, Unsupported);                            // if 0x0F, samplingFrequency (24 bits) - not supported!
        const uint8_t channel_configuration = bit_reader.read_bits(4);                      // channelConfiguration        (4 bits)
        THROW_IF(channel_configuration != 1 && channel_configuration != 2, Unsupported);    // mono/stereo

        // GASpecificConfig
        THROW_IF(audio_object_type != 2, Invalid);                                          // GASpecificConfig present only for audioObjectType == 2
        const bool frame_length_flag = bit_reader.read_bits(1);                             // frameLengthFlag             (1 bit)
        THROW_IF(frame_length_flag, Unsupported);                                           // if true, alternate frame length      - not supported!
        const bool depends_on_core_coder = bit_reader.read_bits(1);                         // dependsOnCoreCoder          (1 bit)
        THROW_IF(depends_on_core_coder, Unsupported);                                       // if true, coreCoderDelay    (14 bits) - not supported!
        const bool extension_flag = bit_reader.read_bits(1);                                // extensionFlag               (1 bit)
        THROW_IF(extension_flag, Unsupported);                                              // if true, more data                   - not supported!

        if (bit_reader.remaining() >= 16) {
          // potentially we might read 24 bits here but we are checking for >= 16 bits per standard
          // BitReader will throw an exception if we try to read beyond the available data so it's safe
          THROW_IF(audio_object_type == 5, Invalid);
          const uint16_t sync_extension_type = bit_reader.read_bits(11);                   // syncExtensionType              (11 bits)
          if (sync_extension_type == 0x02B7) {
            const uint8_t extension_audio_object_type = bit_reader.read_bits(5);           // extensionAudioObjectType        (5 bits)
            THROW_IF(extension_audio_object_type != 5, Unsupported);
            const bool sbr_present_flag = bit_reader.read_bits(1);                         // sbrPresentFlag                  (1 bit)
            if (sbr_present_flag) {
              const uint8_t extension_sampling_frequency_index = bit_reader.read_bits(4);  // extensionSamplingFrequencyIndex (4 bits)
              THROW_IF(extension_sampling_frequency_index + 3 != sampling_frequency_index, Invalid);
              audio.codec = settings::Audio::Codec::AAC_LC_SBR;
            }
          }
        }
        break;  // we found the info we wanted, no need to check the rest of the codec specific data
      } else if (cs->type == LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS) {
        if (settings::Audio::IsPCM(audio.codec)) {
          lsmash_qt_audio_format_specific_flags_t* flags = (lsmash_qt_audio_format_specific_flags_t*)cs->data.structured;
          THROW_IF(flags->format_flags & QT_AUDIO_FORMAT_FLAG_NON_INTERLEAVED, Unsupported);
          if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_IN24_AUDIO)) {  // PCM 24-bit
            CHECK(audio.codec == settings::Audio::Codec::PCM_S24LE);
            if (flags->format_flags & QT_AUDIO_FORMAT_FLAG_BIG_ENDIAN) {
              audio.codec = settings::Audio::Codec::PCM_S24BE;  // Big Endian
            }
          } else if (lsmash_check_box_type_identical(audio_summary->sample_type, QT_CODEC_TYPE_LPCM_AUDIO)) {  // LPCM
            CHECK(audio.codec == settings::Audio::Codec::PCM_S16LE);
            THROW_IF(!(flags->format_flags & QT_AUDIO_FORMAT_FLAG_SIGNED_INTEGER), Unsupported);
            THROW_IF(!(flags->format_flags & QT_AUDIO_FORMAT_FLAG_PACKED), Unsupported);
            if (flags->format_flags & QT_AUDIO_FORMAT_FLAG_BIG_ENDIAN) {
              audio.codec = settings::Audio::Codec::PCM_S16BE;  // Big Endian
            }
          }
          break;  // we found the info we wanted, no need to check the rest of the codec specific data
        }
      } else {
        // fail if we see an unexpected codec specific data, otherwise just skip
        THROW_IF(cs->type != LSMASH_CODEC_SPECIFIC_DATA_TYPE_UNKNOWN &&
                 cs->type != LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_COMMON &&
                 cs->type != LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_CHANNEL_LAYOUT &&
                 cs->type != LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_DECOMPRESSION_PARAMETERS, Unsupported);
      }
    }
  }

  void parse_codec_info(lsmash_track_parameters_t& track_param, SampleType type) {
    THROW_IF(type != SampleType::Audio && type != SampleType::Video, InvalidArguments);
    if (type == SampleType::Video) {
      parse_video_codec_info(track_param);
      if (video.codec == settings::Video::Codec::H264) {
        caption.codec = settings::Caption::Codec::Unknown;
      }
    } else {
      parse_audio_codec_info(track_param);
    }
  }

  void parse_samples(lsmash_track_parameters_t& track_param, SampleType type) {
    if (tracks(type).duration) {
      THROW_IF(lsmash_construct_timeline(root.get(), tracks(type).track_ID) != 0, Invalid);
      tracks(type).sample_count = lsmash_get_sample_count_in_media_timeline(root.get(), tracks(type).track_ID);
    }

    if (tracks(type).sample_count) {
      if (type == SampleType::Audio && settings::Audio::IsPCM(audio.codec)) {
        // accumulate neighboring PCM samples into larger pieces to reduce total number of samples (MEDIASERV-6821)
        uint8_t num_bytes_per_sample = sizeof(int16_t) * audio.channels;
        if (audio.codec == settings::Audio::Codec::PCM_S24LE ||
            audio.codec == settings::Audio::Codec::PCM_S24BE) {
          num_bytes_per_sample = 24 / CHAR_BIT * audio.channels;
        }
        const uint32_t max_bytes_to_accumulate = AUDIO_FRAME_SIZE * num_bytes_per_sample;
        uint32_t total_bytes = 0;

        auto aligned_with_audio_frame_size = [num_bytes_per_sample](uint32_t bytes) -> bool {
          return bytes % (AUDIO_FRAME_SIZE * num_bytes_per_sample) == 0;
        };
        auto save_anchor_sample = [_this = this, &total_bytes, num_bytes_per_sample, &aligned_with_audio_frame_size](lsmash_sample_t anchor_sample, uint32_t size) {
          THROW_IF(anchor_sample.pos > numeric_limits<uint32_t>::max(), Overflow);
          uint32_t pos = (uint32_t)anchor_sample.pos;
          auto nal = [_this, pos, size]() -> common::Data32 {
            auto nal_data = _this->reader.read(pos, size);
            THROW_IF(nal_data.count() != size, ReaderError);
            return move(nal_data);
          };
          bool keyframe = anchor_sample.prop.ra_flags & ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC;
          keyframe &= aligned_with_audio_frame_size(total_bytes);  // safe to split track at these boundaries
          _this->audio.pcm_samples.push_back(Sample(anchor_sample.cts,
                                                    anchor_sample.dts,
                                                    keyframe,
                                                    SampleType::Audio,
                                                    nal,
                                                    pos,
                                                    size));
          total_bytes += size;
        };
        lsmash_sample_t anchor_sample;
        lsmash_sample_t prev_sample;
        uint32_t bytes_accumulated = 0;
        for (int index = 0; index < tracks(SampleType::Audio).sample_count; ++index) {
          lsmash_sample_t sample;
          lsmash_get_sample_info_from_media_timeline(root.get(), tracks(SampleType::Audio).track_ID, index + 1, &sample);
          THROW_IF(sample.length != num_bytes_per_sample, Unsupported);

          bool first_sample = index == 0;
          if (first_sample) {
            anchor_sample = sample;
          } else {
            THROW_IF(sample.cts - prev_sample.cts != 1, Unsupported);
            THROW_IF(sample.dts - prev_sample.dts != 1, Unsupported);
          }
          bool aligned = aligned_with_audio_frame_size(total_bytes + bytes_accumulated);
          bool continuous = sample.pos == prev_sample.pos + prev_sample.length;
          CHECK(bytes_accumulated <= max_bytes_to_accumulate);
          bool enough_bytes = bytes_accumulated == max_bytes_to_accumulate;
          bool new_data_block = !first_sample && (!continuous || aligned || enough_bytes);
          bool last_sample = index == (tracks(SampleType::Audio).sample_count - 1);
          if (new_data_block || last_sample) {
            // save the anchor sample and mark current sample as the anchor
            if (last_sample && !new_data_block) {
              bytes_accumulated += sample.length;  // also add last sample as part of anchor sample
            }
            save_anchor_sample(anchor_sample, bytes_accumulated);
            if (last_sample && new_data_block) {
              save_anchor_sample(sample, sample.length);  // add last sample separately
            }
            anchor_sample = sample;
            bytes_accumulated = 0;
          }
          bytes_accumulated += sample.length;
          prev_sample = sample;
        }
        tracks(type).sample_count = (uint32_t)audio.pcm_samples.size();  // update sample count
      } else {
        lsmash_media_ts_list_t ts_list;
        THROW_IF(lsmash_get_media_timestamps(root.get(), tracks(type).track_ID, &ts_list) != 0, Invalid);
        THROW_IF(!ts_list.timestamp, Invalid);
        CHECK(ts_list.sample_count == tracks(type).sample_count);
        enforce_correct_pts(ts_list);  // Mitigation of MEDIASERV-4739
        tracks(type).timestamps.reset(ts_list.timestamp);
      }

      if (type == SampleType::Video) {
        // Create an additional list of pts sorted timestamps - used for Open GOP detection
        CHECK(tracks(type).timestamps);
        vector<lsmash_media_ts_t> pts_sorted_timestamps(tracks(type).timestamps.get(), tracks(type).timestamps.get() + tracks(type).sample_count);
        sort(pts_sorted_timestamps.begin(), pts_sorted_timestamps.end(), [](const lsmash_media_ts_t& a, const lsmash_media_ts_t& b){ return a.cts < b.cts; });
        swap(video.pts_sorted_timestamps, pts_sorted_timestamps);

        // Handle non-standard inputs, discard samples at the beginning of the video track until the first keyframe
        for (uint32_t index = 0; index < tracks(type).sample_count; ++index) {
          lsmash_sample_property_t sample_property;
          lsmash_get_sample_property_from_media_timeline(root.get(), tracks(type).track_ID, index + 1, &sample_property);
          if (sample_property.ra_flags & ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC) {
            video.first_keyframe_index = index;
            break;
          }
        }
      }
    }
  }

  void parse_edit_boxes(SampleType type) {
    const uint32_t track_ID = tracks(type).track_ID;
    const uint32_t num_edits = lsmash_count_explicit_timeline_map(root.get(), track_ID);
    THROW_IF(num_edits > 60, Unsafe);  // 30s / 500ms = 60 edits max

    uint64_t remaining_playback_duration = common::round_divide(tracks(type).playback_duration, (uint64_t)tracks(type).timescale, (uint64_t)movie.timescale);
    for (uint32_t edit_number = 1; edit_number <= num_edits; ++edit_number) {
      lsmash_edit_t edit;
      THROW_IF(lsmash_get_explicit_timeline_map(root.get(), track_ID, edit_number, &edit) != 0, Invalid);
      THROW_IF(edit.rate != ISOM_EDIT_MODE_NORMAL, Unsupported);
      THROW_IF(edit.duration == ISOM_EDIT_DURATION_UNKNOWN32 || edit.duration == ISOM_EDIT_DURATION_UNKNOWN64, Unsupported);

      const bool empty_edit_box = edit.start_time == ISOM_EDIT_MODE_EMPTY;
      THROW_IF(!empty_edit_box && edit.start_time < 0, Invalid);
      if (edit.duration == ISOM_EDIT_DURATION_IMPLICIT) {
        if (!empty_edit_box) {
          edit.duration = common::round_divide(tracks(type).duration, (uint64_t)movie.timescale, (uint64_t)tracks(type).timescale);
        } else {
          continue;  // EditBox (-1, 0) has no effect on playback, ignore
        }
      }
      const uint64_t duration_pts = common::round_divide(edit.duration, (uint64_t)tracks(type).timescale, (uint64_t)movie.timescale);
      const common::EditBox edit_box = common::EditBox(edit.start_time, duration_pts, 1.0f, type);

      if (remaining_playback_duration) {  // if existing edit boxes already cover entire playback duration, rest don't have any effect, ignore
        tracks(type).edit_boxes.push_back(edit_box);
        if (type == SampleType::Video) {
          const common::EditBox caption_edit_box = common::EditBox(edit.start_time, duration_pts, 1.0f, SampleType::Caption);
          tracks(SampleType::Caption).edit_boxes.push_back(caption_edit_box); // clone video track edit boxes for caption
        }
        remaining_playback_duration -= min(remaining_playback_duration, duration_pts);
      }
    }
  }

  bool finish_initialization() {
    if (!root) {
      return false;
    }
    lsmash_movie_parameters_t movie_param;
    lsmash_initialize_movie_parameters(&movie_param);
    THROW_IF(lsmash_get_movie_parameters(root.get(), &movie_param) != 0, Invalid);
    movie.timescale = movie_param.timescale;
    const uint32_t num_tracks = movie_param.number_of_tracks;
    for (uint32_t index = 1; index <= num_tracks; ++index) {
      uint32_t track_ID = lsmash_get_track_ID(root.get(), index);
      THROW_IF(!track_ID, Invalid);
      const uint32_t num_summary = lsmash_count_summary(root.get(), track_ID);
      if (num_summary) {  // found a media track
        unique_ptr<lsmash_summary_t, decltype(&lsmash_cleanup_summary)> summary(lsmash_get_summary(root.get(), track_ID, 1), lsmash_cleanup_summary);
        if (summary) {  // found a valid media track
          auto type = SampleType::Unknown;
          if (summary->summary_type == LSMASH_SUMMARY_TYPE_VIDEO) {
            type = SampleType::Video;
          } else if (summary->summary_type == LSMASH_SUMMARY_TYPE_AUDIO) {
            type = SampleType::Audio;
          } else {  // LSMASH_SUMMARY_TYPE_UNKNOWN
            continue;
          }

          if (tracks(type).track_ID) {
            continue;  // already found a track of this type, skip
          }
          tracks(type).track_ID = track_ID;
          tracks(type).summary.reset(lsmash_get_summary(root.get(), track_ID, 1));

          lsmash_media_parameters_t media_param;
          lsmash_initialize_media_parameters(&media_param);
          THROW_IF(lsmash_get_media_parameters(root.get(), track_ID, &media_param) != 0, Invalid);
          tracks(type).timescale = media_param.timescale;
          tracks(type).duration = media_param.duration;

          lsmash_track_parameters_t track_param;
          lsmash_initialize_track_parameters(&track_param);
          THROW_IF(lsmash_get_track_parameters(root.get(), track_ID, &track_param) != 0, Invalid);
          tracks(type).playback_duration = track_param.duration;

          parse_codec_info(track_param, type);

          if (type == SampleType::Video) {
            parse_video_resolution(track_param);
          }
          parse_samples(track_param, type);
          parse_edit_boxes(type);
        }
      }
    }
    if (tracks(SampleType::Video).track_ID) {
      tracks(SampleType::Caption).timescale = tracks(SampleType::Video).timescale;
      tracks(SampleType::Caption).duration = tracks(SampleType::Video).duration;
    }
    return true;
  }

  Sample video_sample(const uint32_t index) {
    THROW_IF(!root.get(), Uninitialized);
    const uint32_t input_index = index + video.first_keyframe_index;
    const SampleType type = SampleType::Video;
    THROW_IF(input_index >= tracks(type).sample_count, OutOfRange);
    CHECK(tracks(type).timestamps);
    const lsmash_media_ts_t& media_ts = tracks(type).timestamps.get()[input_index];
    THROW_IF(media_ts.cts > std::numeric_limits<int64_t>::max() || media_ts.dts > std::numeric_limits<int64_t>::max(), Unsupported);
    int64_t pts = media_ts.cts;
    int64_t dts = media_ts.dts;
    lsmash_sample_t sample;
    lsmash_get_sample_info_from_media_timeline(root.get(), tracks(type).track_ID, input_index + 1, &sample);
    uint32_t pos = (uint32_t)sample.pos;
    uint32_t size = sample.length;
    lsmash_sample_property_t sample_property;
    lsmash_get_sample_property_from_media_timeline(root.get(), tracks(type).track_ID, input_index + 1, &sample_property);
    bool keyframe = sample_property.ra_flags & ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC;
    if (index) {
      // Add the following checks for key frames to detect open GOPs
      // But only do the check for index > 0: if first frame is key frame, always assume it an IDR frame
      THROW_IF(input_index >= video.pts_sorted_timestamps.size(), OutOfRange);
      keyframe = keyframe & (video.pts_sorted_timestamps[input_index].cts == pts);  // detect open GOPs and only report IDR frames as keyframe
      keyframe = keyframe & (video.pts_sorted_timestamps[input_index].dts == dts);  // TODO: remove this logic once MEDIASERV-4386 is resolved
    }
    THROW_IF(!index && !keyframe, Invalid);
    auto nal = [_this = this, input_index]() -> common::Data32 {
      lsmash_sample_t* sample = lsmash_get_sample_from_media_timeline(_this->root.get(), _this->tracks(type).track_ID, input_index + 1);
      CHECK(sample);
      common::Data32 data = common::Data32(sample->data, sample->length, [sample](void* p) {
        lsmash_delete_sample(sample);
      });
      return move(data);
    };
    return Sample(pts, dts, keyframe, type, nal, pos, size);
  }

  vector<ByteRange> get_sei_ranges(common::Data32& data) {
    vector<ByteRange> sei_ranges;
    if (video.codec == settings::Video::Codec::H264) {
      decode::AVCC<decode::H264NalType> avcc_parser(data, nalu_length_size);
      for (const auto& info: avcc_parser) {
        if (info.type == decode::H264NalType::SEI) {
          uint32_t pos = info.byte_offset - nalu_length_size;
          uint32_t size = info.size + nalu_length_size;
          sei_ranges.push_back(ByteRange(pos, size));
        }
      }
    }
    return sei_ranges;
  }
};

MP4::MP4(common::Reader&& reader)
  : _this(make_shared<_MP4>(move(reader))), audio_track(_this), video_track(_this), caption_track(_this) {
  _this->root.reset(lsmash_create_root());
  _this->file.reset(new lsmash_file_parameters_t());
  memset((void*)_this->file.get(), 0, sizeof(lsmash_file_parameters_t));

  _this->file->mode = LSMASH_FILE_MODE_READ;
  _this->file->opaque = (void*)_this->reader.opaque;
  _this->file->read = _this->reader.read_callback;
  _this->file->write = nullptr;
  _this->file->seek = _this->reader.seek_callback;
  _this->file->brand_count = 0;
  _this->file->minor_version = 0;
  _this->file->max_chunk_duration = 0.5;
  _this->file->max_async_tolerance = 2.0;
  _this->file->max_chunk_size = kSizeBuffer;
  _this->file->max_read_size = kSizeBuffer;

  lsmash_file_t* file = lsmash_set_file(_this->root.get(), _this->file.get());
  THROW_IF(lsmash_read_file(file, _this->file.get()) < 0, Invalid);

  if (_this->finish_initialization()) {
    video_track.set_bounds(0, _this->tracks(SampleType::Video).sample_count - _this->video.first_keyframe_index);
    audio_track.set_bounds(0, _this->tracks(SampleType::Audio).sample_count);
    if (_this->video.codec == settings::Video::Codec::H264) {
      caption_track.set_bounds(video_track.a(), video_track.b()); // don't have caption information yet, use video bounds to set caption bounds
    } else {
      caption_track.set_bounds(0, 0);
    }

    if (_this->tracks(SampleType::Video).track_ID) {
      CHECK(_this->video.sps_pps.get());
      video_track._settings = (settings::Video){
        _this->video.codec,
        _this->video.width,
        _this->video.height,
        _this->video.par_width,
        _this->video.par_height,
        _this->tracks(SampleType::Video).timescale,
        _this->video.orientation,
        *_this->video.sps_pps
      };

      caption_track._settings = (settings::Caption){
        _this->caption.codec,
        _this->tracks(SampleType::Caption).timescale
      };
    }

    if (_this->tracks(SampleType::Audio).track_ID) {
      audio_track._settings = (settings::Audio){
        _this->audio.codec,
        _this->tracks(SampleType::Audio).timescale,
        _this->audio.sample_rate,
        _this->audio.channels,
        0
      };
    }
  } else {
    _this->root.reset(nullptr);
    _this->video.sps_pps.reset(nullptr);
    THROW_IF(true, Uninitialized);
  }
}

MP4::MP4(MP4&& mp4)
  : audio_track(_this), video_track(_this), caption_track(_this) {
  _this = mp4._this;
  mp4._this = nullptr;
}

MP4::VideoTrack::VideoTrack(const std::shared_ptr<_MP4>& _mp4_this)
  : _this(_mp4_this) {}

MP4::VideoTrack::VideoTrack(const VideoTrack& video_track)
  : functional::DirectVideo<VideoTrack, Sample>(video_track.a(), video_track.b()), _this(video_track._this) {}

auto MP4::VideoTrack::duration() const -> uint64_t {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Video).duration;
}

auto MP4::VideoTrack::edit_boxes() const -> const vector<common::EditBox>& {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Video).edit_boxes;
}

auto MP4::VideoTrack::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0.0f;
}

auto MP4::VideoTrack::operator()(const uint32_t index) const -> Sample {
  THROW_IF(!_this->root.get(), Uninitialized);
  THROW_IF(index >= b(), OutOfRange);
  auto sample = _this->video_sample(index);
  auto nal = [_this = _this, sample]() -> common::Data32 {
    common::Data32 data = sample.nal();
    vector<ByteRange> sei_ranges = _this->get_sei_ranges(data);
    bool has_caption = false;
    for (const auto& range: sei_ranges) {
      uint32_t sei_data_pos = data.a() + range.pos + _this->nalu_length_size;
      uint32_t sei_data_size = range.size - _this->nalu_length_size;
      THROW_IF(sei_data_size > data.b() - sei_data_pos, Invalid);
      common::Data32 sei_data = common::Data32(data.data() + sei_data_pos, sei_data_size, nullptr);
      util::CaptionPayloadInfo info = util::CaptionHandler::ParsePayloadInfo(sei_data);
      if (info.valid && !info.byte_ranges.empty()) {
        has_caption = true;
        break;
      }
    }
    if (has_caption) {
      uint32_t sei_size = 0;
      for (const auto& range: sei_ranges) {
        sei_size += range.size;
      }
      uint32_t video_size = data.count() - sei_size;
      common::Data32 video_data = common::Data32(new uint8_t[video_size], video_size, [](uint8_t* p) { delete[] p; });
      uint32_t b = data.b();
      for (const auto& range: sei_ranges) {
        data.set_bounds(data.a(), range.pos);
        video_data.copy(data);
        video_data.set_bounds(video_data.b(), video_data.b());
        data.set_bounds(range.pos + range.size, b);
      }
      video_data.copy(data);
      video_data.set_bounds(0, video_size);
      return move(video_data);
    } else {
      return move(data);
    }
  };
  return Sample(sample.pts, sample.dts, sample.keyframe, SampleType::Video, nal, sample.byte_range.pos, sample.byte_range.size);
}

MP4::AudioTrack::AudioTrack(const std::shared_ptr<_MP4>& _mp4_this)
  : _this(_mp4_this) {}

MP4::AudioTrack::AudioTrack(const AudioTrack& audio_track)
  : functional::DirectAudio<AudioTrack, Sample>(audio_track.a(), audio_track.b()), _this(audio_track._this) {}

auto MP4::AudioTrack::duration() const -> uint64_t {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Audio).duration;
}

auto MP4::AudioTrack::edit_boxes() const -> const vector<common::EditBox>& {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Audio).edit_boxes;
}

auto MP4::AudioTrack::operator()(const uint32_t index) const -> Sample {
  THROW_IF(!_this->root.get(), Uninitialized);
  THROW_IF(index >= b(), OutOfRange);
  const SampleType type = SampleType::Audio;
  THROW_IF(index >= _this->tracks(type).sample_count, OutOfRange);
  if (settings::Audio::IsPCM(_this->audio.codec)) {
    return _this->audio.pcm_samples[index];
  } else {
    CHECK(_this->tracks(type).timestamps);
    const lsmash_media_ts_t& media_ts = _this->tracks(type).timestamps.get()[index];
    THROW_IF(media_ts.cts > std::numeric_limits<int64_t>::max() || media_ts.dts > std::numeric_limits<int64_t>::max(), Unsupported);
    int64_t pts = media_ts.cts;
    int64_t dts = media_ts.dts;
    lsmash_sample_t sample;
    lsmash_get_sample_info_from_media_timeline(_this->root.get(), _this->tracks(type).track_ID, index + 1, &sample);
    uint32_t pos = (uint32_t)sample.pos;
    uint32_t size = sample.length;
    lsmash_sample_property_t sample_property;
    lsmash_get_sample_property_from_media_timeline(_this->root.get(), _this->tracks(type).track_ID, index + 1, &sample_property);
    bool keyframe = sample_property.ra_flags & ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC;
    auto nal = [_this = _this, index]() -> common::Data32 {
      lsmash_sample_t* sample = lsmash_get_sample_from_media_timeline(_this->root.get(), _this->tracks(type).track_ID, index + 1);
      CHECK(sample);
      return common::Data32(sample->data, sample->length, [sample](void* p) {
        lsmash_delete_sample(sample);
      });
    };
    return Sample(pts, dts, keyframe, type, nal, pos, size);
  }
}

MP4::CaptionTrack::CaptionTrack(const std::shared_ptr<_MP4>& _mp4_this)
  : _this(_mp4_this) {}

MP4::CaptionTrack::CaptionTrack(const CaptionTrack& caption_track)
  : functional::DirectCaption<CaptionTrack, Sample>(caption_track.a(), caption_track.b()), _this(caption_track._this) {}

auto MP4::CaptionTrack::duration() const -> uint64_t {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Caption).duration;
}

auto MP4::CaptionTrack::edit_boxes() const -> const vector<common::EditBox>& {
  THROW_IF(!_this->root.get(), Uninitialized);
  return _this->tracks(SampleType::Caption).edit_boxes;
}

auto MP4::CaptionTrack::operator()(const uint32_t index) const -> Sample {
  THROW_IF(!_this->root.get(), Uninitialized);
  THROW_IF(index >= b(), OutOfRange);
  auto sample = _this->video_sample(index);
  auto nal = [_this = _this, sample]() -> common::Data32 {
    common::Data32 data = sample.nal();
    vector<ByteRange> sei_ranges = _this->get_sei_ranges(data);
    uint32_t sei_size = 0;
    for (const auto& range: sei_ranges) {
      sei_size += range.size;
    }
    common::Data32 caption_data = common::Data32(new uint8_t[sei_size], sei_size, [](uint8_t* p) { delete[] p; });
    bool has_caption = false;
    uint32_t output_size = 0;
    for (const auto& range: sei_ranges) {
      uint32_t sei_data_pos = data.a() + range.pos + _this->nalu_length_size;
      uint32_t sei_data_size = range.size - _this->nalu_length_size;
      THROW_IF(sei_data_size > data.b() - sei_data_pos, Invalid);
      common::Data32 sei_data = common::Data32(data.data() + sei_data_pos, sei_data_size, nullptr);
      util::CaptionPayloadInfo info = util::CaptionHandler::ParsePayloadInfo(sei_data);
      CHECK(info.valid);
      if (!info.byte_ranges.empty()) {
        uint32_t current_sei_size = util::CaptionHandler::CopyPayloadsIntoData(sei_data, info, _this->nalu_length_size, caption_data);
        output_size += current_sei_size;
      }
      caption_data.set_bounds(output_size, output_size);
    }

    has_caption = output_size != 0;
    caption_data.set_bounds(0, output_size);
    return has_caption ? move(caption_data) : move(common::Data32());
  };
  return Sample(sample.pts, sample.dts, sample.keyframe, SampleType::Caption, nal, sample.byte_range.pos, sample.byte_range.size);
}

}}}
