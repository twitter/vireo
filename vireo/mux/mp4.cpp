//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

extern "C" {
#include "lsmash.h"
}
#include "vireo/base_cpp.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/math.h"
#include "vireo/common/ref.h"
#include "vireo/common/security.h"
#include "vireo/constants.h"
#include "vireo/encode/util.h"
#include "vireo/error/error.h"
#include "vireo/internal/decode/annexb.h"
#include "vireo/mux/mp4.h"
#include "vireo/types.h"
#include "vireo/util/caption.h"
#include "vireo/version.h"

const static uint8_t kNumTracks = 2;

namespace vireo {
namespace mux {

class MP4Creator {
  struct Track {
    uint32_t timescale;
    uint32_t media_timescale;
    uint64_t num_samples = 0;
    uint32_t sample_entry;
    uint32_t track_ID = 0;
    int64_t first_sample_dts = 0;
    int64_t prev_adjusted_dts = 0;
    int64_t last_dts_offset = 0;
    queue<int64_t> dts_offsets;  // dts_offsets is used only when we have cached_samples, otherwise last_dts_offset is all we need
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

  template <class F>
  void apply_on_tracks(F f) {
    for (auto type: enumeration::Enum<SampleType>(SampleType::Video, SampleType::Audio)) {
      if (tracks(type).track_ID) {
        f(type);
      }
    }
  }

  static const uint32_t kSize_Buffer = 4 * 1024 * 1024;
  static const uint32_t kSize_Default = 512 * 1024;
  lsmash_adhoc_remux_t moov_to_front = { kSize_Buffer, nullptr, nullptr };
  unique_ptr<lsmash_root_t, function<void(lsmash_root_t*)>> root = { nullptr, [&](lsmash_root_t* p) {
    apply_on_tracks([&](SampleType type) {
      lsmash_delete_track(p, tracks(type).track_ID);
    });
    lsmash_destroy_root(p);
  }};
  unique_ptr<lsmash_file_parameters_t> main_param;
  unique_ptr<lsmash_file_parameters_t> dash_data_param;
  FileFormat file_format;
  unique_ptr<lsmash_video_summary_t, function<void(lsmash_video_summary_t* p)>> video_summary = { nullptr, [](lsmash_video_summary_t* p) {
    lsmash_cleanup_summary((lsmash_summary_t*)p);
  }};
  unique_ptr<common::Data32> main_segment = nullptr;
  unique_ptr<common::Data32> dash_data_segment = nullptr;
  uint32_t movie_timescale;
  functional::Caption<encode::Sample> caption;
  vector<util::PtsIndexPair> caption_pts_index_pairs;
  bool initialized = false;
  bool enforce_strict_dts_ordering = false;
  bool is_dash = false;
  bool qt_compatible = false;
  struct CachedSample {
    lsmash_sample_t* ptr;
    SampleType type;
  };
  queue<CachedSample> cached_samples;  // caching is used only when enable_strict_dts_ordering is true, otherwise l-smash handles everything

  static int Write(unique_ptr<common::Data32>& data, uint8_t* buf, int size) {
    if (size > security::kMaxWriteSize) {
      return 0;
    }
    const uint32_t needed_size = data ? data->a() + size : size;
    const uint32_t current_capacity = data ? data->capacity() : 0;
    if (needed_size > current_capacity) {
      THROW_IF(current_capacity >= std::numeric_limits<uint32_t>::max() / 2, Unsafe);
      const uint32_t new_capacity = common::align_divide(std::max<uint32_t>(needed_size, current_capacity * 1.6), kSize_Default);
      common::Data32* new_data = new common::Data32(new uint8_t[new_capacity], new_capacity, [](uint8_t* p) { delete[] p; });
      THROW_IF(!new_data->data(), OutOfMemory);
      if (data) {
        new_data->set_bounds(data->a(), data->b());
        memcpy((uint8_t*)new_data->data(), data->data(), data->b());
      } else {
        new_data->set_bounds(0, 0);
      }
      data.reset(new_data);
    }
    memcpy((uint8_t*)data->data() + data->a(), buf, size);
    data->set_bounds(data->a() + (uint32_t)size, std::max(data->b(), data->a() + (uint32_t)size));
    return (int)size;
  };

  static int Read(common::Data32* data, uint8_t* buf, int size) {
    if (data->a() >= data->b()) {
      return 0;
    }
    const uint32_t read_size = std::min((uint32_t)size, data->b() - data->a());
    if (read_size) {
      memcpy(buf, (uint8_t*)data->data() + data->a(), read_size);
      data->set_bounds(data->a() + read_size, data->b());
    }
    return (int)read_size;
  };

  static int64_t Seek(common::Data32* data, int64_t offset, int whence) {
    if (whence == SEEK_SET) {
      THROW_IF(offset < 0 || offset > data->b(), OutOfRange);
      data->set_bounds((uint32_t)offset, data->b());
      return (int64_t)offset;
    }
    return 0;
  };

  void flush_cached_samples(bool force) {
    // manual sample caching is used only when enforce_strict_dts_ordering is true, otherwise l-smash handles everything
    THROW_IF(!enforce_strict_dts_ordering, Invalid);

    auto write_sample = [&](CachedSample sample, int64_t dts_offset) {
      THROW_IF(dts_offset > std::numeric_limits<uint32_t>::max(), Overflow);
      CHECK(lsmash_append_sample(root.get(), tracks(sample.type).track_ID, sample.ptr) == 0);
      CHECK(lsmash_flush_pooled_samples(root.get(), tracks(sample.type).track_ID, (uint32_t)dts_offset) == 0);
    };

    while (cached_samples.size()) {
      CachedSample sample = cached_samples.front();
      if (tracks(sample.type).dts_offsets.size()) {  // sample ready to flush
        const int64_t dts_offset = tracks(sample.type).dts_offsets.front();
        write_sample(sample, dts_offset);
        cached_samples.pop();
        tracks(sample.type).dts_offsets.pop();
      } else if (force) {  // last sample of the track so predict dts_offset from last_dts_offset
        int64_t last_dts_offset = tracks(sample.type).last_dts_offset;
        write_sample(sample, last_dts_offset ? last_dts_offset : 1);
        cached_samples.pop();
      } else {  // need another sample from the same track to get the dts_offset, not ready to be flushed
        break;
      }
    }
  }

  void append_sample(lsmash_sample_t* sample, SampleType type) {
    if (enforce_strict_dts_ordering) {
      // Avoid pooling in l-smash by caching and force flushing every sample in dts order
      // Pro: have a tight control over the exact ordering of interleaved audio / video samples
      // Con: a new chunk is created for every sample in stco box (marginally increased header size)
      cached_samples.push({ sample, type });
      if (tracks(type).num_samples) {
        tracks(type).dts_offsets.push(tracks(type).last_dts_offset);
      }
      flush_cached_samples(false);
    } else {
      CHECK(lsmash_append_sample(root.get(), tracks(type).track_ID, sample) == 0);
    }
  }

  void finalize_tracks() {
    if (!enforce_strict_dts_ordering) {
      apply_on_tracks([&](SampleType type) {
        if (tracks(type).track_ID) {
          THROW_IF(tracks(type).last_dts_offset > std::numeric_limits<uint32_t>::max(), Overflow);
          const uint32_t last_dts_offset = tracks(type).last_dts_offset ? (uint32_t)tracks(type).last_dts_offset : 1;
          CHECK(lsmash_flush_pooled_samples(root.get(), tracks(type).track_ID, last_dts_offset) == 0);
        }
      });
    } else {
      flush_cached_samples(true);
    }
  };

  void mux(const encode::Sample& sample) {
    THROW_IF(file_format == DashInitializer, Invalid);  // dash init segment does not contain sample information
    THROW_IF(!initialized, Uninitialized);
    THROW_IF(sample.nal.count() >= 0x400000, Unsafe);

    // calculate PTS / DTS values as well as dts offset (unless first sample in track)
    const int64_t sample_dts = sample.dts;
    const int64_t sample_pts = sample.pts;
    bool first_sample = tracks(sample.type).num_samples == 0;
    if (first_sample) {
      tracks(sample.type).first_sample_dts = sample_dts;
    }
    CHECK(sample_dts >= tracks(sample.type).first_sample_dts);
    CHECK(sample_pts >= tracks(sample.type).first_sample_dts);

    int64_t adjusted_dts = is_dash ? sample_dts : sample_dts - tracks(sample.type).first_sample_dts;
    if (!first_sample) {
      // dts offset can only be calculated once another sample from the same track is available
      // dts offset has to be made available to l-smash to finalize the state of previous sample before appending a new sample
      // thus track[kSampleTypeToTrack(sample.type, false)].last_dts_offset needs to be set prior to calling append_sample for sample with index > 0
      THROW_IF(adjusted_dts <= tracks(sample.type).prev_adjusted_dts, Invalid);
      tracks(sample.type).last_dts_offset = adjusted_dts - tracks(sample.type).prev_adjusted_dts;
    }

    // create and append sample to track
    const bool write_data = (file_format != FileFormat::HeaderOnly);
    lsmash_sample_t* lsmash_sample = nullptr;
    uint64_t offset = 0;
    uint32_t caption_size = 0;
    int32_t caption_index = -1;
    uint32_t lsmash_sample_size = sample.nal.count();

    if (sample.type == SampleType::Video) {
      util::PtsIndexPair pts_index(sample.pts, 0); // only pts value is used when searching for the index
      auto caption_pts_and_index = lower_bound(caption_pts_index_pairs.begin(), caption_pts_index_pairs.end(), pts_index);
      if (caption_pts_and_index != caption_pts_index_pairs.end()) {
        caption_index = caption_pts_and_index->index;
      }

      if (caption_index >= 0) {
        auto caption_sample = caption(caption_index);
        caption_size = caption_sample.nal.count();
        lsmash_sample_size += caption_size;
      }
    }

#if TWITTER_INTERNAL
    lsmash_sample = write_data ? lsmash_create_sample(lsmash_sample_size) : lsmash_create_no_data_sample(lsmash_sample_size);
#else
    lsmash_sample = lsmash_create_sample(lsmash_sample_size);
#endif
    THROW_IF(!lsmash_sample, OutOfMemory);

    if (write_data) {
      THROW_IF(!sample.nal.data(), Invalid);
      THROW_IF(sample.nal.a(), Unsupported);
      if (sample.type == SampleType::Video && caption_size) {
        auto caption_sample = caption(caption_index);
        uint8_t* caption_data = (uint8_t*)caption_sample.nal.data();
        memcpy(lsmash_sample->data + offset, caption_data, caption_size);
        offset += caption_size;
      }
      memcpy(lsmash_sample->data + offset, sample.nal.data(), sample.nal.count());
    }
    lsmash_sample->dts = adjusted_dts;
    lsmash_sample->cts = sample_pts;
    lsmash_sample->index = tracks(sample.type).sample_entry;
    lsmash_sample->prop.ra_flags = sample.keyframe ? ISOM_SAMPLE_RANDOM_ACCESS_FLAG_SYNC : ISOM_SAMPLE_RANDOM_ACCESS_FLAG_NONE;
    append_sample(lsmash_sample, sample.type);

    // update internal state for upcoming samples
    tracks(sample.type).num_samples++;
    tracks(sample.type).prev_adjusted_dts = adjusted_dts;
  }

  void mux(const common::EditBox& edit_box) {
    THROW_IF(file_format != Regular, Unsupported); // unknown behaviour for other file types, hence not supported
    THROW_IF(!initialized, Uninitialized);
    THROW_IF(edit_box.type == SampleType::Caption, Unsupported, "edit box in caption track is not supported");
    THROW_IF(tracks(edit_box.type).track_ID == 0, InvalidArguments);
    lsmash_edit_t edit;
    THROW_IF(edit_box.start_pts != ISOM_EDIT_MODE_EMPTY && edit_box.start_pts > std::numeric_limits<int64_t>::max(), Overflow);
    edit.start_time = (edit_box.start_pts == ISOM_EDIT_MODE_EMPTY) ? ISOM_EDIT_MODE_EMPTY : edit_box.start_pts;
    THROW_IF(edit_box.duration_pts > std::numeric_limits<uint64_t>::max() / movie_timescale, Overflow);
    edit.duration = edit_box.duration_pts * movie_timescale / tracks(edit_box.type).media_timescale;
    edit.rate = ISOM_EDIT_MODE_NORMAL;
    lsmash_create_explicit_timeline_map(root.get(), tracks(edit_box.type).track_ID, edit);
  }

  void mux(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption, const vector<common::EditBox>& edit_boxes) {
    THROW_IF(audio.count() >= security::kMaxSampleCount, Unsafe);
    THROW_IF(video.count() >= security::kMaxSampleCount, Unsafe);

    this->caption = caption;

    uint32_t index = 0;
    for (const auto& sample: caption) {
      util::PtsIndexPair pts_index(sample.pts, index);
      this->caption_pts_index_pairs.push_back(pts_index);
      index++;
    }
    sort(this->caption_pts_index_pairs.begin(), this->caption_pts_index_pairs.end());

    for (auto edit_box: edit_boxes) {
      mux(edit_box);
    }

    if (file_format != DashInitializer) {
      order_samples(tracks(SampleType::Audio).timescale, audio,
                    tracks(SampleType::Video).timescale, video,
                    [this](const encode::Sample& sample) {
                      mux(sample);
                    });
      finalize_tracks();
    }

    CHECK(lsmash_finish_movie(root.get(), &moov_to_front) == 0);  // Remux moov to beginning to cover progressive download case

    if (file_format == DashData) {
      dash_data_segment->set_bounds(0, dash_data_segment->b());
    }
    main_segment->set_bounds(0, main_segment->b());
  }

  void setup_video_track(const settings::Video& video_settings) {
    lsmash_video_summary_t* video_summary = (lsmash_video_summary_t*)lsmash_create_summary(LSMASH_SUMMARY_TYPE_VIDEO);
    CHECK(video_summary);
    video_summary->sample_type = ISOM_CODEC_TYPE_AVC1_VIDEO;
    video_summary->width = video_settings.coded_width;
    video_summary->height = video_settings.coded_height;

    // Video track
    tracks(SampleType::Video).track_ID = lsmash_create_track(root.get(), ISOM_MEDIA_HANDLER_TYPE_VIDEO_TRACK);

    // Video track params
    lsmash_track_parameters_t track_parameters;
    lsmash_initialize_track_parameters(&track_parameters);
    track_parameters.mode = (lsmash_track_mode)(ISOM_TRACK_ENABLED | ISOM_TRACK_IN_MOVIE | ISOM_TRACK_IN_PREVIEW);
    track_parameters.display_width = video_settings.width << 16;
    track_parameters.display_height = video_settings.height << 16;

    THROW_IF(video_settings.par_width <= 0 || video_settings.par_width >= 0x8000, InvalidArguments, "Invalid pixel aspect ratio");
    THROW_IF(video_settings.par_height <= 0 || video_settings.par_height >= 0x8000, InvalidArguments, "Invalid pixel aspect ratio");

    // Set orientation
    CHECK(video_settings.orientation != settings::Video::Orientation::UnknownOrientation);
    switch (video_settings.orientation) {
      case settings::Video::Orientation::LandscapeReverse:
        track_parameters.matrix[0] = -0x10000 * video_settings.par_width;
        track_parameters.matrix[1] =  0x00000;
        track_parameters.matrix[3] =  0x00000;
        track_parameters.matrix[4] = -0x10000 * video_settings.par_height;
        track_parameters.matrix[6] =  video_summary->width << 16;
        track_parameters.matrix[7] =  video_summary->height << 16;
        break;
      case settings::Video::Orientation::Portrait:
        track_parameters.matrix[0] =  0x00000;
        track_parameters.matrix[1] =  0x10000 * video_settings.par_width;
        track_parameters.matrix[3] = -0x10000 * video_settings.par_height;
        track_parameters.matrix[4] =  0x00000;
        track_parameters.matrix[6] =  video_summary->height << 16;
        break;
      case settings::Video::Orientation::PortraitReverse:
        track_parameters.matrix[0] =  0x00000;
        track_parameters.matrix[1] = -0x10000 * video_settings.par_width;
        track_parameters.matrix[3] =  0x10000 * video_settings.par_height;
        track_parameters.matrix[4] =  0x00000;
        track_parameters.matrix[7] =  video_summary->width << 16;
        break;
      case settings::Video::Orientation::Landscape:
        track_parameters.matrix[0] =  0x10000 * video_settings.par_width;
        track_parameters.matrix[1] =  0x00000;
        track_parameters.matrix[3] =  0x00000;
        track_parameters.matrix[4] =  0x10000 * video_settings.par_height;
      default:
        break;
    }
    THROW_IF(lsmash_set_track_parameters(root.get(), tracks(SampleType::Video).track_ID, &track_parameters) != 0, InvalidArguments);

    // Media params
    lsmash_media_parameters_t media_parameters;
    lsmash_initialize_media_parameters(&media_parameters);
    media_parameters.timescale = video_settings.timescale;
    char vireo_version[64];
    snprintf(vireo_version, 64, "Vireo Eyes v%s", VIREO_VERSION);
    media_parameters.media_handler_name = vireo_version;

    THROW_IF(lsmash_set_media_parameters(root.get(), tracks(SampleType::Video).track_ID, &media_parameters) != 0, InvalidArguments);
    tracks(SampleType::Video).media_timescale = lsmash_get_media_timescale(root.get(), tracks(SampleType::Video).track_ID);
    CHECK(tracks(SampleType::Video).media_timescale);

    // SPS / PPS
    auto cs = unique_ptr<lsmash_codec_specific_t, decltype(&lsmash_destroy_codec_specific_data)>(lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED), lsmash_destroy_codec_specific_data);
    CHECK(cs);
    lsmash_h264_specific_parameters_t* parameters = (lsmash_h264_specific_parameters_t*)cs->data.structured;
    parameters->lengthSizeMinusOne = video_settings.sps_pps.nalu_length_size - 1;
    THROW_IF(lsmash_append_h264_parameter_set(parameters, H264_PARAMETER_SET_TYPE_SPS, (void*)video_settings.sps_pps.sps.data(), video_settings.sps_pps.sps.count()) != 0, InvalidArguments);
    THROW_IF(lsmash_append_h264_parameter_set(parameters, H264_PARAMETER_SET_TYPE_PPS, (void*)video_settings.sps_pps.pps.data(), video_settings.sps_pps.pps.count()) != 0, InvalidArguments);
    THROW_IF(lsmash_add_codec_specific_data((lsmash_summary_t*)video_summary, cs.get()) != 0, InvalidArguments);

    auto csb = unique_ptr<lsmash_codec_specific_t, decltype(&lsmash_destroy_codec_specific_data)>(lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264_BITRATE, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED), lsmash_destroy_codec_specific_data);
    CHECK(csb);
    THROW_IF(lsmash_add_codec_specific_data((lsmash_summary_t*)video_summary, csb.get()) != 0, InvalidArguments);

    // Sample entry
    tracks(SampleType::Video).sample_entry = lsmash_add_sample_entry(root.get(), tracks(SampleType::Video).track_ID, video_summary);
    CHECK(tracks(SampleType::Video).sample_entry);
    lsmash_cleanup_summary((lsmash_summary_t*)video_summary);

    tracks(SampleType::Video).timescale = video_settings.timescale;
  }

  void setup_audio_track(const settings::Audio& audio_settings) {
    THROW_IF(!settings::Audio::IsAAC(audio_settings.codec) &&
             !settings::Audio::IsPCM(audio_settings.codec), Unsupported);

    // Audio track
    tracks(SampleType::Audio).track_ID = lsmash_create_track(root.get(), ISOM_MEDIA_HANDLER_TYPE_AUDIO_TRACK);

    // Audio track params
    lsmash_track_parameters_t track_parameters;
    lsmash_initialize_track_parameters(&track_parameters);
    track_parameters.mode = ISOM_TRACK_ENABLED;
    THROW_IF(lsmash_set_track_parameters(root.get(), tracks(SampleType::Audio).track_ID, &track_parameters) != 0, InvalidArguments);

    // Media params
    lsmash_media_parameters_t media_parameters;
    lsmash_initialize_media_parameters(&media_parameters);
    media_parameters.timescale = audio_settings.timescale;
    char vireo_version[64];
    snprintf(vireo_version, 64, "Vireo Ears v%s", VIREO_VERSION);
    media_parameters.media_handler_name = vireo_version;

    THROW_IF(lsmash_set_media_parameters(root.get(), tracks(SampleType::Audio).track_ID, &media_parameters) != 0, InvalidArguments);
    tracks(SampleType::Audio).media_timescale = lsmash_get_media_timescale(root.get(), tracks(SampleType::Audio).track_ID);
    CHECK(tracks(SampleType::Audio).media_timescale);
    tracks(SampleType::Audio).timescale = audio_settings.timescale;

    // Audio summary
    lsmash_audio_summary_t* audio_summary = (lsmash_audio_summary_t*)lsmash_create_summary(LSMASH_SUMMARY_TYPE_AUDIO);
    CHECK(audio_summary);
    audio_summary->frequency = audio_settings.sample_rate;
    audio_summary->channels = audio_settings.channels;
    audio_summary->samples_in_frame = AUDIO_FRAME_SIZE;
    if (settings::Audio::IsAAC(audio_settings.codec)) {  // AAC
      audio_summary->sample_type = ISOM_CODEC_TYPE_MP4A_AUDIO;
      auto cs = unique_ptr<lsmash_codec_specific_t, decltype(&lsmash_destroy_codec_specific_data)>(lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_MP4SYS_DECODER_CONFIG, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED), lsmash_destroy_codec_specific_data);
      CHECK(cs);
      lsmash_mp4sys_decoder_parameters_t* decoder_info = (lsmash_mp4sys_decoder_parameters_t*)cs->data.structured;
      decoder_info->objectTypeIndication = MP4SYS_OBJECT_TYPE_Audio_ISO_14496_3;
      decoder_info->streamType = MP4SYS_STREAM_TYPE_AudioStream;

      const auto extradata = audio_settings.as_extradata(settings::Audio::ExtraDataType::aac);
      lsmash_set_mp4sys_decoder_specific_info(decoder_info, (uint8_t*)extradata.data(), extradata.count());
      THROW_IF(lsmash_add_codec_specific_data((lsmash_summary_t*)audio_summary, cs.get()) != 0, InvalidArguments);
    } else {  // PCM
      THROW_IF(!settings::Audio::IsPCM(audio_settings.codec), Invalid);
      auto cs = unique_ptr<lsmash_codec_specific_t, decltype(&lsmash_destroy_codec_specific_data)>(lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_QT_AUDIO_FORMAT_SPECIFIC_FLAGS, LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED), lsmash_destroy_codec_specific_data);
      CHECK(cs);
      lsmash_qt_audio_format_specific_flags_t* flags = (lsmash_qt_audio_format_specific_flags_t*)cs->data.structured;
      if (audio_settings.codec == settings::Audio::Codec::PCM_S16LE) {
        audio_summary->sample_type = QT_CODEC_TYPE_SOWT_AUDIO;
        audio_summary->sample_size = 16;
      } else if (audio_settings.codec == settings::Audio::Codec::PCM_S16BE) {
        audio_summary->sample_type = QT_CODEC_TYPE_TWOS_AUDIO;
        audio_summary->sample_size = 16;
        flags->format_flags = QT_AUDIO_FORMAT_FLAG_BIG_ENDIAN;
      } else {
        CHECK(audio_settings.codec == settings::Audio::Codec::PCM_S24LE ||
              audio_settings.codec == settings::Audio::Codec::PCM_S24BE);
        audio_summary->sample_type = QT_CODEC_TYPE_IN24_AUDIO;
        audio_summary->sample_size = 24;
        if (audio_settings.codec == settings::Audio::Codec::PCM_S24BE) {
          flags->format_flags = QT_AUDIO_FORMAT_FLAG_BIG_ENDIAN;
        }
      }
      flags->format_flags = (lsmash_qt_audio_format_specific_flag)(flags->format_flags | QT_AUDIO_FORMAT_FLAG_SIGNED_INTEGER);
      audio_summary->bytes_per_frame = audio_settings.channels * (audio_summary->sample_size / CHAR_BIT) * AUDIO_FRAME_SIZE;
      THROW_IF(lsmash_add_codec_specific_data((lsmash_summary_t*)audio_summary, cs.get()) != 0, InvalidArguments);
    }
    tracks(SampleType::Audio).sample_entry = lsmash_add_sample_entry(root.get(), tracks(SampleType::Audio).track_ID, audio_summary);
    CHECK(tracks(SampleType::Audio).sample_entry);
    lsmash_cleanup_summary((lsmash_summary_t*)audio_summary);
  }

  void setup_main_segment(const settings::Audio& audio_settings, const settings::Video& video_settings, const FileFormat file_format) {
    this->file_format = file_format;

    // Open root
    auto write_func = [](void* opaque, uint8_t* buf, int size) -> int {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Write(creator->main_segment, buf, size);
    };
    auto read_func = [](void* opaque, uint8_t* buf, int size) -> int {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Read(creator->main_segment.get(), buf, size);
    };
    auto seek_func = [](void* opaque, int64_t offset, int whence) -> int64_t {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Seek(creator->main_segment.get(), offset, whence);
    };
    root.reset(lsmash_create_root());
    CHECK(root);

    // Setup main / initializer segment
    main_param.reset(new lsmash_file_parameters_t());
    memset((void*)main_param.get(), 0, sizeof(lsmash_file_parameters_t));
    main_param->opaque = (void*)this;
    main_param->read = read_func;
    main_param->write = write_func;
    main_param->seek = seek_func;
    lsmash_brand_type main_brands[3];
    if (qt_compatible) {
      main_brands[0] = ISOM_BRAND_TYPE_QT;
      *((int*)&main_param->mode) = LSMASH_FILE_MODE_WRITE | LSMASH_FILE_MODE_BOX | LSMASH_FILE_MODE_INITIALIZATION | LSMASH_FILE_MODE_MEDIA;
      main_param->major_brand = ISOM_BRAND_TYPE_QT;
      main_param->brand_count = 1;
    } else {
      main_brands[0] = ISOM_BRAND_TYPE_MP42;
      main_brands[1] = ISOM_BRAND_TYPE_MP41;
      if (is_dash) {
        *((int*)&main_param->mode) = LSMASH_FILE_MODE_WRITE | LSMASH_FILE_MODE_FRAGMENTED | LSMASH_FILE_MODE_BOX | LSMASH_FILE_MODE_INITIALIZATION | ~LSMASH_FILE_MODE_MEDIA | LSMASH_FILE_MODE_SEGMENT;
        main_brands[2] = ISOM_BRAND_TYPE_ISO6;
      } else {
        *((int*)&main_param->mode) = LSMASH_FILE_MODE_WRITE | LSMASH_FILE_MODE_BOX | LSMASH_FILE_MODE_INITIALIZATION | LSMASH_FILE_MODE_MEDIA;
        main_brands[2] = ISOM_BRAND_TYPE_ISO4;
      }
      main_param->major_brand = ISOM_BRAND_TYPE_MP42;
      main_param->brand_count = sizeof(main_brands) / sizeof(lsmash_brand_type);
    }
    main_param->brands = main_brands;
    main_param->minor_version = 0;
    main_param->max_chunk_duration = 0.5;
    main_param->max_async_tolerance = 2.0;
    main_param->max_chunk_size = kSize_Buffer;
    main_param->max_read_size = kSize_Buffer;

    lsmash_file_t* file = lsmash_set_file(root.get(), main_param.get());
    lsmash_activate_file(root.get(), file);

    // Movie parameters
    lsmash_movie_parameters_t movie_param;
    lsmash_initialize_movie_parameters(&movie_param);
    movie_param.timescale = max(video_settings.timescale, audio_settings.timescale);
    THROW_IF(lsmash_set_movie_parameters(root.get(), &movie_param) != 0, InvalidArguments);

    // Movie timescale
    movie_timescale = lsmash_get_movie_timescale(root.get());
    CHECK(movie_timescale);

    if (video_settings.timescale) {
      setup_video_track(video_settings);
    }

    if (audio_settings.sample_rate) {
      setup_audio_track(audio_settings);
    }
  }

  void setup_dash_data_segment() {  // has to be called after setting up the tracks
    THROW_IF(qt_compatible, Invalid);

    // Setup segment
    auto write_func = [](void* opaque, uint8_t* buf, int size) -> int {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Write(creator->dash_data_segment, buf, size);
    };
    auto read_func = [](void* opaque, uint8_t* buf, int size) -> int {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Read(creator->dash_data_segment.get(), buf, size);
    };
    auto seek_func = [](void* opaque, int64_t offset, int whence) -> int64_t {
      MP4Creator* creator = (MP4Creator*)opaque;
      return creator->Seek(creator->dash_data_segment.get(), offset, whence);
    };

    dash_data_param.reset(new lsmash_file_parameters_t());
    memset((void*)dash_data_param.get(), 0, sizeof(lsmash_file_parameters_t));

    *((int*)&dash_data_param->mode) = LSMASH_FILE_MODE_WRITE | LSMASH_FILE_MODE_FRAGMENTED | LSMASH_FILE_MODE_BOX | LSMASH_FILE_MODE_MEDIA | LSMASH_FILE_MODE_INDEX | LSMASH_FILE_MODE_SEGMENT;
    dash_data_param->opaque = (void*)this;
    dash_data_param->read = read_func;
    dash_data_param->write = write_func;
    dash_data_param->seek = seek_func;
    lsmash_brand_type brands[5] = { ISOM_BRAND_TYPE_MSDH, ISOM_BRAND_TYPE_MSIX, ISOM_BRAND_TYPE_MP42, ISOM_BRAND_TYPE_MP41, ISOM_BRAND_TYPE_ISO6 };
    dash_data_param->brands = brands;
    dash_data_param->major_brand = ISOM_BRAND_TYPE_MSDH;
    dash_data_param->brand_count = sizeof(brands) / sizeof(lsmash_brand_type);
    dash_data_param->minor_version = 0;
    dash_data_param->max_chunk_duration = std::numeric_limits<double>::max() / 2; // for ExoPlayer compatibility: avoid creating "trun" boxes in m4s file (has to be larger than the duration of track)
    dash_data_param->max_async_tolerance = 2 * dash_data_param->max_chunk_duration;
    dash_data_param->max_chunk_size = kSize_Buffer;
    dash_data_param->max_read_size = kSize_Buffer;

    lsmash_file_t* dash_data_file = lsmash_set_file(root.get(), dash_data_param.get());
    CHECK(lsmash_switch_media_segment(root.get(), dash_data_file, &moov_to_front) == 0);
    CHECK(lsmash_create_fragment_movie(root.get()) == 0);
  }

  void init(const settings::Audio audio_settings, const settings::Video video_settings, const FileFormat file_format) {
    THROW_IF(initialized, Uninitialized);
    if (file_format == HeaderOnly || file_format == SamplesOnly) {
      enforce_strict_dts_ordering = true;
    } else if (file_format == DashInitializer || file_format == DashData) {
      is_dash = true;
    }
    if (settings::Audio::IsPCM(audio_settings.codec)) {
      THROW_IF(is_dash, Invalid);
      qt_compatible = true;
    }
    setup_main_segment(audio_settings, video_settings, file_format);
    if (file_format == DashData) {
      setup_dash_data_segment();
    }
    initialized = true;
  }

  common::Data32* file() {
    if (file_format == DashData) {
      CHECK(dash_data_segment.get());
      return dash_data_segment.release();
    } else {
      CHECK(main_segment.get());
      return main_segment.release();
    }
  }
public:
  common::Data32* create(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption, const vector<common::EditBox> edit_boxes, const FileFormat file_format) {
    if (file_format != DashInitializer) {
      THROW_IF(!audio.count() && !video.count(), InvalidArguments);
    }

    init(audio.settings(), video.settings(), file_format);
    mux(audio, video, caption, edit_boxes);
    return file();
  }
};

struct MP4BoxHandler {
  static uint32_t LocateBox(const common::Data32* file, const char box_name[]) {
    size_t box_name_length = strlen(box_name);
    THROW_IF(box_name_length <= 0, InvalidArguments);
    THROW_IF(!file, InvalidArguments);
    THROW_IF(file->count() < sizeof(uint32_t) + box_name_length, InvalidArguments);
    int64_t box_location = -1;
    auto data = common::Data32(file->data() + file->a(), file->count(), nullptr);
    while (data.count()) {
      for (uint32_t j = 0; j < box_name_length; ++j) {
        if (data(data.a() + sizeof(uint32_t) + j) != box_name[j]) {
          break;
        } else if (j == box_name_length - 1) {
          box_location = data.a();
        }
      }
      if (box_location < 0) {
        uint32_t box_size = BoxSize(data);
        THROW_IF(box_size == 0, Invalid);
        data.set_bounds(data.a() + box_size, data.b());
      } else {
        break;
      }
    }
    THROW_IF(box_location < 0, Invalid);
    return (uint32_t)box_location;
  }

  static uint32_t BoxSize(const common::Data32& box) {
    THROW_IF(box.count() < sizeof(uint32_t), InvalidArguments);  // each box starts with a 32-bit size field
    const uint8_t* buffer = box.data() + box.a();
    const uint32_t box_size = (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + buffer[3];
    THROW_IF(box_size == 1, Unsupported);  // extended size field present to support files larger than 2^32 bytes
    if (box_size == 0) {  // implicit
      return box.count();
    } else {
      return box_size;
    }
  }

  static uint32_t HeaderSize(const common::Data32* file) {  // assumes [header | samples] format
    THROW_IF(!file, Invalid);
    const char box_name[] = "mdat";
    const uint32_t location = LocateBox(file, box_name);
    const auto mdat = common::Data32(file->data() + file->a() + location, file->count() - location, nullptr);
    THROW_IF(BoxSize(mdat) != mdat.count(), Invalid);  // mdat has to span till the end of file
    return location + sizeof(uint32_t) + (uint32_t)strlen(box_name);  // mdat box size and "mdat" belongs to header
  }
};

struct _MP4 {
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
  functional::Caption<encode::Sample> caption;
  vector<common::EditBox> edit_boxes;
  FileFormat file_format;
  unique_ptr<common::Data32> cached_file;
  void create_and_cache_file() {
    MP4Creator creator;
    cached_file.reset(creator.create(audio, video, caption, edit_boxes, file_format));
    if (file_format == FileFormat::SamplesOnly) {
      uint32_t header_size = MP4BoxHandler::HeaderSize(cached_file.get());
      cached_file->set_bounds(header_size, cached_file->b());
    }
  }
};

MP4::MP4(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption, const vector<common::EditBox> edit_boxes, const FileFormat file_format)
  : _this(new _MP4()) {
  THROW_IF(video.settings().timescale == 0 && audio.settings().sample_rate == 0.0, InvalidArguments);
  THROW_IF(video.settings().sps_pps.sps.count() >= security::kMaxHeaderSize, Unsafe);
  THROW_IF(video.settings().sps_pps.pps.count() >= security::kMaxHeaderSize, Unsafe);
  if (video.settings().timescale) {
    THROW_IF(!security::valid_dimensions(video.settings().width, video.settings().height), Unsafe);
  }

  _this->audio = audio;
  _this->video = video;
  _this->caption = caption;
  _this->edit_boxes = edit_boxes;
  _this->file_format = file_format;

  *static_cast<std::function<common::Data32(void)>*>(this) = [_this = _this]() {
    if (!_this->cached_file) {
      _this->create_and_cache_file();
    }
    return *_this->cached_file.get();
  };
}

MP4::MP4(const MP4& mp4)
  : Function<common::Data32>(*static_cast<const functional::Function<common::Data32>*>(&mp4)), _this(mp4._this) {
}

MP4::MP4(MP4&& mp4)
  : Function<common::Data32>(*static_cast<const functional::Function<common::Data32>*>(&mp4)), _this(mp4._this) {
  mp4._this = nullptr;
}

auto MP4::operator()() -> common::Data32 {
  return move((*static_cast<std::function<common::Data32(void)>*>(this))());
  // breakdown of move((*static_cast<std::function<common::Data32(void)>*>(this))())
  // 1- std::function<common::Data32(void)> : a std::function that doesn't take any arguments and returns common::Data32
  // 2- static_cast<...*>(this))            : static casting "this" to a std::function pointer
  // 3- (*static_cast<...>(...))()          : calling the apply method on std::function pointer, which returns common::Data32
  // 4- move(...)                           : prevents copying the common::Data32
}

auto MP4::operator()(FileFormat file_format) -> common::Data32 {
  if (file_format != _this->file_format) {
    if (file_format == FileFormat::HeaderOnly && _this->file_format == FileFormat::SamplesOnly && _this->cached_file) {  // special case where we can avoid reprocessing
      _this->cached_file->set_bounds(0, _this->cached_file->b());
      uint32_t header_size = MP4BoxHandler::HeaderSize(_this->cached_file.get());
      _this->cached_file->set_bounds(_this->cached_file->a(), _this->cached_file->a() + header_size);
    } else {
      _this->file_format = file_format;
      _this->cached_file.reset(nullptr);
    }
  }
  return move((*static_cast<std::function<common::Data32(void)>*>(this))());
}

}}
