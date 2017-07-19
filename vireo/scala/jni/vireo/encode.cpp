//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <fstream>
#include <mutex>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/common/path.h"
#include "vireo/encode/aac.h"
#include "vireo/encode/h264.h"
#include "vireo/encode/jpg.h"
#include "vireo/encode/png.h"
#include "vireo/encode/vp8.h"
#include "vireo/encode/vorbis.h"
#include "vireo/error/error.h"
#include "vireo/util/util.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/encode.h"
#include "vireo/scala/jni/vireo/util.h"

using std::lock_guard;
using std::mutex;
using std::ofstream;

using namespace vireo;

// AAC Implementation

struct _JNIAACEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::AAC> encoder;
  ~_JNIAACEncodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_encode_jni_AAC_jniInit(JNIEnv* env, jobject aac_obj, jobject sounds_obj, jbyte channels, jint bitrate) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_aac = jni::Wrap(env, aac_obj);
    auto jni = new _JNIAACEncodeStruct();
    jni_aac.set<jlong>("jni", (jlong)jni);

    vector<sound::Sound> sound_funcs = jni::createVectorFromMedia(env, sounds_obj, function<sound::Sound(jobject)>([env](jobject sound_obj) -> sound::Sound {
      auto jni_sound = jni::Wrap(env, sound_obj);
      auto jni_pcm_func = jni::Wrap(env, jni_sound.get("pcm", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_pcm_func);

      return (sound::Sound) {
        (int64_t)jni_sound.get<jlong>("pts"),
        jni::createFunc<sound::PCM>(env, *jni_pcm_func)
      };
    }));

    auto jni_sounds = jni::Wrap(env, sounds_obj);
    auto settings_obj = jni_sounds.get("settings", "Ljava/lang/Object;");
    CHECK(settings_obj);
    const auto settings = jni::createAudioSettings(env, settings_obj);
    jni->encoder.reset(new encode::AAC(functional::Audio<sound::Sound>(sound_funcs, settings), (uint8_t)channels, (int)bitrate));

    jni_aac.set<jint>("a", jni_sounds.get<jint>("a"));
    jni_aac.set<jint>("b", jni_sounds.get<jint>("b"));

    setAudioSettings(env, jni_aac, jni->encoder->settings());
  }, [env, aac_obj] {
    Java_com_twitter_vireo_encode_jni_AAC_jniClose(env, aac_obj);
  });
}

void JNICALL Java_com_twitter_vireo_encode_jni_AAC_jniClose(JNIEnv* env, jobject aac_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_aac(env, aac_obj);
    auto jni = (_JNIAACEncodeStruct*)jni_aac.get<jlong>("jni");
    delete jni;
    jni_aac.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_AAC_sample(JNIEnv* env, jobject aac_obj, jlong _jni, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIAACEncodeStruct*)_jni;
    CHECK(jni && jni->encoder);
    auto& encoder = *jni->encoder;
    auto sample = move(encoder(index));

    auto jni_byte_data = jni::create(env, jni, move(sample.nal), copy);
    return *jni::Wrap(env, "com/twitter/vireo/encode/Sample", "(JJZBLcom/twitter/vireo/common/Data;)V",
                      sample.pts, sample.dts, sample.keyframe, sample.type, *jni_byte_data);
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_encode_jni_AAC_freeSample(JNIEnv* env, jobject aac_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIAACEncodeStruct*)_jni;
    CHECK(jni && jni->encoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");

    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

// Vorbis Implementation

struct _JNIVorbisEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::Vorbis> encoder;
  ~_JNIVorbisEncodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_jniInit(JNIEnv* env, jobject vorbis_obj, jobject sounds_obj, jbyte channels, jint bitrate) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_vorbis = jni::Wrap(env, vorbis_obj);
    auto jni = new _JNIVorbisEncodeStruct();
    jni_vorbis.set<jlong>("jni", (jlong)jni);

    auto jni_sounds = jni::Wrap(env, sounds_obj);

    auto audio_settings_obj = jni_sounds.get("settings", "Ljava/lang/Object;");
    CHECK(audio_settings_obj);
    auto audio_settings = jni::createAudioSettings(env, audio_settings_obj);

    const uint32_t sound_count = (uint32_t)jni_sounds.get<jint>("b");
    functional::Audio<sound::Sound> sounds([env, jni, sound_count, jni_sounds](uint32_t index) -> sound::Sound {
      auto sound_obj = jni_sounds.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index);
      auto jni_sound = jni::Wrap(env, sound_obj);
      auto jni_sound_func = jni::Wrap(env, jni_sound.get("pcm", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_sound_func);

      return (sound::Sound) {
        (int64_t)jni_sound.get<jlong>("pts"),
        jni::createFunc<sound::PCM>(env, *jni_sound_func)
      };
    }, (uint32_t)jni_sounds.get<jint>("a"), (uint32_t)jni_sounds.get<jint>("b"), audio_settings);

    jni->encoder.reset(new encode::Vorbis(sounds, channels, bitrate));
    setAudioSettings(env, jni_vorbis, jni->encoder->settings());

    jni_vorbis.set<jint>("a", jni->encoder->a());
    jni_vorbis.set<jint>("b", jni->encoder->b());
  }, [env, vorbis_obj] {
    Java_com_twitter_vireo_encode_jni_Vorbis_jniClose(env, vorbis_obj);
  });
}

void JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_jniClose(JNIEnv* env, jobject vorbis_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_vorbis(env, vorbis_obj);
    auto jni = (_JNIVorbisEncodeStruct*)jni_vorbis.get<jlong>("jni");
    delete jni;
    jni_vorbis.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_sample(JNIEnv* env, jobject vorbis_obj, jlong _jni, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIVorbisEncodeStruct*)_jni;
    CHECK(jni && jni->encoder);

    auto& encoder = *jni->encoder;
    auto sample = move(encoder(index));

    auto jni_byte_data = jni::create(env, jni, move(sample.nal), copy);
    auto jni_sample = jni::Wrap(env, "com/twitter/vireo/encode/Sample", "(JJZBLcom/twitter/vireo/common/Data;)V",
                                sample.pts, sample.dts, sample.keyframe, sample.type, *jni_byte_data);
    return *jni_sample;
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_encode_jni_Vorbis_freeSample(JNIEnv* env, jobject vorbis_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {    auto jni = (_JNIVorbisEncodeStruct*)_jni;
    CHECK(jni && jni->encoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");

    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

// H264 Implementation

struct _JNIH264EncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::H264> encoder;
  ~_JNIH264EncodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_encode_jni_H264_jniInit(JNIEnv* env, jobject h264_obj, jobject frames_obj, jint optimization, jint thread_count, jbyte rc_method, jfloat crf, jint max_bitrate, jint bitrate, jint buffer_size, jfloat buffer_init, jint look_ahead, jboolean is_second_pass, jboolean enable_mb_tree, jbyte aq_mode, jint qp_min, jstring stats_log_path, jboolean mixed_refs, jint trellis, jbyte me_method, jint subpel_refine, jint num_bframes, jbyte pyramid_mode, jint keyint_max, jint keyint_min, jint frame_references, jbyte profile, jfloat fps) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_h264 = jni::Wrap(env, h264_obj);
    auto jni = new _JNIH264EncodeStruct();
    jni_h264.set<jlong>("jni", (jlong)jni);

    vector<frame::Frame> frame_funcs = jni::createVectorFromMedia(env, frames_obj, function<frame::Frame(jobject)>([env](jobject frame_obj) -> frame::Frame {
      auto jni_frame = jni::Wrap(env, frame_obj);
      auto jni_yuv_func = jni::Wrap(env, jni_frame.get("yuv", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_yuv_func);

      return (frame::Frame) {
        (int64_t)jni_frame.get<jlong>("pts"),
        jni::createFunc<frame::YUV>(env, *jni_yuv_func),
        nullptr
      };
    }));

    auto jni_frames = jni::Wrap(env, frames_obj);
    auto settings_obj = jni_frames.get("settings", "Ljava/lang/Object;");
    CHECK(settings_obj);
    const auto settings = jni::createVideoSettings(env, settings_obj);
    auto computation = encode::H264Params::ComputationalParams((uint32_t)optimization, (uint32_t)thread_count);
    const char* log_path = env->GetStringUTFChars(stats_log_path, NULL);
    string log_path_string = log_path;
    env->ReleaseStringUTFChars(stats_log_path, log_path);
    auto rc = encode::H264Params::RateControlParams((encode::RCMethod)rc_method, (float)crf, (uint32_t) max_bitrate, (uint32_t)bitrate, (uint32_t)buffer_size, (float)buffer_init, (uint32_t)look_ahead, (bool)is_second_pass, (bool)enable_mb_tree, (encode::AdaptiveQuantizationMode)aq_mode, (uint32_t)qp_min, log_path_string, (bool)mixed_refs, (uint32_t)trellis, (encode::MotionEstimationMethod)me_method, (uint32_t)subpel_refine);
    auto gop = encode::H264Params::GopParams((int32_t)num_bframes, (encode::PyramidMode)pyramid_mode, (uint32_t)keyint_max, (uint32_t)keyint_min, (uint32_t)frame_references);
    auto params = encode::H264Params(computation, rc, gop, (encode::VideoProfileType)profile, (float)fps);
    jni->encoder.reset(new encode::H264(functional::Video<frame::Frame>(frame_funcs, settings), params));

    jni_h264.set<jint>("a", jni_frames.get<jint>("a"));
    jni_h264.set<jint>("b", jni_frames.get<jint>("b"));

    setVideoSettings(env, jni_h264, jni->encoder->settings());
  }, [env, h264_obj] {
    Java_com_twitter_vireo_encode_jni_H264_jniClose(env, h264_obj);
  });
}


void JNICALL Java_com_twitter_vireo_encode_jni_H264_jniClose(JNIEnv* env, jobject h264_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {    jni::Wrap jni_h264(env, h264_obj);
    auto jni = (_JNIH264EncodeStruct*)jni_h264.get<jlong>("jni");
    delete jni;
    jni_h264.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_H264_sample(JNIEnv* env, jobject h264_obj, jlong _jni, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIH264EncodeStruct*)_jni;
    CHECK(jni);
    auto& encoder = *jni->encoder;
    auto sample = move(encoder(index));

    auto jni_byte_data = jni::create(env, jni, move(sample.nal), copy);
    auto jni_sample = jni::Wrap(env, "com/twitter/vireo/encode/Sample", "(JJZBLcom/twitter/vireo/common/Data;)V",
                                sample.pts, sample.dts, sample.keyframe, sample.type, *jni_byte_data);
    return *jni_sample;
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_encode_jni_H264_freeSample(JNIEnv* env, jobject h264_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIH264EncodeStruct*)_jni;
    CHECK(jni && jni->encoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");

    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

// VP8 Implementation

struct _JNIVP8EncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::VP8> encoder;
  ~_JNIVP8EncodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_encode_jni_VP8_jniInit(JNIEnv* env, jobject vp8_obj, jobject frames_obj, jint quantizer,
                                                            jint optimization, jfloat fps, jint bit_rate) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_vp8 = jni::Wrap(env, vp8_obj);
    auto jni = new _JNIVP8EncodeStruct();
    jni_vp8.set<jlong>("jni", (jlong)jni);

    vector<frame::Frame> frame_funcs = jni::createVectorFromMedia(env, frames_obj, function<frame::Frame(jobject)>([env](jobject frame_obj) -> frame::Frame {
      auto jni_frame = jni::Wrap(env, frame_obj);
      auto jni_yuv_func = jni::Wrap(env, jni_frame.get("yuv", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_yuv_func);

      return (frame::Frame) {
        (int64_t)jni_frame.get<jlong>("pts"),
        jni::createFunc<frame::YUV>(env, *jni_yuv_func),
        nullptr
      };
    }));

    auto jni_frames = jni::Wrap(env, frames_obj);
    auto settings_obj = jni_frames.get("settings", "Ljava/lang/Object;");
    CHECK(settings_obj);
    const auto settings = jni::createVideoSettings(env, settings_obj);
    jni->encoder.reset(new encode::VP8(functional::Video<frame::Frame>(frame_funcs, settings), (int)quantizer, (int)optimization, (float)fps, (int)bit_rate));

    jni_vp8.set<jint>("a", jni_frames.get<jint>("a"));
    jni_vp8.set<jint>("b", jni_frames.get<jint>("b"));

    setVideoSettings(env, jni_vp8, jni->encoder->settings());
  }, [env, vp8_obj] {
    Java_com_twitter_vireo_encode_jni_VP8_jniClose(env, vp8_obj);
  });
}

void JNICALL Java_com_twitter_vireo_encode_jni_VP8_jniClose(JNIEnv* env, jobject vp8_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_vp8(env, vp8_obj);
    auto jni = (_JNIVP8EncodeStruct*)jni_vp8.get<jlong>("jni");
    delete jni;
    jni_vp8.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_VP8_sample(JNIEnv* env, jobject vp8_obj, jlong _jni, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIVP8EncodeStruct*)_jni;
    CHECK(jni);
    auto& encoder = *jni->encoder;
    auto sample = move(encoder(index));

    auto jni_byte_data = jni::create(env, jni, move(sample.nal), copy);
    return *jni::Wrap(env, "com/twitter/vireo/encode/Sample", "(JJZBLcom/twitter/vireo/common/Data;)V",
                                sample.pts, sample.dts, sample.keyframe, sample.type, *jni_byte_data);
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_encode_jni_VP8_freeSample(JNIEnv* env, jobject vp8_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIVP8EncodeStruct*)_jni;
    CHECK(jni && jni->encoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");

    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

// JPG Implementation

struct _JNIJPGEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::JPG> encoder;
  functional::Video<frame::YUV> yuvs;
  int quality = 0;
  int optimization = 0;
};

void JNICALL Java_com_twitter_vireo_encode_jni_JPG_jniInit(JNIEnv* env, jobject jpg_obj, jobject yuv_funcs_obj, jint quality, jint optimization) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_jpg = jni::Wrap(env, jpg_obj);
    auto jni = new _JNIJPGEncodeStruct();
    jni_jpg.set<jlong>("jni", (jlong)jni);

    jni->quality = (int)quality;
    jni->optimization = (int)optimization;

    auto jni_yuv_funcs = jni::Wrap(env, yuv_funcs_obj);
    const uint32_t a = (uint32_t)jni_yuv_funcs.get<jint>("a");
    const uint32_t b = (uint32_t)jni_yuv_funcs.get<jint>("b");

    const uint32_t yuv_count = b;

    for (int index = 0; index < yuv_count; ++index) {
      auto jni_yuv_func = jni::Wrap(env, jni_yuv_funcs.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_yuv_func);
    }

    auto video_settings_obj = jni_yuv_funcs.get("settings", "Ljava/lang/Object;");
    auto video_settings = video_settings_obj ? jni::createVideoSettings(env, video_settings_obj) : settings::Video::None;

    jni->yuvs = functional::Video<frame::YUV>([env, jni, yuv_count, jni_yuv_funcs = move(jni_yuv_funcs)](uint32_t index) {
      return jni::createFunc<frame::YUV>(env, jni_yuv_funcs.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, a, b, video_settings);

    jni_jpg.set<jint>("a", jni->yuvs.a());
    jni_jpg.set<jint>("b", jni->yuvs.b());
  }, [env, jpg_obj] {
    Java_com_twitter_vireo_encode_jni_JPG_jniClose(env, jpg_obj);
  });
}

void JNICALL Java_com_twitter_vireo_encode_jni_JPG_jniClose(JNIEnv* env, jobject jpg_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_jpg(env, jpg_obj);
    auto jni = (_JNIJPGEncodeStruct*)jni_jpg.get<jlong>("jni");
    delete jni;
    jni_jpg.set<jlong>("jni", 0);
  });
}

static void _CreateJPGEncoderIfNeeded(JNIEnv* env, jobject jpg_obj) {
  jni::Wrap jni_jpg(env, jpg_obj);
  auto jni = (_JNIJPGEncodeStruct*)jni_jpg.get<jlong>("jni");
  CHECK(jni);

  if (!jni->encoder.get()) {
    jni->encoder.reset(new encode::JPG(jni->yuvs, jni->quality, jni->optimization));
  }
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_JPG_encode(JNIEnv* env, jobject jpg_obj, jlong _jni, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIJPGEncodeStruct*)_jni;
    CHECK(jni);

    _CreateJPGEncoderIfNeeded(env, jpg_obj);
    CHECK(jni->encoder);

    auto data = (*jni->encoder)(index);

    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);

    return *jni_byte_data;
  }, NULL);
}

// PNG Implementation

struct _JNIPNGEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<encode::PNG> encoder;
  functional::Video<frame::RGB> rgbs;
};

void JNICALL Java_com_twitter_vireo_encode_jni_PNG_jniInit(JNIEnv* env, jobject png_obj, jobject rgb_funcs_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_png = jni::Wrap(env, png_obj);
    auto jni = new _JNIPNGEncodeStruct();
    jni_png.set<jlong>("jni", (jlong)jni);

    auto jni_rgb_funcs = jni::Wrap(env, rgb_funcs_obj);
    const uint32_t a = (uint32_t)jni_rgb_funcs.get<jint>("a");
    const uint32_t b = (uint32_t)jni_rgb_funcs.get<jint>("b");

    const uint32_t rgb_count = b;
    for (int index = 0; index < b; ++index) {
      auto jni_rgb_func = jni::Wrap(env, jni_rgb_funcs.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_rgb_func);
    }

    auto video_settings_obj = jni_rgb_funcs.get("settings", "Ljava/lang/Object;");
    auto video_settings = video_settings_obj ? jni::createVideoSettings(env, video_settings_obj) : settings::Video::None;

    jni->rgbs = functional::Video<frame::RGB>([env, jni, rgb_count, jni_rgb_funcs = move(jni_rgb_funcs)](uint32_t index) {
      return jni::createFunc<frame::RGB>(env, jni_rgb_funcs.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, a, b, video_settings);

    jni_png.set<jint>("a", jni->rgbs.a());
    jni_png.set<jint>("b", jni->rgbs.b());
  }, [env, png_obj] {
    Java_com_twitter_vireo_encode_jni_PNG_jniClose(env, png_obj);
  });
}

void JNICALL Java_com_twitter_vireo_encode_jni_PNG_jniClose(JNIEnv* env, jobject png_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_png(env, png_obj);
    auto jni = (_JNIPNGEncodeStruct*)jni_png.get<jlong>("jni");
    delete jni;
    jni_png.set<jlong>("jni", 0);
  });
}

static void _CreatePNGEncoderIfNeeded(JNIEnv* env, jobject png_obj) {
  jni::Wrap jni_png(env, png_obj);
  auto jni = (_JNIPNGEncodeStruct*)jni_png.get<jlong>("jni");
  CHECK(jni);

  if (!jni->encoder.get()) {
    jni->encoder.reset(new encode::PNG(jni->rgbs));
  }
}

jobject JNICALL Java_com_twitter_vireo_encode_jni_PNG_encode(JNIEnv* env, jobject png_obj, jlong _jni, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIPNGEncodeStruct*)_jni;
    CHECK(jni);

    _CreatePNGEncoderIfNeeded(env, png_obj);
    CHECK(jni->encoder);

    auto data = (*jni->encoder)(index);

    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);

    return *jni_byte_data;
  }, NULL);
}
