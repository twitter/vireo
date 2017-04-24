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
#include "vireo/internal/decode/avcc.h"
#include "vireo/internal/decode/types.h"
#include "vireo/internal/decode/util.h"
#include "vireo/mux/mp2ts.h"
#include "vireo/util/caption.h"
#include "vireo/version.h"

CONSTRUCTOR static void _Init() {
#if TWITTER_INTERNAL
  extern AVOutputFormat ff_mpegts_muxer;
  av_register_output_format(&ff_mpegts_muxer);
#else
  av_register_all();
#endif
}

const static uint32_t kTimescale = 90000;  // default timescale for MPEG-TS
const static uint8_t kNumTracks = 2;

namespace vireo {
namespace mux {

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
  common::Data16 aac_payload;
  AVCodec* codec = nullptr;
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
  _MP2TS(common::Data16&& aac_payload, uint8_t nalu_length_size, common::Data16&& sps_pps)
    : aac_payload(move(aac_payload)), nalu_length_size(nalu_length_size), sps_pps(move(sps_pps)) {
  }
  void mux(const encode::Sample& sample) {
    THROW_IF(!initialized, Uninitialized);
    THROW_IF(sample.nal.count() >= security::kMaxSampleSize, Unsafe);
    THROW_IF(tracks(SampleType::Audio).num_frames >= security::kMaxSampleCount, Unsafe);
    THROW_IF(tracks(SampleType::Video).num_frames >= security::kMaxSampleCount, Unsafe);
    THROW_IF(sample.type != SampleType::Audio && sample.type != SampleType::Video, Unsupported);

    AVStream* stream = format_context->streams[tracks(sample.type).track_ID];
    CHECK(stream);
    CHECK(stream->codec->time_base.den != 0);
    int64_t pts = sample.pts * kTimescale * stream->codec->time_base.num / stream->codec->time_base.den;
    int64_t dts = sample.dts * kTimescale * stream->codec->time_base.num / stream->codec->time_base.den;

    AVPacket packet;
    av_init_packet(&packet);

    const uint8_t* nal_data = sample.nal.data();
    CHECK(nal_data);
    if (sample.type == SampleType::Video) {
      auto video_data_annexb = internal::decode::avcc_to_annexb(sample.nal, nalu_length_size);
      uint32_t packet_size = sample.keyframe ? sps_pps.count() + video_data_annexb.count() : video_data_annexb.count();

      uint32_t out_offset = 0;
      int32_t caption_index = -1;
      common::Data32 caption_data_annexb;
      util::PtsIndexPair pts_index(sample.pts, 0); // only pts value is used when searching for the index
      auto caption_pts_and_index = lower_bound(caption_pts_index_pairs.begin(), caption_pts_index_pairs.end(), pts_index);
      if (caption_pts_and_index != caption_pts_index_pairs.end()) {
        caption_index = caption_pts_and_index->index;
      }
      if (caption_index >= 0) {
        auto caption_sample = caption(caption_index);

        if (caption_sample.nal.count()) { // caption size could be 0 in case there is no caption data associated with the corresponding video sample
          caption_data_annexb = internal::decode::avcc_to_annexb(caption_sample.nal, nalu_length_size);
          packet_size += caption_data_annexb.count();
        }
      }

      av_new_packet(&packet, packet_size);

      // add SPS/PPS before each keyframe
      if (sample.keyframe) {
        memcpy((void*)(packet.data + out_offset), (void*)sps_pps.data(), sps_pps.count());
        out_offset += sps_pps.count();
      }

      // insert caption data
      if (caption_data_annexb.count()) {
        memcpy((void*)(packet.data + out_offset), caption_data_annexb.data(), caption_data_annexb.count());
        out_offset += caption_data_annexb.count();
      }

      // insert video frame data
      memcpy((void*)(packet.data + out_offset), video_data_annexb.data(), video_data_annexb.count());
    } else {
      CHECK(sample.type == SampleType::Audio);
      const uint32_t data_size = sample.nal.count();
      const uint32_t frame_size = data_size + 7;
      uint32_t offset = 0;
      av_new_packet(&packet, frame_size);

      // ADTS Header (http://wiki.multimedia.cx/index.php?title=ADTS)
      packet.data[offset++] = 0b11111111;
      packet.data[offset++] = 0b11110001;
      packet.data[offset++] = (uint8_t)((audio_object_type - 1) << 6) | (uint8_t)(sample_rate_index << 2) | (uint8_t)(channel_configuration & 0x4);
      packet.data[offset++] = (uint8_t)((channel_configuration & 0x3) << 6) | frame_size >> 11;
      packet.data[offset++] = (uint8_t)((frame_size >> 3) & 0xff);
      packet.data[offset++] = (uint8_t)((frame_size << 5) & 0xe0) | 0b00011111;
      packet.data[offset++] = 0b11111100;
      memcpy((void*)(packet.data + offset), (void*)nal_data, data_size);
    }

    packet.pts = pts;
    packet.dts = dts;
    packet.flags = sample.keyframe ? AV_PKT_FLAG_KEY : 0;
    packet.stream_index = tracks(sample.type).track_ID;

    CHECK(av_write_frame(format_context.get(), &packet) == 0);
    av_packet_unref(&packet);

    ++tracks(sample.type).num_frames;
  }
  void flush() {
    THROW_IF(!initialized, Uninitialized);
    if (!finalized) {
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

  auto aac_payload_padded = common::Data16::None;
  if (audio.settings().sample_rate != 0) {
    const auto extradata = audio.settings().as_extradata(settings::Audio::ExtraDataType::aac);
    const uint16_t padded_size = (size_t)extradata.count() + FF_INPUT_BUFFER_PADDING_SIZE;
    common::Data16 extradata_padded = { (const uint8_t*)calloc(padded_size, sizeof(uint8_t)), padded_size, [](uint8_t* p) { free(p); } };
    extradata_padded.copy(extradata);
    aac_payload_padded = move(extradata_padded);
  }

  _this.reset(new _MP2TS(move(aac_payload_padded),
                         video.settings().sps_pps.nalu_length_size,
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
