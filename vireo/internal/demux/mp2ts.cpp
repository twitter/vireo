//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "lsmash.h"
#include "lsmash-h264.h"
}
#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/reader.h"
#include "vireo/common/security.h"
#include "vireo/common/util.h"
#include "vireo/constants.h"
#include "vireo/decode/types.h"
#include "vireo/error/error.h"
#include "vireo/header/header.h"
#include "vireo/internal/decode/annexb.h"
#include "vireo/internal/decode/types.h"
#include "vireo/internal/demux/mp2ts.h"
#include "vireo/util/caption.h"

const static uint8_t kNaluLengthSize = 4;
const static uint8_t kNumTracks = 4;

CONSTRUCTOR static void _Init() {
#ifdef TWITTER_INTERNAL
  extern AVInputFormat ff_mpegts_demuxer;
  av_register_input_format(&ff_mpegts_demuxer);
#else
  av_register_all();
#endif
}

namespace vireo {
namespace internal {
namespace demux {

using namespace decode;

static const uint32_t kSize_Buffer = 4 * 1024 * 1024;
static const uint16_t kMaxMP2TSSampleCount = 0x1000;  // Avoid using excessive memory

struct MP2TSSample {
  vector<common::Data32> contents;  // if the data is too large, it's actually wrapped in multiple packets
  uint32_t pts;
  uint32_t dts;
  bool keyframe;
};

struct ADTSHeader {
  settings::Audio::Codec codec;
  uint32_t sample_rate;
  uint8_t channels;
  uint32_t header_size;  // size of the header
  uint32_t data_size;  // size of the audio sample
  bool valid;  // false, if we aren't able to extract complete ADTS from PES
};

struct _MP2TS {
  common::Reader reader;
  common::Data32 iobuffer = common::Data32((uint8_t*)av_malloc(kSize_Buffer + FF_INPUT_BUFFER_PADDING_SIZE), kSize_Buffer + FF_INPUT_BUFFER_PADDING_SIZE, [](uint8_t*p){ av_free(p); });
  unique_ptr<AVFormatContext, function<void(AVFormatContext*)>> format_context = { nullptr, [](AVFormatContext* p) {
    if (p->pb) {
      av_free(p->pb);
      p->pb = nullptr;
    }
    avformat_close_input(&p);
    avformat_free_context(p);
  }};

  struct Track {
    bool initialized = false;
    uint32_t index = numeric_limits<uint32_t>::max();
    uint32_t timescale = 0;
    uint64_t duration = 0;
    vector<MP2TSSample> samples;
    vector<uint32_t> dts_offsets_per_packet;
  };

  class {
    Track _tracks[kNumTracks];
  public:
    auto operator()(const SampleType type) -> Track& {
      uint32_t i = type - SampleType::Video;
      THROW_IF(i >= kNumTracks, OutOfRange);
      return _tracks[i];
    };
  } tracks;

  struct PacketCache {
    common::Data32 packet_data = common::Data32(new uint8_t[kSize_Buffer], kSize_Buffer, [](uint8_t*p){ delete[] p; });
    int64_t packet_pts = AV_NOPTS_VALUE;
    int64_t packet_dts = AV_NOPTS_VALUE;

    void clear() {
      packet_data.set_bounds(0, 0);
      packet_pts = AV_NOPTS_VALUE;
      packet_dts = AV_NOPTS_VALUE;
    }

    bool empty() {
      return packet_data.count() == 0;
    }

    void cache_new_packet(int64_t pts, int64_t dts, const common::Data32& new_data) {
      clear();
      packet_data.copy(new_data);
      packet_pts = pts;
      packet_dts = dts;
    }

    void pad_data_to_cached_packet(const common::Data32& new_data) {
      packet_data.set_bounds(packet_data.b(), packet_data.b());
      packet_data.copy(new_data);
      packet_data.set_bounds(0, packet_data.b());
    }
  };

  struct {
    uint16_t width = 0;
    uint16_t height = 0;
    settings::Video::Codec codec = settings::Video::Codec::Unknown;
    vector<header::SPS_PPS> sps_pps;
    vector<common::Data16> sps_pps_extradatas; // same as sps_pps, just processed via as_extradata
    PacketCache cache;
  } video;

  struct {
    settings::Audio::Codec codec = settings::Audio::Codec::Unknown;
    uint32_t sample_rate = 0;
    uint8_t channels = 0;
    bool multiple_samples_per_packet = false;
    vector<uint32_t> samples_per_packet;
    PacketCache cache;
  } audio;

  struct {
    settings::Data::Codec codec = settings::Data::Codec::Unknown;
  } data;

  struct {
    settings::Caption::Codec codec = settings::Caption::Codec::Unknown;
  } caption;

  _MP2TS(common::Reader&& reader) : reader(move(reader)) {}

  static ADTSHeader ParseADTSHeader(const common::Data32& packet_data) {
    // Returns true if the entire ADTS packet is inside packet_data. Otherwise,
    // some part of the ADTS is in the next PES packet.
    // http://wiki.multimedia.cx/index.php?title=ADTS
    ADTSHeader header;
    header.valid = false;
    const uint8_t* bytes = packet_data.data() + packet_data.a();
    uint32_t offset = 0;
    if (packet_data.count() < 7) {
      return header;
    }
    THROW_IF(bytes[offset] != 0xFF, Invalid);  // syncword 0xFFF
    offset++;
    THROW_IF((bytes[offset] & 0xF0) != 0xF0, Invalid);
    const bool mpeg_version = (bytes[offset] & 0b00001000);  // 0: MPEG-4, 1: MPEG-2
    THROW_IF(mpeg_version != 0, Unsupported);
    THROW_IF((bytes[offset] & 0b00000110) != 0, Invalid);  // layer (always 0)
    const bool protection_absent = (bytes[offset] & 0b00000001);  // 0: CRC present, 1: no CRC
    offset++;
    const uint8_t audio_object_type = ((bytes[offset] & 0b11000000) >> 6) + 1;  // 1: AAC-Main 2: AAC-LC
    switch (audio_object_type) {
      case 1:
        header.codec = settings::Audio::Codec::AAC_Main;
        break;
      case 2:
        header.codec = settings::Audio::Codec::AAC_LC;
        break;
      default:
        header.codec = settings::Audio::Codec::Unknown;
    }
    const uint8_t sampling_frequency_index = ((bytes[offset] & 0b00111100) >> 2);  // 13 supported frequencies: [0-12]
    THROW_IF(sampling_frequency_index < 0 || sampling_frequency_index > 12, Invalid);
    header.sample_rate = kSampleRate[sampling_frequency_index];
    const uint8_t channel_configuration = ((bytes[offset] & 0b00000001) << 2) + ((bytes[offset + 1] & 0b11000000) >> 6);  // 1: mono, 2: stereo
    THROW_IF(channel_configuration != 1 && channel_configuration != 2, Unsupported);
    header.channels = channel_configuration;
    offset++;
    const uint32_t frame_length = ((uint32_t)(bytes[offset] & 0b00000011) << 11) + ((uint32_t)bytes[offset + 1] << 3) + ((bytes[offset + 2] & 0b11100000) >> 5);
    if (frame_length > packet_data.count()) {
      return header;
    }
    offset += 3;
    const uint8_t num_aac_frames = (bytes[offset] & 0b00000011) + 1;
    THROW_IF(num_aac_frames != 1, Unsupported);  // we expect 1 AAC frame per ADTS frame
    offset++;
    if (!protection_absent) {
      offset += 2;  // CRC
    }
    THROW_IF(frame_length <= offset, Invalid);
    header.header_size = offset;
    header.data_size = frame_length - header.header_size;
    header.valid = true;
    return header;
  }

  int32_t aud_offset(const common::Data32& packet) {
    common::Data32 _packet = common::Data32(packet.data() + packet.a(), packet.count(), nullptr);
    while (_packet.count()) {
      uint32_t start_code_prefix_size = ANNEXB<H264NalType>::StartCodePrefixSize(_packet);
      if (start_code_prefix_size) {
        _packet.set_bounds(_packet.a() + start_code_prefix_size, _packet.b());
        H264NalType type = ANNEXB<H264NalType>::GetNalType(_packet);
        if (type == H264NalType::AUD) {
          return packet.a() + _packet.a() - start_code_prefix_size;
        }
      }
      _packet.set_bounds(_packet.a() + 1, _packet.b());
    }
    return -1;
  }

  void process_h264_packet(const AVPacket& packet) {
    // H.264 nal unit in Annex-B format
    auto pts = packet.pts;
    auto dts = packet.dts;
    auto packet_data = common::Data32(packet.data, packet.size, nullptr);
    int32_t offset = aud_offset(packet_data);
    if (offset == 0) {
      // Packet starts with new frame data
      CHECK(video.cache.packet_data.count() == 0);
      THROW_IF(pts == AV_NOPTS_VALUE || dts == AV_NOPTS_VALUE, Invalid, "PES packet doesn't contain a valid timestamp");
      process_h264_frame(pts, dts, packet_data);
    } else {
      // There is data that belongs to previous frame
      CHECK(tracks(SampleType::Video).samples.size());
      auto& sample = tracks(SampleType::Video).samples.back();
      if (offset < 0) {
        // All data belongs to the previous sample
        if (pts != AV_NOPTS_VALUE && dts != AV_NOPTS_VALUE) {
          THROW_IF(pts != sample.pts || dts != sample.dts, Invalid, "PES packet contains an invalid timestamp");
        } else {
          THROW_IF(pts != AV_NOPTS_VALUE || dts != AV_NOPTS_VALUE, Invalid, "PES packet contains an invalid timestamp");
        }
        pad_data_to_previous_h264_frame(packet_data);
      } else {
        // There is new frame data as well as data that belongs to previous sample
        THROW_IF(pts == AV_NOPTS_VALUE || dts == AV_NOPTS_VALUE, Invalid, "PES packet doesn't contain a valid timestamp");
        common::Data32 head_packet_data = common::Data32(packet_data.data() + packet_data.a(), offset, nullptr);
        if (video.cache.packet_data.count() == 0) {
          // All data from previous packet is already processed
          THROW_IF(pts == sample.pts || dts == sample.dts, Invalid, "PES packet contains an invalid timestamp");
          pad_data_to_previous_h264_frame(head_packet_data);
        } else {
          // There is cached data that needs to be processed before processing new frame data
          THROW_IF(pts == video.cache.packet_pts || dts == video.cache.packet_dts, Invalid, "PES packet contains an invalid timestamp");
          video.cache.pad_data_to_cached_packet(head_packet_data);
          process_h264_frame(video.cache.packet_pts, video.cache.packet_dts, video.cache.packet_data);
        }
        common::Data32 tail_packet_data = common::Data32(packet_data.data() + packet_data.a() + offset, packet_data.count() - offset, nullptr);
        process_h264_frame(pts, dts, tail_packet_data);
      }
    }
  }

  void pad_data_to_previous_h264_frame(const common::Data32& packet_data) {
    CHECK(tracks(SampleType::Video).samples.size());
    auto& sample = tracks(SampleType::Video).samples.back();
    sample.contents.push_back(packet_data);
    video.cache.clear();
  }

  void process_h264_frame(int64_t pts, int64_t dts, const common::Data32& packet_data) {
    // H.264 nal unit in Annex-B format
    bool keyframe = false;
    CHECK(ANNEXB<H264NalType>::StartCodePrefixSize(packet_data));
    ANNEXB<H264NalType> annexb_parser(packet_data);

    vector<common::Data32> caption_contents;
    bool frame_data_found = false;
    for (uint32_t index = 0; index < annexb_parser.b(); index++) {
      NalInfo<H264NalType> info = annexb_parser(index);

      // Check if there is an SPS + PPS
      if (info.type == H264NalType::SPS) {
        // Found an SPS
        common::Data16 packet_sps = common::Data16(packet_data.data() + info.byte_offset, info.size, nullptr);

        // Parse width/height from SPS
        uint16_t packet_width = 0;
        uint16_t packet_height = 0;
        if (!tracks(SampleType::Video).initialized) {
          h264_info_t h264_info;
          THROW_IF(h264_setup_parser(&h264_info, 1) != 0, Invalid);
          THROW_IF(h264_parse_sps(&h264_info, h264_info.buffer.rbsp, (uint8_t*)(packet_sps.data() + 1), packet_sps.count() - 1) != 0, Invalid);
          h264_cleanup_parser(&h264_info);
          packet_width = h264_info.sps.cropped_width;
          packet_height = h264_info.sps.cropped_height;
        }

        // After SPS, expect a PPS
        index++;
        if (index >= annexb_parser.count()) {
          CHECK(ANNEXB<H264NalType>::StartCodePrefixSize(packet_data));
          video.cache.cache_new_packet(pts, dts, packet_data);
          return;
        }
        info = annexb_parser(index);
        THROW_IF(info.type != H264NalType::PPS, Invalid);
        common::Data16 packet_pps = common::Data16(packet_data.data() + info.byte_offset, info.size, nullptr);
        auto sps_pps = header::SPS_PPS(packet_sps, packet_pps, kNaluLengthSize);
        auto sps_pps_extradata = sps_pps.as_extradata(header::SPS_PPS::ExtraDataType::annex_b);

        // Save all unique SPS / PPS
        if (video.sps_pps_extradatas.empty() || video.sps_pps_extradatas.back() != sps_pps_extradata) {
          video.sps_pps.push_back(sps_pps);
          video.sps_pps_extradatas.push_back(sps_pps_extradata);
        }

        // Mark track as initialized
        if (!tracks(SampleType::Video).initialized) {
          video.width = packet_width;
          video.height = packet_height;
          THROW_IF(!security::valid_dimensions(video.width, video.height), Unsafe);
          tracks(SampleType::Video).initialized = true;
        }
      }

      if (info.type == decode::H264NalType::SEI) {
        common::Data32 data = common::Data32(packet_data.data() + info.byte_offset, info.size, nullptr);
        common::Data32 caption_data = common::Data32(new uint8_t[info.size + info.start_code_prefix_size], info.size + info.start_code_prefix_size, [](uint8_t* p) { delete[] p; });
        vector<ByteRange> caption_ranges = util::get_caption_ranges(data);
        if (!caption_ranges.empty()) {
          // copy start code
          uint32_t caption_size = util::copy_caption_payloads_to_caption_data(data, caption_data, caption_ranges, info.start_code_prefix_size);
          common::Data32 start_code = common::Data32(packet_data.data() + info.byte_offset - info.start_code_prefix_size, info.start_code_prefix_size, nullptr);
          caption_data.set_bounds(0, info.start_code_prefix_size);
          caption_data.copy(start_code);
          caption_data.set_bounds(0, caption_size);
          caption_contents.push_back(caption_data);
        }
      }

      // Find the first frame data, check if it is a keyframe
      // Save the packet contents starting from the first frame data
      if (!frame_data_found && (info.type == decode::H264NalType::IDR || info.type == decode::H264NalType::FRM)) {
        frame_data_found = true;
        keyframe = (info.type == H264NalType::IDR) ? true : false;
        vector<common::Data32> contents;
        const uint32_t byte_offset = info.byte_offset - info.start_code_prefix_size;
        THROW_IF(byte_offset < 0, Invalid);
        common::Data32 packet_video = common::Data32(packet_data.data() + byte_offset, packet_data.b() - byte_offset, nullptr);
        CHECK(packet_video.count());

        if (keyframe) {
          CHECK(video.sps_pps_extradatas.size() > 0);
          const auto& sps_pps_extradata = video.sps_pps_extradatas.back();
          contents.emplace_back(sps_pps_extradata.data() + sps_pps_extradata.a(), sps_pps_extradata.count(), nullptr);
        }
        contents.push_back(packet_video);

        if (tracks(SampleType::Video).samples.size()) {
          int64_t prev_dts = tracks(SampleType::Video).samples.back().dts;
          THROW_IF(dts < prev_dts, Invalid);
          tracks(SampleType::Video).dts_offsets_per_packet.push_back((uint32_t)(dts - prev_dts));
        }
        tracks(SampleType::Video).samples.push_back({ contents, (uint32_t)pts, (uint32_t)dts, keyframe });
        THROW_IF(tracks(SampleType::Video).samples.size() >= kMaxMP2TSSampleCount, Unsafe);
      }
    }

    if (!frame_data_found) {
      CHECK(ANNEXB<H264NalType>::StartCodePrefixSize(packet_data));
      video.cache.cache_new_packet(pts, dts, packet_data);
      return;
    }

    if (!caption_contents.empty()) {
      tracks(SampleType::Caption).initialized = true;
      caption.codec = settings::Caption::Codec::Unknown;
      tracks(SampleType::Caption).timescale = tracks(SampleType::Video).timescale;
    }

    if (tracks(SampleType::Video).dts_offsets_per_packet.size()) {
      tracks(SampleType::Caption).dts_offsets_per_packet.push_back(tracks(SampleType::Video).dts_offsets_per_packet.back());
    }
    tracks(SampleType::Caption).samples.push_back({ caption_contents, (uint32_t)pts, (uint32_t)dts, true });
    THROW_IF(tracks(SampleType::Caption).samples.size() >= kMaxMP2TSSampleCount, Unsafe);

    video.cache.clear();
  }

  uint32_t process_adts_packet(int64_t pts, int64_t dts, common::Data32& packet_data) {
    // Processes a single ADTS packet from packet_data, if possible.
    // Returns the number of bytes processed from packet_data,
    // and adjusts packet_data's boundaries with the data processed.
    ADTSHeader header = ParseADTSHeader(packet_data);
    if (!header.valid) {
      return 0;
    }

    // Save AAC settings from first packet and ensure all packets match
    if (tracks(SampleType::Audio).initialized) {
      THROW_IF(header.codec != audio.codec, Invalid);
      THROW_IF(header.sample_rate != audio.sample_rate, Invalid);
      THROW_IF(header.channels != audio.channels, Invalid);
    } else {
      audio.codec = header.codec;
      audio.sample_rate = header.sample_rate;
      audio.channels = header.channels;
      tracks(SampleType::Audio).initialized = true;
    }

    // Save the packet contents
    vector<common::Data32> contents;
    uint32_t packet_b = packet_data.b();
    packet_data.set_bounds(packet_data.a() + header.header_size, packet_data.a() + header.header_size + header.data_size);
    // copy-construct a copy of packet_data inside contents
    contents.emplace_back(packet_data);
    // packet_data.a() is already shifted by header.header_size
    packet_data.set_bounds(packet_data.a() + header.data_size, packet_b);

    // Save the dts offset between packets (starting from 2nd packet) for duration calculation and PTS/DTS adjustment
    if (tracks(SampleType::Audio).samples.size()) {
      int64_t prev_dts = tracks(SampleType::Audio).samples.back().dts;
      if (dts > prev_dts) {
        tracks(SampleType::Audio).dts_offsets_per_packet.push_back((uint32_t)(dts - prev_dts));
      }
    }

    // Save the sample
    tracks(SampleType::Audio).samples.push_back({ contents, (uint32_t)pts, (uint32_t)dts, true });
    THROW_IF(tracks(SampleType::Audio).samples.size() >= kMaxMP2TSSampleCount, Unsafe);
    return header.header_size + header.data_size;
  }

  void process_aac_packet(AVPacket& packet) {
    // AAC samples in ADTS frames (1 AAC sample per ADTS frame)
    int64_t pts = packet.pts;
    int64_t dts = packet.dts;
    common::Data32 packet_data = common::Data32(packet.data, packet.size, nullptr);

    if (!audio.cache.empty()) {
      // Some of the beginning of this packet belongs to previous ADTS frame.
      // Audio PES packets are usually small, especially when using this
      // optimisation - no problem to copy.
      uint32_t cached_bytes = audio.cache.packet_data.count();
      audio.cache.pad_data_to_cached_packet(packet_data);
      uint32_t processed_bytes = process_adts_packet(audio.cache.packet_pts, audio.cache.packet_dts, audio.cache.packet_data);
      THROW_IF(processed_bytes == 0, Unsupported, "Unable to finish cached audio packet on next PES");
      packet_data.set_bounds(packet_data.a() + processed_bytes - cached_bytes,
                             packet_data.b());
      audio.cache.clear();
    }

    // Packet may contain multiple ADTS frames
    uint32_t num_samples = 0;
    while (packet_data.count()) {
      uint32_t processed_bytes = process_adts_packet(pts, dts, packet_data);
      num_samples++;
      if (processed_bytes == 0) {
        // Reached the end. Cache.
        audio.cache.cache_new_packet(pts, dts, packet_data);
        break;
      }
    }
    // num_samples can be 0 if we only continue the previous ADTS in this PES
    if (num_samples > 1) {
      audio.multiple_samples_per_packet = true;
    }
    if (num_samples > 0) {
      audio.samples_per_packet.push_back(num_samples);
    }
  }

  void process_timed_id3_packet(AVPacket& packet) {
    // timed id3, we return the whole payload without parsing
    THROW_IF(tracks(SampleType::Data).samples.size() >= kMaxMP2TSSampleCount, Unsafe);
    int64_t pts = packet.pts;
    int64_t dts = packet.dts;
    common::Data32 packet_data = common::Data32(packet.data, packet.size, nullptr);
    if (tracks(SampleType::Data).samples.size()) {
      int64_t prev_dts = tracks(SampleType::Data).samples.back().dts;
      THROW_IF(dts < prev_dts, Invalid);
      tracks(SampleType::Data).dts_offsets_per_packet.push_back((uint32_t)(dts - prev_dts));
    }
    tracks(SampleType::Data).samples.push_back({ vector<common::Data32>({ move(packet_data) }), (uint32_t)pts, (uint32_t)dts, true });
    tracks(SampleType::Data).initialized = true;
  }

  bool finish_initialization() {
    if (format_context == nullptr) {
      return false;
    }
    // Gather stream specific info
    const uint32_t num_streams = format_context->nb_streams;
    for (uint32_t stream_index = 0; stream_index < num_streams; ++stream_index) {
      AVStream* stream = format_context->streams[stream_index];
      AVCodecContext* codec_context = stream->codec;
      SampleType type = SampleType::Unknown;
      CHECK(codec_context);
      if (codec_context->codec_type == AVMEDIA_TYPE_VIDEO) {
        type = SampleType::Video;
      } else if (codec_context->codec_type == AVMEDIA_TYPE_AUDIO) {
        type = SampleType::Audio;
      } else if (codec_context->codec_type == AVMEDIA_TYPE_DATA) {
        type = SampleType::Data;
      } else {  // AVMEDIA_TYPE_UNKNOWN
        continue;
      }

      tracks(type).index = stream_index;
      THROW_IF(stream->time_base.num != 1, Unsupported);
      tracks(type).timescale = stream->time_base.den;

      if (type == SampleType::Video) {
        if (codec_context->codec_id == AV_CODEC_ID_H264) {
          video.codec = settings::Video::Codec::H264;
        } else {
          THROW_IF(codec_context->codec_id != AV_CODEC_ID_H264, Unsupported);
        }
      } else if (type == SampleType::Audio) {
        if (codec_context->codec_id == AV_CODEC_ID_AAC) {
          audio.codec = settings::Audio::Codec::AAC_Main;  // At this point we only know it's AAC, we don't know the actual profile
        } else {
          THROW_IF(codec_context->codec_id != AV_CODEC_ID_AAC, Unsupported);
        }
      } else {
        CHECK(type == SampleType::Data);
        if (codec_context->codec_id == AV_CODEC_ID_TIMED_ID3) {
          data.codec = settings::Data::Codec::TimedID3;
        }
      }
    }

    // Parse packets
    audio.cache.clear();
    video.cache.clear();
    AVPacket packet;
    while (av_read_frame(format_context.get(), &packet) >= 0) {
      SampleType type = SampleType::Unknown;
      uint32_t stream_index = packet.stream_index;
      if (stream_index == tracks(SampleType::Video).index) {
        type = SampleType::Video;
      } else if (stream_index == tracks(SampleType::Audio).index) {
        type = SampleType::Audio;
      } else if (stream_index == tracks(SampleType::Data).index) {
        type = SampleType::Data;
      }

      if (type == SampleType::Video) {
        if (video.codec == settings::Video::Codec::H264) {
          process_h264_packet(packet);
        }
      } else if (type == SampleType::Audio) {
        if (audio.codec == settings::Audio::Codec::AAC_Main ||
            audio.codec == settings::Audio::Codec::AAC_LC) {
          process_aac_packet(packet);
        }
      } else if (type == SampleType::Data) {
        process_timed_id3_packet(packet);
      }
      av_packet_unref(&packet);
    }

    // Calculate duration of the tracks from the parsed packets
    for (auto type: enumeration::Enum<SampleType>(SampleType::Video, SampleType::Caption)) {
      if (tracks(type).dts_offsets_per_packet.size()) {
        vector<uint32_t> dts_offsets_per_sample;
        if (type == SampleType::Audio) {
          CHECK(audio.samples_per_packet.size() == tracks(type).dts_offsets_per_packet.size() + 1);
        }
        for (uint32_t index = 0; index < tracks(type).dts_offsets_per_packet.size(); ++index) {
          uint32_t dts_offset_per_packet = tracks(type).dts_offsets_per_packet[index];
          uint32_t samples_per_packet = (type == SampleType::Audio) ? audio.samples_per_packet[index] : 1;
          uint32_t dts_offset_per_sample = common::round_divide(tracks(type).dts_offsets_per_packet[index], (uint32_t)1, samples_per_packet);
          tracks(type).duration += dts_offset_per_packet;
          for (uint32_t i = 0; i < samples_per_packet; ++i) {
            dts_offsets_per_sample.push_back(dts_offset_per_sample);
          }
        }
        uint64_t last_dts_offset;
        if (type == SampleType::Audio) {
          last_dts_offset = common::round_divide<uint64_t>((uint64_t) kMP2TSTimescale * AUDIO_FRAME_SIZE, audio.samples_per_packet.back(), audio.sample_rate);
        } else {
          last_dts_offset = common::median(dts_offsets_per_sample);
        }
        tracks(type).duration += last_dts_offset;
      }
    }

    // In case there were multiple audio samples per packet, PTS/DTS values have to be adjusted
    if (audio.multiple_samples_per_packet) {
      int32_t index = -1;
      uint64_t num_samples = 0;
      uint64_t current_sample = 0;
      uint32_t start_pts = 0;
      uint32_t start_dts = 0;
      for (auto& sample: tracks(SampleType::Audio).samples) {
        if (current_sample == num_samples) {
          index++;
          num_samples = audio.samples_per_packet[index];
          current_sample = 0;
          start_pts = sample.pts;
          start_dts = sample.dts;
        } else {
          int64_t dts_offset_this_sample = (int64_t) kMP2TSTimescale * current_sample * AUDIO_FRAME_SIZE / audio.sample_rate;
          CHECK(dts_offset_this_sample);
          sample.pts = (uint32_t)(start_pts + dts_offset_this_sample);
          sample.dts = (uint32_t)(start_dts + dts_offset_this_sample);
        }
        current_sample++;
      }
    }

    return true;
  }
};

MP2TS::MP2TS(common::Reader&& reader)
  : _this(make_shared<_MP2TS>(move(reader))), audio_track(_this), video_track(_this), data_track(_this), caption_track(_this) {
  AVInputFormat* format = av_find_input_format("mpegts");
  THROW_IF(format == nullptr, Invalid);
  AVFormatContext* format_context = avformat_alloc_context();
  format_context->pb = avio_alloc_context((uint8_t*)_this->iobuffer.data(), kSize_Buffer, 0, (void*)_this->reader.opaque, _this->reader.read_callback, nullptr, _this->reader.seek_callback);
  THROW_IF(format_context->pb == nullptr, OutOfMemory);
  THROW_IF(avformat_open_input(&format_context, "", format, nullptr) != 0, Invalid);
  _this->format_context.reset(format_context);
  av_log_set_level(AV_LOG_ERROR);  // disable ffmpeg warning messages

  if (_this->finish_initialization()) {
    video_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Video).samples.size());
    audio_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Audio).samples.size());
    data_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Data).samples.size());
    caption_track.set_bounds(0, (uint32_t)_this->tracks(SampleType::Caption).samples.size());

    if (_this->tracks(SampleType::Video).initialized) {
      CHECK(_this->video.sps_pps.size() > 0);
      video_track._settings = (settings::Video){
        _this->video.codec,
        _this->video.width,
        _this->video.height,
        _this->tracks(SampleType::Video).timescale,
        settings::Video::Orientation::Landscape,
        _this->video.sps_pps.front()
      };
    }
    if (_this->tracks(SampleType::Audio).initialized) {
      audio_track._settings = (settings::Audio){
        _this->audio.codec,
        _this->tracks(SampleType::Audio).timescale,
        _this->audio.sample_rate,
        _this->audio.channels,
        0
      };
    }
    if (_this->tracks(SampleType::Data).initialized) {
      data_track._settings = (settings::Data){
        _this->data.codec,
        _this->tracks(SampleType::Data).timescale
      };
    }
    if (_this->tracks(SampleType::Caption).initialized) {
      caption_track._settings = (settings::Caption){
        _this->caption.codec,
        _this->tracks(SampleType::Caption).timescale
      };
    }
  } else {
    THROW_IF(true, Uninitialized);
  }
}

MP2TS::MP2TS(MP2TS&& mp2ts)
  : audio_track(_this), video_track(_this), data_track(_this), caption_track(_this) {
  _this = mp2ts._this;
  mp2ts._this = nullptr;
}

MP2TS::VideoTrack::VideoTrack(const std::shared_ptr<_MP2TS>& _mp2ts_this)
  : _this(_mp2ts_this) {
}

MP2TS::VideoTrack::VideoTrack(const VideoTrack& video_track)
  : functional::DirectVideo<VideoTrack, Sample>(video_track.a(), video_track.b()), _this(video_track._this) {
}

auto MP2TS::VideoTrack::duration() const -> uint64_t {
  return _this->tracks(SampleType::Video).duration;
}

auto MP2TS::VideoTrack::fps() const -> float {
  return duration() ? (float)count() / duration() * settings().timescale : 0.0f;
}

auto MP2TS::VideoTrack::operator()(uint32_t index) const -> Sample {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  THROW_IF(!_this->tracks(SampleType::Video).initialized, Invalid);
  MP2TSSample sample = _this->tracks(SampleType::Video).samples[index];
  THROW_IF(sample.contents.size() == 0, Invalid);

  auto nal = [_this = _this, index]() -> common::Data32 {
    MP2TSSample sample = _this->tracks(SampleType::Video).samples[index];
    if (sample.contents.size() == 1) {
      // no need to copy any data
      common::Data32 nal = sample.contents.back();
      annexb_to_avcc(nal, kNaluLengthSize);
      return move(nal);
    } else {
      // join all the contents within packet
      uint32_t size = 0;
      for (auto& content: sample.contents) {
        size += content.count();
      }
      common::Data32 nal = common::Data32(new uint8_t[size], size, [](uint8_t* p) { delete[] p; });
      nal.set_bounds(0, 0);
      for (auto& content: sample.contents) {
        nal.copy(content);
        nal.set_bounds(nal.b(), nal.b());
      }
      nal.set_bounds(0, nal.b());
      annexb_to_avcc(nal, kNaluLengthSize);
      return move(nal);
    }
  };
  return Sample(sample.pts, sample.dts, sample.keyframe, SampleType::Video, nal);
}

MP2TS::AudioTrack::AudioTrack(const std::shared_ptr<_MP2TS>& _mp2ts_this)
  :  _this(_mp2ts_this) {
}

MP2TS::AudioTrack::AudioTrack(const AudioTrack& audio_track)
  : functional::DirectAudio<AudioTrack, Sample>(audio_track.a(), audio_track.b()), _this(audio_track._this) {
}

auto MP2TS::AudioTrack::duration() const -> uint64_t {
  if (_this->tracks(SampleType::Audio).duration) {
    CHECK(_this->audio.sample_rate);
    CHECK(_this->tracks(SampleType::Audio).timescale);
    return _this->tracks(SampleType::Audio).duration;
  } else if (_this->tracks(SampleType::Audio).samples.size() == 1) {
    return 1;
  } else {
    return 0;
  }
}

auto MP2TS::AudioTrack::operator()(uint32_t index) const -> Sample {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  THROW_IF(!_this->tracks(SampleType::Audio).initialized, Invalid);
  MP2TSSample sample = _this->tracks(SampleType::Audio).samples[index];
  THROW_IF(sample.contents.size() != 1, Invalid);

  auto nal = [_this = _this, index]() -> common::Data32 {
    MP2TSSample sample = _this->tracks(SampleType::Audio).samples[index];
    CHECK(sample.contents.size() == 1);
    common::Data32 nal = sample.contents.back();
    return move(nal);
  };
  return Sample(sample.pts, sample.dts, sample.keyframe, SampleType::Audio, nal);
}

MP2TS::DataTrack::DataTrack(const std::shared_ptr<_MP2TS>& _mp2ts_this)
  : _this(_mp2ts_this) {}

MP2TS::DataTrack::DataTrack(const DataTrack& data_track)
  : functional::DirectData<DataTrack, Sample>(data_track.a(), data_track.b()), _this(data_track._this) {}

auto MP2TS::DataTrack::operator()(uint32_t index) const -> Sample {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);
  THROW_IF(!_this->tracks(SampleType::Data).initialized, Invalid);
  MP2TSSample sample = _this->tracks(SampleType::Data).samples[index];
  THROW_IF(sample.contents.size() != 1, Invalid);

  bool keyframe = sample.keyframe;
  THROW_IF(!index && !keyframe, Invalid);
  auto nal = [_this = _this, index]() -> common::Data32 {
    MP2TSSample sample = _this->tracks(SampleType::Data).samples[index];
    CHECK(sample.contents.size() == 1);
    common::Data32 nal = sample.contents.back();
    return move(nal);
  };
  return Sample(sample.pts, sample.dts, keyframe, SampleType::Data, nal);
}

MP2TS::CaptionTrack::CaptionTrack(const std::shared_ptr<_MP2TS>& _mp2ts_this)  : _this(_mp2ts_this) {}

MP2TS::CaptionTrack::CaptionTrack(const CaptionTrack& caption_track)
  : functional::DirectCaption<CaptionTrack, Sample>(caption_track.a(), caption_track.b()), _this(caption_track._this) {}

auto MP2TS::CaptionTrack::duration() const -> uint64_t {
  return _this->tracks(SampleType::Caption).duration;
}

auto MP2TS::CaptionTrack::operator()(uint32_t index) const -> Sample {
  THROW_IF(index <  a(), OutOfRange);
  THROW_IF(index >= b(), OutOfRange);

  MP2TSSample sample = _this->tracks(SampleType::Caption).samples[index];
  auto nal = [_this = _this, index]() -> common::Data32 {
    MP2TSSample sample = _this->tracks(SampleType::Caption).samples[index];
    if (sample.contents.empty()) {
      return common::Data32();
    } else {
      CHECK(sample.contents.size() == 1)
      // no need to copy any data
      common::Data32 nal = sample.contents.back();
      annexb_to_avcc(nal, kNaluLengthSize);
      return move(nal);
    }
  };
  return Sample(sample.pts, sample.dts, sample.keyframe, SampleType::Caption, nal);
}

}}}
