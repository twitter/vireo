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

#include <algorithm>
extern "C" {
#include "libavformat/avformat.h"
}
#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/security.h"
#include "vireo/decode/types.h"
#include "vireo/internal/decode/h264.h"
#include "vireo/internal/decode/types.h"
#include "vireo/error/error.h"

CONSTRUCTOR static void _Init() {
  av_register_all();
}

namespace vireo {
namespace internal {
namespace decode {

struct FrameInfo {
  int64_t pts;
  bool keyframe;
};

struct _H264 {
  common::Data16 headers;
  AVCodec* codec = NULL;
  unique_ptr<AVCodecContext, function<void(AVCodecContext*)>> codec_context = { NULL, [](AVCodecContext* p) {
    avcodec_close(p);
    av_free(p);
  }};
  functional::Video<Sample> video_track;
  vector<FrameInfo> frame_infos;  // pts sorted list of frame information
  uint32_t num_cached_frames = 0;
  int64_t last_decoded_index = -1;
  _H264(const functional::Video<Sample>& video_track, common::Data16&& headers, uint32_t thread_count)
    : video_track(video_track), headers(move(headers)) {
    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    CHECK(codec);
    codec_context.reset(avcodec_alloc_context3(codec));
    CHECK(codec_context);
    codec_context->refcounted_frames = 1;
    codec_context->extradata = (uint8_t*)this->headers.data();
    codec_context->extradata_size = this->headers.count();
    codec_context->strict_std_compliance = FF_COMPLIANCE_STRICT;
    if (thread_count > 1) {
      codec_context->thread_count = thread_count;
      codec_context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
    }
    CHECK(avcodec_open2(codec_context.get(), codec, NULL) >= 0);
  }
  auto process_samples() -> void {
    frame_infos.clear();
    for (auto sample: video_track) {
      THROW_IF(video_track.count() >= security::kMaxSampleCount, Unsafe);
      THROW_IF(sample.type != SampleType::Video, InvalidArguments);

      FrameInfo frame_info = { sample.pts, sample.keyframe };
      frame_infos.push_back(frame_info);
    }
    sort(frame_infos.begin(), frame_infos.end(), [](const FrameInfo& a, const FrameInfo& b){
      return a.pts < b.pts;
    });
  }
};

H264::H264(const functional::Video<Sample>& track, uint32_t thread_count) {
  const auto& settings = track.settings();
  THROW_IF(settings.codec != settings::Video::Codec::H264, Unsupported);
  THROW_IF(!settings.timescale, Invalid);
  if (settings.width || settings.height) {
    THROW_IF(!security::valid_dimensions(settings.width, settings.height), Unsafe);
  }
  THROW_IF(thread_count > 16, InvalidArguments);
  const auto& sps_pps = settings.sps_pps;
  auto extradata = sps_pps.as_extradata(header::SPS_PPS::iso);
  THROW_IF(extradata.count() > security::kMaxHeaderSize * 2, Unsafe);
  const uint16_t padded_size = (size_t)extradata.count() + FF_INPUT_BUFFER_PADDING_SIZE;
  common::Data16 extradata_padded = { (const uint8_t*)calloc(padded_size, sizeof(uint8_t)), padded_size, [](uint8_t* p) { free(p); } };
  extradata_padded.copy(extradata);

  _this = make_shared<_H264>(track, move(extradata_padded), thread_count);
  _this->process_samples();
  set_bounds(0, track.count());

  _settings = track.settings().to_square_pixel();
  _settings.codec = settings::Video::Codec::Unknown;
}

H264::H264(const H264& h264)
  : functional::DirectVideo<H264, frame::Frame>(h264.a(), h264.b(), h264.settings()), _this(h264._this) {}

static inline bool intra_decode_refresh(const common::Data32& data, const uint8_t nalu_length_size) {
  uint8_t* bytes = (uint8_t*)data.data() + data.a();
  uint32_t size = data.count();
  while (size) {
    THROW_IF(size <= nalu_length_size, Invalid);
    if ((bytes[nalu_length_size] & 0x1F) == H264NalType::IDR) {
      return true;
    }
    uint32_t nal_size = 0;
    for (uint8_t i = 0; i < nalu_length_size; ++i) {
      nal_size = nal_size << (CHAR_BIT * sizeof(uint8_t));
      nal_size += bytes[i];
    }
    THROW_IF(size < (nalu_length_size + nal_size), Invalid);
    size -= (nalu_length_size + nal_size);
    bytes += (nalu_length_size + nal_size);
  }
  return false;
}

auto H264::operator()(uint32_t index) const -> frame::Frame {
  THROW_IF(index >= count(), OutOfRange);
  THROW_IF(index >= _this->frame_infos.size(), OutOfRange);

  frame::Frame frame;
  frame.pts = _this->frame_infos[index].pts;
  frame.yuv = [_this = _this, index, keyframe = _this->frame_infos[index].keyframe]() -> frame::YUV {
    unique_ptr<AVFrame, function<void(AVFrame*)>> frame(av_frame_alloc(), [](AVFrame* frame) {
      av_frame_unref(frame);
      av_free(frame);
    });
    auto settings = _this->video_track.settings();

    auto previous_idr_frame = [&_this, &settings](uint32_t index) -> uint32_t {
      THROW_IF(index >= _this->video_track.count(), OutOfRange);
      while (index) {
        // we return 0 if we cannot find an actual IDR frame <= index
        // this ensures that we at least attempt to decode starting from first available frame
        const Sample& sample = _this->video_track((uint32_t)index);
        bool is_idr = sample.keyframe && intra_decode_refresh(sample.nal(), settings.sps_pps.nalu_length_size);
        if (is_idr) {
          break;
        }
        index--;
      }
      return index;
    };

    auto flush_decoder_buffers = [&_this]() {
      avcodec_flush_buffers(_this->codec_context.get());
      _this->num_cached_frames = 0;
    };

    auto update_resolution = [](const AVFrame* frame, settings::Video& settings) {
      settings.width = (uint16_t)frame->width;
      settings.height = (uint16_t)frame->height;
      if (frame->sample_aspect_ratio.num) {
        if (frame->sample_aspect_ratio.num < frame->sample_aspect_ratio.den) {
          settings.width = settings.width * frame->sample_aspect_ratio.num / frame->sample_aspect_ratio.den;
        } else {
          settings.height = settings.height * frame->sample_aspect_ratio.den / frame->sample_aspect_ratio.num;
        }
      }
    };

    auto decode_frame = [_this = _this, &frame, keyframe, &settings, update_resolution](uint32_t index) {
      THROW_IF(index >= _this->video_track.count(), OutOfRange);
      AVPacket packet;
      int got_picture = 0;
      while (!got_picture) {
        av_init_packet(&packet);
        if (index + _this->num_cached_frames < _this->video_track.count()) {
          const Sample& sample = _this->video_track(index + _this->num_cached_frames);
          const common::Data32 nal = sample.nal();
          av_new_packet(&packet, nal.count());
          memcpy((void*)packet.data, nal.data() + nal.a(), nal.count());
          packet.pts = sample.pts;
          packet.dts = sample.dts;
          packet.flags = sample.keyframe ? AV_PKT_FLAG_KEY : 0;
          CHECK(avcodec_decode_video2(_this->codec_context.get(), frame.get(), &got_picture, &packet) == packet.size);
        } else {
          CHECK(_this->num_cached_frames > 0);
          av_init_packet(&packet);
          packet.data = NULL;
          packet.size = 0;
          CHECK(avcodec_decode_video2(_this->codec_context.get(), frame.get(), &got_picture, &packet) == 0);
          if (!got_picture) {
            break;
          }
          _this->num_cached_frames--;
        }
        av_packet_unref(&packet);
        if (got_picture) {
          if (settings.width == 0 && settings.height == 0) { // infer from decoded frame when not specified in settings
            update_resolution(frame.get(), settings);
          }
          THROW_IF(frame->format != AV_PIX_FMT_YUV420P && frame->format != AV_PIX_FMT_YUVJ420P, Unsupported);
        } else {
          _this->num_cached_frames++;
          THROW_IF(_this->num_cached_frames > std::min((int)_this->video_track.count(), 32), Unsafe);
        }
      }
      if (!got_picture && keyframe) {  // TODO: remove once MEDIASERV-4386 is resolved
        THROW_IF(!intra_decode_refresh(_this->video_track(index).nal(), settings.sps_pps.nalu_length_size), Unsupported);
      }
      CHECK(got_picture);
      _this->last_decoded_index = index;
    };

    if (index - _this->last_decoded_index == 1) {
      // optimization: current sample is right after last decoded sample - continue normally
      decode_frame(index);
    } else {
      if (keyframe) {
        // at the beginning of a gop boundary - start fresh
        if (_this->num_cached_frames) {
          flush_decoder_buffers();
        }
        decode_frame(index);
      } else {
        uint32_t index_to_start_decoding = previous_idr_frame(index);;
        if (index_to_start_decoding <= _this->last_decoded_index && index > _this->last_decoded_index) {
          // optimization: we can just decode the frames starting from last decoded index - no need to decode from previous IDR
          index_to_start_decoding = (uint32_t)(_this->last_decoded_index + 1);
        } else {
          // we have to start fresh
          flush_decoder_buffers();
        }
        THROW_IF(index - index_to_start_decoding >= security::kMaxGOPSize, Unsafe,
                 "GOP is too large (need to decode frame " << index_to_start_decoding << " for frame " << index
                 << " (max allowed = " << security::kMaxGOPSize << ")");
        for (uint32_t current_index = index_to_start_decoding; current_index <= index; ++current_index) {
          decode_frame(current_index);
        }
      }
    }
    AVFrame* yFrame = av_frame_clone(frame.get());
    AVFrame* uFrame = av_frame_clone(frame.get());
    AVFrame* vFrame = av_frame_clone(frame.get());
    common::Data32 yData(frame->data[0], frame->linesize[0] * frame->height, [yFrame](uint8_t*) {
      av_frame_unref(yFrame);
      av_free(yFrame);
    });
    common::Data32 uData(frame->data[1], frame->linesize[1] * frame->height / 2, [uFrame](uint8_t*) {
      av_frame_unref(uFrame);
      av_free(uFrame);
    });
    common::Data32 vData(frame->data[2], frame->linesize[2] * frame->height / 2, [vFrame](uint8_t*) {
      av_frame_unref(vFrame);
      av_free(vFrame);
    });
    frame::Plane y((uint16_t)frame->linesize[0], (uint16_t)frame->width, (uint16_t)frame->height, move(yData));
    frame::Plane u((uint16_t)frame->linesize[1], (uint16_t)frame->width / 2, (uint16_t)frame->height / 2, move(uData));
    frame::Plane v((uint16_t)frame->linesize[2], (uint16_t)frame->width / 2, (uint16_t)frame->height / 2, move(vData));

    auto yuv = frame::YUV(move(y), move(u), move(v), false);
    return (settings.width != frame->width || settings.height != frame->height) ? move(yuv.stretch(settings.width, frame->width, settings.height, frame->height, false)) : move(yuv);
  };
  frame.rgb = [yuv = frame.yuv]() -> frame::RGB {
    return yuv().rgb(4);
  };
  return move(frame);
}

}}}
