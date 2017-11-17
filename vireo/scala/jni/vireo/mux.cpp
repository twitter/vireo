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

#include <fstream>
#include <mutex>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/common/path.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp2ts.h"
#include "vireo/mux/mp4.h"
#include "vireo/mux/webm.h"
#include "vireo/util/util.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/mux.h"
#include "vireo/scala/jni/vireo/util.h"

using std::lock_guard;
using std::mutex;
using std::ofstream;

using namespace vireo;

// MP2TS Implementation

struct _JNIMP2TSEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<mux::MP2TS> encoder;
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
  functional::Caption<encode::Sample> caption;
};

void JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_jniInit(JNIEnv* env, jobject mp2ts_obj, jobject audio_obj, jobject video_obj, jobject caption_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_mp2ts = jni::Wrap(env, mp2ts_obj);
    auto jni = new _JNIMP2TSEncodeStruct();
    jni_mp2ts.set<jlong>("jni", (jlong)jni);

    auto jni_audio = jni::Wrap(env, audio_obj);

    auto audio_settings_obj = jni_audio.get("settings", "Ljava/lang/Object;");
    auto audio_settings = audio_settings_obj ? jni::createAudioSettings(env, audio_settings_obj) : settings::Audio::None;

    jni->audio = functional::Audio<encode::Sample>([env, jni, jni_audio](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_audio.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, (uint32_t)jni_audio.get<jint>("a"), (uint32_t)jni_audio.get<jint>("b"), audio_settings);

    auto jni_video = jni::Wrap(env, video_obj);

    auto video_settings_obj = jni_video.get("settings", "Ljava/lang/Object;");
    auto video_settings = video_settings_obj ? jni::createVideoSettings(env, video_settings_obj) : settings::Video::None;

    jni->video = functional::Video<encode::Sample>([env, jni, jni_video](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_video.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, (uint32_t)jni_video.get<jint>("a"), (uint32_t)jni_video.get<jint>("b"), video_settings);

    auto jni_caption = jni::Wrap(env, caption_obj);

    auto caption_settings_obj = jni_caption.get("settings", "Ljava/lang/Object;");
    auto caption_settings = caption_settings_obj ? jni::createCaptionSettings(env, caption_settings_obj) : settings::Caption::None;

    jni->caption = functional::Caption<encode::Sample>([env, jni, jni_caption](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_caption.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, (uint32_t)jni_caption.get<jint>("a"), (uint32_t)jni_caption.get<jint>("b"), caption_settings);
  }, [env, mp2ts_obj] {
    Java_com_twitter_vireo_mux_jni_MP2TS_jniClose(env, mp2ts_obj);
  });
}

void JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_jniClose(JNIEnv* env, jobject mp2ts_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_mp2ts(env, mp2ts_obj);
    auto jni = (_JNIMP2TSEncodeStruct*)jni_mp2ts.get<jlong>("jni");
    delete jni;
    jni_mp2ts.set<jlong>("jni", 0);
  });
}

static void _CreateMP2TSEncoder(JNIEnv* env, jobject mp2ts_obj) {
  jni::Wrap jni_mp2ts(env, mp2ts_obj);
  auto jni = (_JNIMP2TSEncodeStruct*)jni_mp2ts.get<jlong>("jni");
  CHECK(jni && !jni->encoder);

  jni->encoder.reset(new mux::MP2TS(jni->audio, jni->video, jni->caption));
}

jobject JNICALL Java_com_twitter_vireo_mux_jni_MP2TS_encode(JNIEnv* env, jobject mp2ts_obj, jlong _jni) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIMP2TSEncodeStruct*)_jni;
    CHECK(jni);
    if (!jni->encoder) {
      _CreateMP2TSEncoder(env, mp2ts_obj);
    }

    auto data = (*jni->encoder)();

    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);

    return *jni_byte_data;
  }, NULL);
}

// MP4 Implementation

struct _JNIMP4EncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<mux::MP4> encoder;
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
  functional::Caption<encode::Sample> caption;
  vector<common::EditBox> edit_boxes;
  FileFormat file_format;
};

void JNICALL Java_com_twitter_vireo_mux_jni_MP4_jniInit(JNIEnv* env, jobject mp4_obj, jobject audio_obj, jobject video_obj, jobject caption_obj, jobject edit_boxes_obj, jbyte file_format) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_mp4 = jni::Wrap(env, mp4_obj);
    auto jni = new _JNIMP4EncodeStruct();
    jni_mp4.set<jlong>("jni", (jlong)jni);

    auto jni_audio = jni::Wrap(env, audio_obj);
    const uint32_t audio_a = (uint32_t)jni_audio.get<jint>("a");
    const uint32_t audio_b = (uint32_t)jni_audio.get<jint>("b");

    auto audio_settings_obj = jni_audio.get("settings", "Ljava/lang/Object;");
    auto audio_settings = audio_settings_obj ? jni::createAudioSettings(env, audio_settings_obj) : settings::Audio::None;

    jni->audio = functional::Audio<encode::Sample>([env, jni, jni_audio = move(jni_audio)](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_audio.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, audio_a, audio_b, audio_settings);

    auto jni_video = jni::Wrap(env, video_obj);
    const uint32_t video_a = (uint32_t)jni_video.get<jint>("a");
    const uint32_t video_b = (uint32_t)jni_video.get<jint>("b");

    auto video_settings_obj = jni_video.get("settings", "Ljava/lang/Object;");
    auto video_settings = video_settings_obj ? jni::createVideoSettings(env, video_settings_obj) : settings::Video::None;

    jni->video = functional::Video<encode::Sample>([env, jni, jni_video = move(jni_video)](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_video.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, video_a, video_b, video_settings);

    auto jni_caption = jni::Wrap(env, caption_obj);
    const uint32_t caption_a = (uint32_t)jni_caption.get<jint>("a");
    const uint32_t caption_b = (uint32_t)jni_caption.get<jint>("b");

    auto caption_settings_obj = jni_caption.get("settings", "Ljava/lang/Object;");
    auto caption_settings = caption_settings_obj ? jni::createCaptionSettings(env, caption_settings_obj) : settings::Caption::None;

    jni->caption = functional::Caption<encode::Sample>([env, jni, jni_caption = move(jni_caption)](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_caption.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, caption_a, caption_b, caption_settings);

    jni->edit_boxes = jni::createVectorFromSeq<common::EditBox>(env, edit_boxes_obj, function<common::EditBox(jobject)>([env](jobject edit_box_obj) -> common::EditBox {
      auto jni_edit_box = jni::Wrap(env, edit_box_obj);
      return common::EditBox((int64_t)jni_edit_box.get<jlong>("startPts"),
                             (uint64_t)jni_edit_box.get<jlong>("durationPts"),
                             1.0f,
                             (SampleType)jni_edit_box.get<jbyte>("sampleType"));

    }));
    jni->file_format = (FileFormat)file_format;
  }, [env, mp4_obj] {
    Java_com_twitter_vireo_mux_jni_MP4_jniClose(env, mp4_obj);
  });
}

void JNICALL Java_com_twitter_vireo_mux_jni_MP4_jniClose(JNIEnv* env, jobject mp4_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_mp4(env, mp4_obj);
    auto jni = (_JNIMP4EncodeStruct*)jni_mp4.get<jlong>("jni");
    delete jni;
    jni_mp4.set<jlong>("jni", 0);
  });
}

static void _CreateMP4Encoder(JNIEnv* env, jobject mp4_obj) {
  jni::Wrap jni_mp4(env, mp4_obj);
  auto jni = (_JNIMP4EncodeStruct*)jni_mp4.get<jlong>("jni");
  CHECK(jni && !jni->encoder);

  jni->encoder.reset(new mux::MP4(jni->audio, jni->video, jni->caption, jni->edit_boxes, jni->file_format));
}

jobject JNICALL Java_com_twitter_vireo_mux_jni_MP4_encode(JNIEnv* env, jobject mp4_obj, jlong _jni, jbyte file_format) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIMP4EncodeStruct*)_jni;
    CHECK(jni);
    if (!jni->encoder) {
      _CreateMP4Encoder(env, mp4_obj);
    }

    auto data = (*jni->encoder)((FileFormat)file_format);
    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);

    return *jni_byte_data;
  }, NULL);
}

// WebM Implementation

struct _JNIWebMEncodeStruct : public jni::Struct<common::Data32> {
  unique_ptr<mux::WebM> encoder;
  functional::Audio<encode::Sample> audio;
  functional::Video<encode::Sample> video;
};

void JNICALL Java_com_twitter_vireo_mux_jni_WebM_jniInit(JNIEnv* env, jobject webm_obj, jobject audio_obj, jobject video_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_webm = jni::Wrap(env, webm_obj);
    auto jni = new _JNIWebMEncodeStruct();
    jni_webm.set<jlong>("jni", (jlong)jni);

    auto jni_audio = jni::Wrap(env, audio_obj);
    const uint32_t audio_a = (uint32_t)jni_audio.get<jint>("a");
    const uint32_t audio_b = (uint32_t)jni_audio.get<jint>("b");

    auto audio_settings_obj = jni_audio.get("settings", "Ljava/lang/Object;");
    auto audio_settings = audio_settings_obj ? jni::createAudioSettings(env, audio_settings_obj) : settings::Audio::None;

    const uint32_t audio_count = (uint32_t)jni_audio.get<jint>("b");
    jni->audio = functional::Audio<encode::Sample>([env, jni, audio_count, jni_audio = move(jni_audio)](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_audio.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, audio_a, audio_b, audio_settings);

    auto jni_video = jni::Wrap(env, video_obj);
    const uint32_t video_a = (uint32_t)jni_video.get<jint>("a");
    const uint32_t video_b = (uint32_t)jni_video.get<jint>("b");

    auto video_settings_obj = jni_video.get("settings", "Ljava/lang/Object;");
    auto video_settings = video_settings_obj ? jni::createVideoSettings(env, video_settings_obj) : settings::Video::None;

    const uint32_t video_count = (uint32_t)jni_video.get<jint>("b");
    jni->video = functional::Video<encode::Sample>([env, jni, video_count, jni_video = move(jni_video)](uint32_t index) {
      jni::LocalFrame frame(env);
      return jni::createFunc<encode::Sample>(env, jni_video.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)index))();
    }, video_a, video_b, video_settings);
 }, [env, webm_obj] {
    Java_com_twitter_vireo_mux_jni_WebM_jniClose(env, webm_obj);
  });
}

void JNICALL Java_com_twitter_vireo_mux_jni_WebM_jniClose(JNIEnv* env, jobject webm_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    jni::Wrap jni_webm(env, webm_obj);
    auto jni = (_JNIWebMEncodeStruct*)jni_webm.get<jlong>("jni");
    delete jni;
    jni_webm.set<jlong>("jni", 0);
  });
}

static void _CreateWebMEncoder(JNIEnv* env, jobject webm_obj) {
  jni::Wrap jni_webm(env, webm_obj);
  auto jni = (_JNIWebMEncodeStruct*)jni_webm.get<jlong>("jni");
  CHECK(jni && !jni->encoder);

  jni->encoder.reset(new mux::WebM(jni->audio, jni->video));
}

jobject JNICALL Java_com_twitter_vireo_mux_jni_WebM_encode(JNIEnv* env, jobject webm_obj, jlong _jni) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNIWebMEncodeStruct*)_jni;
    CHECK(jni);
    if (!jni->encoder) {
      _CreateWebMEncoder(env, webm_obj);
    }

    auto data = (*jni->encoder)();

    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);

    return *jni_byte_data;
  }, NULL);
}
