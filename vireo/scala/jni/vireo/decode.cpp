//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <list>
#include <fstream>
#include <mutex>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/path.h"
#include "vireo/decode/audio.h"
#include "vireo/decode/video.h"
#include "vireo/error/error.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/decode.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/util/util.h"

using std::ifstream;
using std::list;
using std::mutex;
using std::lock_guard;

using namespace vireo;

// Audio

struct _JNIAudioDecodeStruct : public jni::Struct<common::Sample16> {
  mutex lock;
  unique_ptr<decode::Audio> decoder;
  map<uint32_t, sound::Sound> sound_funcs;
  ~_JNIAudioDecodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_decode_jni_Audio_jniInit(JNIEnv* env, jobject decoder_obj, jobject samples_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = new _JNIAudioDecodeStruct();
    auto jni_decoder = jni::Wrap(env, decoder_obj);
    jni_decoder.set<jlong>("jni", (jlong)jni);

    vector<decode::Sample> sample_funcs = jni::createVectorFromMedia(env, samples_obj, function<decode::Sample(jobject)>([env](jobject sample_obj) -> decode::Sample {
      auto jni_sample = jni::Wrap(env, sample_obj);
      auto jni_nal_func = jni::Wrap(env, jni_sample.get("nal", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_nal_func);

      return (decode::Sample){
        (int64_t)jni_sample.get<jlong>("pts"),
        (int64_t)jni_sample.get<jlong>("dts"),
        (bool)jni_sample.get<jboolean>("keyframe"),
        (SampleType)jni_sample.get<jbyte>("sampleType"),
        jni::createFunc<common::Data32>(env, *jni_nal_func)
      };
    }));

    auto jni_samples = jni::Wrap(env, samples_obj);
    auto settings_obj = jni_samples.get("settings", "Ljava/lang/Object;");
    const auto settings = jni::createAudioSettings(env, settings_obj);

    auto samples = functional::Audio<decode::Sample>(sample_funcs, settings);

    jni->decoder.reset(new decode::Audio(samples));

    jni_decoder.set<jint>("a", jni->decoder->a());
    jni_decoder.set<jint>("b", jni->decoder->b());
    setAudioSettings(env, jni_decoder, jni->decoder->settings());
  }, [env, decoder_obj] {
    Java_com_twitter_vireo_decode_jni_Audio_jniClose(env, decoder_obj);
  });
}

void JNICALL Java_com_twitter_vireo_decode_jni_Audio_jniClose(JNIEnv* env, jobject decoder_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_decoder = jni::Wrap(env, decoder_obj);
    auto jni = (_JNIAudioDecodeStruct*)jni_decoder.get<jlong>("jni");
    delete jni;
    jni_decoder.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_decode_jni_Audio_decode(JNIEnv* env, jobject decoder_obj, jlong _jni, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIAudioDecodeStruct*)_jni;
    CHECK(jni);
    auto& decoder = *jni->decoder;
    auto sound = decoder(index);
    jni->sound_funcs[(uint32_t)index] = move(sound);
    return *jni::Wrap(env, "com/twitter/vireo/decode/jni/Audio$Sound", "(Lcom/twitter/vireo/decode/jni/Audio;JI)V",
                      decoder_obj, (jlong)sound.pts, (jint)index);
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_decode_jni_Audio_pcm(JNIEnv* env, jobject decoder_obj, jlong _jni, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIAudioDecodeStruct*)_jni;
    CHECK(jni);

    lock_guard<mutex>(jni->lock);
    THROW_IF(index >= jni->decoder->count(), OutOfRange);
    const auto found = jni->sound_funcs.find(index);
    CHECK(found != jni->sound_funcs.end());
    const auto& sample = found->second;
    const auto pcm = sample.pcm();

    auto jni_byte_data = jni::create(env, jni, move(pcm.samples()), copy);
    return *jni::Wrap(env, "com/twitter/vireo/sound/PCM",
                      "(SBLcom/twitter/vireo/common/Data;)V",
                      pcm.size(), pcm.channels(), *jni_byte_data);
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_decode_jni_Audio_freePcm(JNIEnv* env, jobject decoder_obj, jlong _jni, jobject short_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIAudioDecodeStruct*)_jni;
    CHECK(jni && jni->decoder);

    auto jni_short_data = jni::Wrap(env, short_data_obj);
    jobject short_buffer_obj = jni_short_data.call<jobject>("shortBuffer", "()Ljava/nio/ShortBuffer;");
    const void* ptr = env->GetDirectBufferAddress(short_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

// Video

struct _JNIVideoDecodeStruct : public jni::Struct<common::Data32> {
  mutex lock;
  unique_ptr<decode::Video> decoder;
  map<uint32_t, frame::Frame> frame_funcs;
  ~_JNIVideoDecodeStruct() {
    CHECK(empty());
  }
};

void JNICALL Java_com_twitter_vireo_decode_jni_Video_jniInit(JNIEnv* env, jobject video_obj, jobject samples_obj, jint thread_count) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = new _JNIVideoDecodeStruct();
    auto jni_video = jni::Wrap(env, video_obj);
    jni_video.set<jlong>("jni", (jlong)jni);

    vector<decode::Sample> sample_funcs = jni::createVectorFromMedia(env, samples_obj, function<decode::Sample(jobject)>([env](jobject sample_obj) -> decode::Sample {
      auto jni_sample = jni::Wrap(env, sample_obj);
      auto jni_nal_func = jni::Wrap(env, jni_sample.get("nal", "Lscala/Function0;"));
      PREVENT_IMMEDIATE_COLLECTION_OF(jni_nal_func);

      return (decode::Sample){
        (int64_t)jni_sample.get<jlong>("pts"),
        (int64_t)jni_sample.get<jlong>("dts"),
        (bool)jni_sample.get<jboolean>("keyframe"),
        (SampleType)jni_sample.get<jbyte>("sampleType"),
        jni::createFunc<common::Data32>(env, *jni_nal_func)
      };
    }));

    auto jni_samples = jni::Wrap(env, samples_obj);
    auto settings_obj = jni_samples.get("settings", "Ljava/lang/Object;");
    const auto settings = jni::createVideoSettings(env, settings_obj);

    auto samples = functional::Video<decode::Sample>(sample_funcs, settings);

    jni->decoder.reset(new decode::Video(samples, (int)thread_count));

    jni_video.set<jint>("a", jni->decoder->a());
    jni_video.set<jint>("b", jni->decoder->b());

    auto this_settings = jni->decoder->settings();
    THROW_IF(this_settings.par_width != this_settings.par_height, InvalidArguments, "Non square decoded frame pixel is not supported");

    setVideoSettings(env, jni_video, jni->decoder->settings());
  }, [env, video_obj] {
    Java_com_twitter_vireo_decode_jni_Video_jniClose(env, video_obj);
  });
}

void JNICALL Java_com_twitter_vireo_decode_jni_Video_jniClose(JNIEnv* env, jobject video_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_video = jni::Wrap(env, video_obj);
    auto jni = (_JNIVideoDecodeStruct*)jni_video.get<jlong>("jni");
    delete jni;
    jni_video.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_decode(JNIEnv* env, jobject video_obj, jlong _jni, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIVideoDecodeStruct*)_jni;
    CHECK(jni && jni->decoder);
    auto& decoder = *jni->decoder;
    auto frame = decoder(index);
    jni->frame_funcs[(uint32_t)index] = move(frame);
    return *jni::Wrap(env, "com/twitter/vireo/decode/jni/Video$Frame", "(Lcom/twitter/vireo/decode/jni/Video;JI)V",
                      video_obj, (jlong)frame.pts, (jint)index);
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_yuv(JNIEnv* env, jobject video_obj, jlong _jni, jint sample, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIVideoDecodeStruct*)_jni;
    CHECK(jni);

    lock_guard<mutex>(jni->lock);
    THROW_IF(sample >= jni->decoder->count(), OutOfRange);
    const auto found = jni->frame_funcs.find(sample);
    CHECK(found != jni->frame_funcs.end());
    const auto yuv_func = found->second.yuv;
    THROW_IF(!yuv_func, InvalidArguments, "YUV representation is unavailable for this frame");
    const auto yuv = yuv_func();

    vector<jobject> planes;
    for (auto p: enumeration::Enum<frame::PlaneIndex>(frame::Y, frame::V)) {
      auto& plane = yuv.plane(p);

      auto jni_byte_data = jni::create(env, jni, move(plane.bytes()), copy);
      auto jni_plane = jni::Wrap(env, "com/twitter/vireo/frame/Plane", "(SSSLcom/twitter/vireo/common/Data;)V",
                                 plane.row(),
                                 plane.width(),
                                 plane.height(),
                                 *jni_byte_data);
      planes.push_back(*jni_plane);
    }

    return *jni::Wrap(env, "com/twitter/vireo/frame/YUV",
                      "(Lcom/twitter/vireo/frame/Plane;Lcom/twitter/vireo/frame/Plane;Lcom/twitter/vireo/frame/Plane;Z)V",
                      planes[0], planes[1], planes[2], (jboolean)yuv.full_range());
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_decode_jni_Video_freeYuv(JNIEnv* env, jobject video_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIVideoDecodeStruct*)_jni;
    CHECK(jni && jni->decoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");
    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}

jobject JNICALL Java_com_twitter_vireo_decode_jni_Video_rgb(JNIEnv* env, jobject video_obj, jlong _jni, jint sample, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIVideoDecodeStruct*)_jni;
    CHECK(jni);

    lock_guard<mutex>(jni->lock);
    THROW_IF(sample >= jni->decoder->count(), OutOfRange);
    const auto found = jni->frame_funcs.find(sample);
    CHECK(found != jni->frame_funcs.end());
    const auto rgb_func = found->second.rgb;
    THROW_IF(!rgb_func, InvalidArguments, "RGB representation is unavailable for this frame");
    const auto rgb = rgb_func();

    auto& plane = rgb.plane();
    auto jni_byte_data = jni::create(env, jni, move(plane.bytes()), copy);
    auto jni_plane = jni::Wrap(env, "com/twitter/vireo/frame/Plane", "(SSSLcom/twitter/vireo/common/Data;)V",
                               plane.row(),
                               plane.width(),
                               plane.height(),
                               *jni_byte_data);
    return *jni::Wrap(env, "com/twitter/vireo/frame/RGB","(BLcom/twitter/vireo/frame/Plane;)V", (jbyte)rgb.component_count(), *jni_plane);
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_decode_jni_Video_freeRgb(JNIEnv* env, jobject video_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIVideoDecodeStruct*)_jni;
    CHECK(jni && jni->decoder);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");
    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}
