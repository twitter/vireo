//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
}
#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/util.h"
#include "vireo/internal/decode/annexb.h"
#include "vireo/internal/decode/avcc.h"
#include "vireo/internal/decode/types.h"
#include "vireo/mux/mp2ts.h"
#include "vireo/util/caption.h"
#include "vireo/version.h"

CONSTRUCTOR static void _Init() {
#ifdef TWITTER_INTERNAL
  extern AVOutputFormat ff_mpegts_muxer;
  av_register_output_format(&ff_mpegts_muxer);
#else
  av_register_all();
#endif
}

const static uint8_t kNumTracks = 2;

namespace vireo {
namespace mux {

// TS packet size minus mandatory TS header size
const static uint32_t kTSPayloadSize = MP2TS_PACKET_LENGTH - 4;
const static uint32_t kNALUDelimiterSize = 6;

struct _MP2TS {
  static const size_t kSize_Buffer = 4 * 1024;
  static const size_t kSize_Default = 512 * 1024;
  unique_ptr<AVFormatContext, function<void(AVFormatContext*)>> format_context = { nullptr, [](AVFormatContext* p) {
    if (p->pb) {
      av_free(p->pb);
      p->pb = nullptr;
    }
    avformat_free_context(p);
  }};
  unique_ptr<common::Data32> movie = nullptr;
  common::Data16 buffer = common::Data16((uint8_t*)av_malloc(kSize_Buffer), kSize_Buffer, av_free);

  uint8_t nalu_length_size;
  common::Data16 sps_pps;
  uint16_t audio_object_type = 0;
  uint8_t channel_configuration = 0;
  uint8_t sample_rate_index = 0xf0;
  AVCodec* codec = nullptr;

  class VideoPacker {
    std::vector<common::Data32> cached_data;
    common::Data32 nalu_aud = common::Data32(new uint8_t[kNALUDelimiterSize], kNALUDelimiterSize, [](uint8_t* p){ delete[] p; });
    uint32_t remaining_frames;

  public:
    void init(uint32_t expected_frames) {
      remaining_frames = expected_frames;
      uint8_t* bytes = (uint8_t*)nalu_aud.data();
      uint32_t offset = 0;
      bytes[offset++] = 0x00;
      bytes[offset++] = 0x00;
      bytes[offset++] = 0x00;
      bytes[offset++] = 0x01;
      bytes[offset++] = 0x09;
      bytes[offset++] = 0xf0;
      nalu_aud.set_bounds(0, offset);
      CHECK(nalu_aud.count() == kNALUDelimiterSize);
      cached_data.clear();
    }

    std::vector<common::Data32> cache_and_flush(int64_t pts, int64_t dts, bool keyframe,
                                                const std::vector<common::Data32>& video_frame) {
      // This function caches some of the data in video_frame (maybe none), and
      // returns a vector containing data for a PES packet. You must use the
      // return value of this function before calling cache_and_flush() again.

      remaining_frames--;
      std::vector<common::Data32> result_packet;

      // Clear cached_data, and move cached contents to result.
      result_packet.swap(cached_data);

      if (video_frame.empty()) {
        // Nothing to write. Exit early.
        return result_packet;
      }

      // Add NAL delimiter before each frame. If we don't add it explicitly,
      // libavformat would add it for us -- even for the first frame.
      result_packet.emplace_back(nalu_aud.data() + nalu_aud.a(),
                                 nalu_aud.count(), nullptr /* deleter */);

      // We might split the last video_frame item. Safe to return the rest.
      for (uint32_t i = 0; i < video_frame.size() - 1; ++i) {
        result_packet.emplace_back(video_frame[i].data() + video_frame[i].a(),
                                   video_frame[i].count(), nullptr /* deleter */);
      }

      // Calculate packet size of all data, if we wouldn't split.
      uint32_t desired_packet_size = 0;
      for (const auto& data: result_packet) {
        desired_packet_size += data.count();
      }
      desired_packet_size += video_frame.back().count();

      // Compute mandatory overhead for starting a PES.
      uint32_t overhead = 3;  // PES start code prefix
      overhead += 1;  // PES stream ID
      overhead += 2;  // PES packet length;
      overhead += 2;  // flags
      overhead += 1;  // PES header data length field.
      CHECK(pts != AV_NOPTS_VALUE);
      overhead += 5;
      CHECK(dts != AV_NOPTS_VALUE);
      if (dts != pts) {
        overhead += 5;
      }
      if (keyframe) {
        // Adaptation field. On a keyframe, libavformat sets the PCR, and it
        // sets Random Access Indicator bit, telling us we can decode without
        // errors from this point.
        //
        overhead += 8;
      }
      const uint32_t max_overhead = 3 + 1 + 2 + 2 + 1 + 5 + 5 + 8;

      // It only makes sense to end this packet on a TS packet end. Otherwise,
      // we wouldn't save space.
      uint32_t num_bytes_to_cache = (desired_packet_size + overhead) % kTSPayloadSize;
      bool split_frame = false;
      // We only split if the packet is more than the payload size,
      // and we can safely start a new packet afterwards.
      // We need to be able to at least put the NALU delimiter for the next
      // frame.
      if (desired_packet_size + overhead > kTSPayloadSize &&
          num_bytes_to_cache + max_overhead + kNALUDelimiterSize <= kTSPayloadSize &&
          remaining_frames > 0) {
        split_frame = true;
      }
      if (!split_frame) {
        num_bytes_to_cache = 0;
      }

      common::Data32 frame_data_h264_annexb(
          video_frame.back().data() + video_frame.back().a(),
          video_frame.back().count(), nullptr /* deleter */);
      internal::decode::ANNEXB<internal::decode::H264NalType> annexb_parser(frame_data_h264_annexb);
      if (annexb_parser.count() == 0) {
        // Do not cache if we can't recognise the video data format.
        num_bytes_to_cache = 0;
      } else {
        const uint32_t last_nal_offset = annexb_parser(annexb_parser.count() - 1).byte_offset;
        // We must split after the last nal because the PTS & DTS from the
        // PES apply to the first NAL unit inside this PES - this must be the
        // NAL unit delimiter for the next frame.
        //
        CHECK(frame_data_h264_annexb.count() >= last_nal_offset);
        if (frame_data_h264_annexb.count() - last_nal_offset < num_bytes_to_cache) {
          num_bytes_to_cache = 0;
        }
      }

      frame_data_h264_annexb.set_bounds(frame_data_h264_annexb.a(),
                                        frame_data_h264_annexb.b() - num_bytes_to_cache);
      result_packet.emplace_back(frame_data_h264_annexb.data() + frame_data_h264_annexb.a(),
                                 frame_data_h264_annexb.count(), nullptr /* deleter */);

      if (num_bytes_to_cache > 0) {
        // Copy the last num_bytes_to_cache bytes from frame_data_h264_annexb
        // to data cache.
        frame_data_h264_annexb.set_bounds(frame_data_h264_annexb.b(),
                                          frame_data_h264_annexb.b() + num_bytes_to_cache);
        // Copy-construct data to be cached.
        cached_data.emplace_back(frame_data_h264_annexb);
      }
      return result_packet;
    }
  } video_packer;

  class ADTSPacker {
    // Maximum number of TS packets that we are allowed to use to represent a
    // single concatenated sequence of ADTS frames.
    // Apple's mediafilesegmenter and FFmpeg both use a similar value for this.
    const static uint32_t kMaxPacketsToPack = 20;
    // start code prefix(3 bytes) + stream ID(1) + packet length(2) +
    // flags(2) + header length(1) +
    // PTS(5) + DTS(5 bytes. Only present if PTS != DTS)
    const static uint32_t kMaxPESOverhead = 3 + 1 + 2 + 2 + 1 + 5 + 5;
    // If we fill up kBufferSize bytes of data, we will send exactly
    // kMaxPacketsToPack TS packets.
    // Note: with this calculation, we waste 5 bytes if PTS == DTS for each
    // kMaxPacketsToPack TS packets.
    const static uint32_t kBufferSize = kTSPayloadSize * kMaxPacketsToPack - kMaxPESOverhead;

    common::Data32 buffer = common::Data32(new uint8_t[kBufferSize], kBufferSize, [](uint8_t* p){ delete[] p; });
    int64_t first_pts;
    int64_t first_dts;
    uint32_t frames_in_buffer = 0;
    uint32_t remaining_frames;

  public:
    void init(uint32_t expected_frames) {
      remaining_frames = expected_frames;
      reset();
    }

    void reset() {
      buffer.set_bounds(0, 0);
      frames_in_buffer = 0;
    }

    bool empty() {
      return frames_in_buffer == 0;
    }

    void set_ts(int64_t pts, int64_t dts) {
      first_pts = pts;
      first_dts = dts;
    }

    int64_t get_first_pts() {
      return first_pts;
    }

    int64_t get_first_dts() {
      return first_dts;
    }

    bool cached_last_frame() {
      return remaining_frames == 0;
    }

    bool can_cache(int64_t pts, int64_t dts, int32_t sample_rate, const common::Data32& aac_frame) {
      const uint32_t adts_frame_size = aac_frame.count() + 7;
      THROW_IF(adts_frame_size > ADTSPacker::kBufferSize, Unsupported, "Audio bitrate is too high");

      // We only send PTS and DTS information about the first audio frame in
      // a packed sequence of frames. Check that we can correctly predict the
      // actual PTS and DTS of all packed frames based on sampling rate.
      //
      int64_t timestamp_change = (int64_t) kMP2TSTimescale * frames_in_buffer * AUDIO_FRAME_SIZE / sample_rate;
      int64_t predicted_pts = first_pts + timestamp_change;
      int64_t predicted_dts = first_dts + timestamp_change;
      // Even if the predicted PTS and DTS are off by a small value, we won't notice.
      //
      return (buffer.count() + adts_frame_size <= ADTSPacker::kBufferSize &&
          std::abs(predicted_pts - pts) <= MP2TS_PTS_ALLOWED_DRIFT &&
          std::abs(predicted_dts - dts) <= MP2TS_PTS_ALLOWED_DRIFT);
    }

    void cache(const common::Data32& nal, uint16_t audio_object_type,
               uint8_t channel_configuration, uint8_t sample_rate_index) {
      // This function appends an ADTS frame corresponding to nal to memory
      // that starts at buffer.b()
      // This function preserves buffer.a(), but changes buffer.b().
      THROW_IF(remaining_frames == 0, Unsafe, "More cache calls than expected");
      remaining_frames--;
      const uint32_t buffer_a = buffer.a();
      const uint32_t data_size = nal.count();
      const uint32_t frame_size = data_size + 7;
      // ADTS Header (http://wiki.multimedia.cx/index.php?title=ADTS)
      uint8_t* bytes = (uint8_t*)buffer.data() + buffer.b();
      uint32_t offset = 0;
      bytes[offset++] = 0b11111111;
      bytes[offset++] = 0b11110001;
      bytes[offset++] = (uint8_t)((audio_object_type - 1) << 6) | (uint8_t)(sample_rate_index << 2) | (uint8_t)(channel_configuration & 0x4);
      bytes[offset++] = (uint8_t)((channel_configuration & 0x3) << 6) | frame_size >> 11;
      bytes[offset++] = (uint8_t)((frame_size >> 3) & 0xff);
      bytes[offset++] = (uint8_t)((frame_size << 5) & 0xe0) | 0b00011111;
      bytes[offset++] = 0b11111100;
      buffer.set_bounds(buffer.b() + offset, buffer.b() + offset);
      buffer.copy(nal);
      buffer.set_bounds(buffer_a, buffer.b());
      frames_in_buffer++;
    }

    std::vector<common::Data32> flush() {
      // You must use the vector returned by flush() before the next call to cache()!

      // Since we wait for a number of AAC frames to arrive, the output TS file
      // will not have the audio and video frames nicely ordered by PTS.
      // This is not a problem, but it means that we might be able to start
      // playback a little earlier (before file download is finished)
      // if we would keep the audio and video frames ordered by PTS.
      //
      // libavformat dumps a PES as a contiguous block. Ideally, we would want
      // to interleave audio and video PES packets on the TS level.
      if (!frames_in_buffer) {
        reset();
        return std::vector<common::Data32>();
      }
      // Create packet_data here to make sure we don't make a copy.
      // emplace_back will create a Data32 object with the contents of
      // adts_packer.buffer. adts_packer.buffer is still responsible for memory
      // clean up. Make sure it lives longer than packet_data.
      std::vector<common::Data32> packet_data;
      packet_data.emplace_back(buffer.data() + buffer.a(), buffer.count(), nullptr /* deleter */);
      reset();
      return packet_data;
    }
  } adts_packer;
  struct Track {
    uint32_t timescale;
    uint64_t num_frames = 0;
    uint32_t track_ID = 0;
    int64_t start_offset = 0;
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
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
  functional::Caption<encode::Sample> caption;
  vector<util::PtsIndexPair> caption_pts_index_pairs;
  bool initialized = false;
  bool finalized = false;
  _MP2TS(uint8_t nalu_length_size, common::Data16&& sps_pps)
    : nalu_length_size(nalu_length_size), sps_pps(move(sps_pps)) {}
  void write_avpacket(int64_t packet_pts, int64_t packet_dts, bool keyframe, uint32_t track_ID,
      const std::vector<common::Data32>& packet_data) {

    CHECK(packet_data.size() > 0);

    uint32_t packet_size = 0;
    for (const auto& data: packet_data) {
      packet_size += data.count();
    }

    AVPacket packet;
    av_init_packet(&packet);
    av_new_packet(&packet, packet_size);

    auto out = common::Data32(packet.data, packet_size, nullptr /* deleter */);
    out.set_bounds(0, 0);
    for (const auto& data: packet_data) {
      out.set_bounds(out.b(), out.b());
      out.copy(data);
    }
    out.set_bounds(0, out.b());
    CHECK(out.count() == packet_size);

    packet.pts = packet_pts;
    packet.dts = packet_dts;
    packet.flags = keyframe ? AV_PKT_FLAG_KEY : 0;
    packet.stream_index = track_ID;

    if (track_ID == tracks(SampleType::Video).track_ID) {
      // For video, we get a warning from libavformat that tells us the packet
      // doesn't start with H.264 start code. That is correct - the packet
      // might start with data for the previuous frame.
      // See https://github.com/FFmpeg/FFmpeg/blob/e7282674a505d548746a4734cbe902a9f242eb6b/libavformat/mpegtsenc.c#L1435
      // for details.
      //
      int log_level = av_log_get_level();
      av_log_set_level(AV_LOG_ERROR);
      CHECK(av_write_frame(format_context.get(), &packet) == 0);
      av_log_set_level(log_level);
    } else {
      CHECK(av_write_frame(format_context.get(), &packet) == 0);
    }
    av_packet_unref(&packet);
  }
  void mux_video(const encode::Sample& sample) {
    THROW_IF(sample.type != SampleType::Video, Unsupported);

    std::vector<common::Data32> video_packet;
    // add SPS/PPS before each keyframe
    if (sample.keyframe && !internal::decode::contain_sps_pps(sample.nal, nalu_length_size)) {
      // Convert sps_pps from Data16 to Data32. emplace_back will create a
      // Data32 object with the contents of sps_pps.
      // sps_pps is still responsible for memory clean up. Make sure it lives
      // longer than video_packet.
      video_packet.emplace_back(sps_pps.data() + sps_pps.a(),
                                sps_pps.count(), nullptr /* deleter */);
    }

    // insert caption data
    int32_t caption_index = -1;
    util::PtsIndexPair pts_index(sample.pts, 0); // only pts value is used when searching for the index
    auto caption_pts_and_index = lower_bound(caption_pts_index_pairs.begin(), caption_pts_index_pairs.end(), pts_index);
    if (caption_pts_and_index != caption_pts_index_pairs.end()) {
      caption_index = caption_pts_and_index->index;
    }
    if (caption_index >= 0) {
      auto caption_sample = caption(caption_index);

      if (caption_sample.nal.count()) { // caption size could be 0 in case there is no caption data associated with the corresponding video sample
        // Move version of push_back is used here.
        video_packet.push_back(internal::decode::avcc_to_annexb(caption_sample.nal, nalu_length_size));
      }
    }

    // insert video frame data. Move version of push_back is used here.
    video_packet.push_back(internal::decode::avcc_to_annexb(sample.nal, nalu_length_size));

    AVStream* stream = format_context->streams[tracks(sample.type).track_ID];
    CHECK(stream);
    CHECK(stream->codec->time_base.den != 0);
    int64_t pts = sample.pts * kMP2TSTimescale * stream->codec->time_base.num / stream->codec->time_base.den;
    int64_t dts = sample.dts * kMP2TSTimescale * stream->codec->time_base.num / stream->codec->time_base.den;

    write_avpacket(pts, dts, sample.keyframe, tracks(sample.type).track_ID,
                   video_packer.cache_and_flush(pts, dts, sample.keyframe, video_packet));
  }
  void mux_audio(const encode::Sample& sample) {
    THROW_IF(sample.type != SampleType::Audio, Unsupported);

    AVStream* stream = format_context->streams[tracks(sample.type).track_ID];
    CHECK(stream);
    CHECK(stream->codec->time_base.den != 0);
    int64_t pts = sample.pts * kMP2TSTimescale * stream->codec->time_base.num / stream->codec->time_base.den;
    int64_t dts = sample.dts * kMP2TSTimescale * stream->codec->time_base.num / stream->codec->time_base.den;

    if (adts_packer.empty()) {
      // Start packing ADTS frames.
      adts_packer.reset();
      adts_packer.set_ts(pts, dts);
    }

    if (!adts_packer.can_cache(pts, dts, stream->codec->sample_rate, sample.nal)) {
      // Start over.
      write_avpacket(adts_packer.get_first_pts(), adts_packer.get_first_dts(),
          false /* keyframe */, tracks(sample.type).track_ID, adts_packer.flush());
      adts_packer.set_ts(pts, dts);
    }
    adts_packer.cache(sample.nal, audio_object_type, channel_configuration, sample_rate_index);
    if (adts_packer.cached_last_frame()) {
      write_avpacket(adts_packer.get_first_pts(), adts_packer.get_first_dts(),
          false /* keyframe */, tracks(sample.type).track_ID, adts_packer.flush());
    }
  }
  void mux(const encode::Sample& sample) {
    THROW_IF(!initialized, Uninitialized);
    THROW_IF(sample.nal.count() >= security::kMaxSampleSize, Unsafe);
    THROW_IF(tracks(SampleType::Audio).num_frames >= security::kMaxSampleCount, Unsafe);
    THROW_IF(tracks(SampleType::Video).num_frames >= security::kMaxSampleCount, Unsafe);
    THROW_IF(sample.type != SampleType::Audio && sample.type != SampleType::Video, Unsupported);

    if (sample.type == SampleType::Video) {
      mux_video(sample);
    } else {
      mux_audio(sample);
    }

    ++tracks(sample.type).num_frames;
  }
  void flush() {
    THROW_IF(!initialized, Uninitialized);
    if (!finalized) {
      video_packer.init(video.count());
      adts_packer.init(audio.count());
      order_samples(tracks(SampleType::Audio).timescale, audio,
                    tracks(SampleType::Video).timescale, video,
                    [this](const encode::Sample& sample) {
                      mux(sample);
                    }
      );

      CHECK(av_write_trailer(format_context.get()) == 0);
      movie->set_bounds(0, movie->b());
      finalized = true;
    }
  }
};

MP2TS::MP2TS(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption) {
  THROW_IF(!audio.count() && !video.count(), InvalidArguments);
  THROW_IF(video.settings().timescale == 0 && audio.settings().sample_rate == 0.0, InvalidArguments);
  THROW_IF(video.settings().sps_pps.sps.count() >= security::kMaxHeaderSize, Unsafe);
  THROW_IF(video.settings().sps_pps.pps.count() >= security::kMaxHeaderSize, Unsafe);
  if (video.settings().timescale) {
    THROW_IF(video.settings().orientation != settings::Video::Orientation::Landscape, Unsupported);
    THROW_IF(!security::valid_dimensions(video.settings().width, video.settings().height), Unsafe);
  }

  _this.reset(new _MP2TS(video.settings().sps_pps.nalu_length_size,
                         video.settings().sps_pps.as_extradata(header::SPS_PPS::annex_b)));

  auto write_func = [] (void* opaque, uint8_t* buf, int size) -> int {
    if (size > security::kMaxWriteSize) {
      return 0;
    }
    _MP2TS* _this = (_MP2TS*)opaque;
    if (!_this->movie || size + _this->movie->a() > _this->movie->capacity()) {
      const uint32_t new_capacity = _this->movie ? _this->movie->capacity() + common::align_divide(size + _this->movie->a() - _this->movie->capacity(), (uint32_t)_MP2TS::kSize_Default) : common::align_divide(size, (int)_MP2TS::kSize_Default);
      common::Data32* new_data = new common::Data32(new uint8_t[new_capacity], new_capacity, [](uint8_t* p) { delete[] p; });
      THROW_IF(!new_data->data(), OutOfMemory);
      if (_this->movie) {
        new_data->set_bounds(_this->movie->a(), _this->movie->b());
        memcpy((uint8_t*)new_data->data(), _this->movie->data(), _this->movie->b());
      } else {
        new_data->set_bounds(0, 0);
      }
      _this->movie.reset(new_data);
    }
    memcpy((uint8_t*)_this->movie->data() + _this->movie->a(), buf, size);
    _this->movie->set_bounds(_this->movie->a() + (uint32_t)size,
                                   std::max(_this->movie->b(), _this->movie->a() + (uint32_t)size));
    return size;
  };
  auto seek_func = [] (void* opaque, int64_t offset, int whence) -> int64_t {
    _MP2TS* _this = (_MP2TS*)opaque;
    if (whence == SEEK_SET) {
      THROW_IF(offset < 0 || offset > _this->movie->b(), OutOfRange);
      _MP2TS* _this = (_MP2TS*)opaque;
      _this->movie->set_bounds((uint32_t)offset, _this->movie->b());
      return (int)offset;
    }
    return 0;
  };

  AVOutputFormat* format = av_guess_format("mpegts", nullptr, nullptr);
  CHECK(format);
  AVFormatContext* format_context = nullptr;
  CHECK(avformat_alloc_output_context2(&format_context, format, nullptr, nullptr) >= 0);
  _this->format_context.reset(format_context);
  _this->format_context->flags |= AVFMT_ALLOW_FLUSH;
  _this->format_context->oformat->flags |= AVFMT_TS_NONSTRICT;
  _this->format_context->pb = avio_alloc_context((uint8_t*)_this->buffer.data(), _this->buffer.count(), 1, _this.get(), nullptr, write_func, seek_func);
  CHECK(_this->format_context->pb);

  char title[] = "title";
  av_dict_set(&_this->format_context->metadata, title, VIREO_VERSION, 0);
  char service_provider[] = "service_provider";
  char vireo[] = "Vireo";
  av_dict_set(&_this->format_context->metadata, service_provider, vireo, 0);

  // Request fewer PAT/PMT packets - one per keyframe, and one at
  // the beginning.
  // The max allowed period between PAT packets is (INT_MAX / 2) - 1.
  //
  std::stringstream period;
  period << (INT_MAX / 2) - 1;

  av_opt_set(_this->format_context->priv_data, "mpegts_flags", "-resend_headers", 0);
  av_opt_set(_this->format_context->priv_data, "mpegts_flags", "-pat_pmt_at_frames", 0);
  av_opt_set(_this->format_context->priv_data, "pat_period", period.str().c_str(), 0);
  // SDT packets are only sent in DVB. We shouldn't be sending it anyway.
  //
  av_opt_set(_this->format_context->priv_data, "sdt_period", period.str().c_str(), 0);

  if (video.settings().timescale != 0) {
    AVStream* stream = avformat_new_stream(_this->format_context.get(), nullptr);
    stream->time_base = (AVRational){ 1, (int)video.settings().timescale };

    avcodec_get_context_defaults3(stream->codec, nullptr);
    stream->codec->codec_type = AVMEDIA_TYPE_VIDEO;
    stream->codec->codec_id = AV_CODEC_ID_H264;
    stream->codec->width = video.settings().width;
    stream->codec->height = video.settings().height;
    stream->codec->time_base = stream->time_base;

    uint32_t index = 0;
    for (const auto& sample: caption) {
      util::PtsIndexPair pts_index(sample.pts, index);
      _this->caption_pts_index_pairs.push_back(pts_index);
      index++;
    }
    sort(_this->caption_pts_index_pairs.begin(), _this->caption_pts_index_pairs.end());

    _this->tracks(SampleType::Video).track_ID = stream->index;
    _this->tracks(SampleType::Video).timescale = video.settings().timescale;
    _this->video = video;
    _this->caption = caption;
  }

  if (audio.settings().sample_rate != 0) {
    uint32_t sample_rate = audio.settings().sample_rate;
    AVStream* stream = avformat_new_stream(_this->format_context.get(), nullptr);
    stream->time_base = (AVRational){ 1, (int)audio.settings().timescale };

    avcodec_get_context_defaults3(stream->codec, nullptr);
    stream->codec->codec_type = AVMEDIA_TYPE_AUDIO;
    stream->codec->codec_id = AV_CODEC_ID_AAC;
    stream->codec->sample_rate = sample_rate;
    stream->codec->channels = audio.settings().channels;
    stream->codec->time_base = stream->time_base;

    _this->audio_object_type = 0;
    switch (audio.settings().codec) {
      case settings::Audio::Codec::AAC_Main:
        _this->audio_object_type = 1;
      case settings::Audio::Codec::AAC_LC:
      case settings::Audio::Codec::AAC_LC_SBR:  // No signaling for SBR at this point
        _this->audio_object_type = 2;
      default: break;
    }
    THROW_IF(!_this->audio_object_type, InvalidArguments);

    _this->channel_configuration = audio.settings().channels;
    THROW_IF(!_this->channel_configuration || _this->channel_configuration > 2, InvalidArguments);

    _this->sample_rate_index = find(kSampleRate.begin(), kSampleRate.end(), sample_rate) - kSampleRate.begin();
    THROW_IF(_this->sample_rate_index >= 13, InvalidArguments);

    _this->tracks(SampleType::Audio).track_ID = stream->index;
    _this->tracks(SampleType::Audio).timescale = audio.settings().timescale;
    _this->audio = audio;
  }

  CHECK(avformat_write_header(_this->format_context.get(), nullptr) == 0);
  _this->initialized = true;

  *static_cast<std::function<common::Data32(void)>*>(this) = [_this = _this]() {
    THROW_IF(!_this->initialized, Uninitialized);
    _this->flush();
    CHECK(_this->movie.get());
    return *_this->movie.get();
  };
}

MP2TS::MP2TS(MP2TS&& mp2ts)
  : Function<common::Data32>(*static_cast<const functional::Function<common::Data32>*>(&mp2ts)), _this(mp2ts._this) {
  mp2ts._this = nullptr;
}

}}
