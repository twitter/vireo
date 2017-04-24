//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <mutex>

#include "vireo/base_cpp.h"
#include "vireo/decode/types.h"
#include "vireo/error/error.h"
#include "vireo/encode/types.h"
#include "vireo/frame/frame.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/sound/sound.h"

using std::mutex;
using std::lock_guard;

namespace vireo {
namespace jni {

template <typename DATA>
struct _Struct {
  mutex lock;
  map<const void*, tuple<DATA, Wrap>> buffers;
};

template <typename DATA>
Struct<DATA>::Struct()
  : _this(new _Struct<DATA>()) {
}

template <typename DATA>
Struct<DATA>::~Struct() {
  CHECK(_this);
  {
    lock_guard<mutex> lock(_this->lock);
    for (auto& buffer: _this->buffers) {
      Wrap& jni_byte_data = get<1>(buffer.second);
      jni_byte_data.set<jboolean>("initialized", false);
    }
  }
  delete _this;
}

template <typename DATA>
void Struct<DATA>::add_buffer_ref(DATA&& data, const Wrap& wrap) {
  lock_guard<mutex> lock(_this->lock);
  const void* ptr = (const void*)(data.data() + data.a()); // store so it's not affected by move()
  _this->buffers.insert(make_pair(ptr, make_tuple(move(data), wrap)));
}

template <typename DATA>
void Struct<DATA>::remove_buffer_ref(const void* data_ptr) {
  lock_guard<mutex>(_this->lock);
  const auto found = _this->buffers.find((void*)data_ptr);
  CHECK(found != _this->buffers.end());
  _this->buffers.erase(found);
}

template <typename DATA>
bool Struct<DATA>::empty() const {
  return _this->buffers.empty();
}

// Explicit instantiations
template class Struct<common::Data32>;
template class Struct<common::Sample16>;

Wrap create(JNIEnv* env, const frame::Plane& plane) {
  jbyteArray array_obj = env->NewByteArray(plane.bytes().count());
  env->SetByteArrayRegion(array_obj, 0, (jsize)plane.bytes().count(), (const jbyte*)plane.bytes().data());
  auto jni_byte_data = Wrap(env, "com/twitter/vireo/common/ByteData", "([B)V", array_obj);
  return Wrap(env, "com/twitter/vireo/frame/Plane", "(SSSLcom/twitter/vireo/common/Data;)V",
              plane.row(),
              plane.width(),
              plane.height(),
              *jni_byte_data);
}

Wrap create(JNIEnv* env, const frame::RGB& rgb) {
  return Wrap(env, "com/twitter/vireo/frame/RGB", "(BLcom/twitter/vireo/frame/Plane;)V",
              (jbyte)rgb.component_count(), *create(env, rgb.plane()));
}

Wrap create(JNIEnv* env, const frame::YUV& yuv) {
  auto& y_plane = yuv.plane(frame::PlaneIndex::Y);
  auto jni_y = create(env, y_plane);

  auto& u_plane = yuv.plane(frame::PlaneIndex::U);
  auto jni_u = create(env, u_plane);

  auto& v_plane = yuv.plane(frame::PlaneIndex::V);
  auto jni_v = create(env, v_plane);

  auto jni_yuv = Wrap(env, "com/twitter/vireo/frame/YUV",
                      "(Lcom/twitter/vireo/frame/Plane;Lcom/twitter/vireo/frame/Plane;Lcom/twitter/vireo/frame/Plane;Z)V",
                      *jni_y, *jni_u, *jni_v, (jboolean)yuv.full_range());

  return jni_yuv;
}

Wrap create(JNIEnv* env, const sound::PCM& pcm) {
  auto samples_array_object = env->NewShortArray(pcm.samples().count());
  env->SetShortArrayRegion(samples_array_object, 0, pcm.samples().count(), (jshort*)pcm.samples().data());
  return Wrap(env, "com/twitter/vireo/sound/PCM", "(SBLcom/twitter/vireo/common/Data;)V",
              pcm.size(), pcm.channels(), samples_array_object);
}

template <>
Wrap create(JNIEnv* env, jni::Struct<common::Data32>* jni, common::Data32&& data, bool copy) {
  if (!copy) {
    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count());
    auto jni_byte_data = jni::Wrap(env, "com/twitter/vireo/common/ByteData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_byte_data);
    return jni_byte_data;
  } else {
    jbyteArray array_obj = env->NewByteArray((jsize)data.count());
    env->SetByteArrayRegion(array_obj, 0, (jsize)data.count(), (const jbyte*)(data.data() + data.a()));
    return jni::Wrap(env, "com/twitter/vireo/common/ByteData", "([B)V", array_obj);
  }
}

template <>
Wrap create(JNIEnv* env, jni::Struct<common::Sample16>* jni, common::Sample16&& data, bool copy) {
  if (!copy) {
    jobject byte_buffer_obj = env->NewDirectByteBuffer((void*)(data.data() + data.a()), (jlong)data.count() * sizeof(short));
    auto jni_short_data = jni::Wrap(env, "com/twitter/vireo/common/ShortData", "(Ljava/nio/ByteBuffer;)V", byte_buffer_obj);
    jni->add_buffer_ref(move(data), jni_short_data);
    return jni_short_data;
  } else {
    jshortArray array_obj = env->NewShortArray((jsize)data.count());
    env->SetShortArrayRegion(array_obj, 0, (jsize)data.count(), (const jshort*)(data.data() + data.a()));
    return jni::Wrap(env, "com/twitter/vireo/common/ShortData", "([S)V", array_obj);
  }
}

template <typename DATA>
static auto _CreateDataFunc(JNIEnv* env, jobject byte_data_func_obj) -> std::function<DATA(void)> {
  CHECK(byte_data_func_obj);
  auto jni_byte_data_func = Wrap(env, byte_data_func_obj);
  PREVENT_IMMEDIATE_COLLECTION_OF(jni_byte_data_func);
  return [env, jni_byte_data_func = move(jni_byte_data_func)]() -> DATA {
    bool has_cleaner = false;
    jobject byte_data_obj = nullptr;
    if (jni_byte_data_func.is_subclass_of_class_named("com/twitter/vireo/common/Native")) {
      has_cleaner = true;
      byte_data_obj = jni_byte_data_func.call<jobject>("apply", "(Z)Ljava/lang/Object;", false);
    } else {
      byte_data_obj = jni_byte_data_func.call<jobject>("apply", "()Ljava/lang/Object;");
    }
    if (!byte_data_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(byte_data_obj);
    return createData<DATA>(env, byte_data_obj, jni_byte_data_func, has_cleaner);
  };
}

template <>
std::function<common::Data32(void)> createFunc(JNIEnv* env, jobject byte_data_func_obj) {
  return _CreateDataFunc<common::Data32>(env, byte_data_func_obj);
}

template <>
std::function<frame::YUV(void)> createFunc(JNIEnv* env, jobject yuv_func_obj) {
  CHECK(yuv_func_obj);
  auto jni_yuv_func = Wrap(env, yuv_func_obj);
  PREVENT_IMMEDIATE_COLLECTION_OF(jni_yuv_func);
  return [env, jni_yuv_func = move(jni_yuv_func)]() -> frame::YUV {
    bool has_cleaner = false;
    jobject yuv_obj = nullptr;
    if (jni_yuv_func.is_subclass_of_class_named("com/twitter/vireo/common/Native")) {
      has_cleaner = true;
      yuv_obj = jni_yuv_func.call<jobject>("apply", "(Z)Lcom/twitter/vireo/frame/YUV;", false);
    } else {
      yuv_obj = jni_yuv_func.call<jobject>("apply", "()Lcom/twitter/vireo/frame/YUV;");
    }
    if (!yuv_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(yuv_obj);
    return createYUV(env, yuv_obj, jni_yuv_func, has_cleaner);
  };
}

template <>
std::function<frame::RGB(void)> createFunc(JNIEnv* env, jobject rgb_func_obj) {
  CHECK(rgb_func_obj);
  auto jni_rgb_func = Wrap(env, rgb_func_obj);
  PREVENT_IMMEDIATE_COLLECTION_OF(jni_rgb_func);
  return [env, jni_rgb_func = move(jni_rgb_func)]() -> frame::RGB {
    bool has_cleaner = false;
    jobject rgb_obj = nullptr;
    if (jni_rgb_func.is_subclass_of_class_named("com/twitter/vireo/common/Native")) {
      has_cleaner = true;
      rgb_obj = jni_rgb_func.call<jobject>("apply", "(Z)Lcom/twitter/vireo/frame/RGB;", false);
    } else {
      rgb_obj = jni_rgb_func.call<jobject>("apply", "()Lcom/twitter/vireo/frame/RGB;");
    }
    if (!rgb_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(rgb_obj);
    return createRGB(env, rgb_obj, jni_rgb_func, has_cleaner);
  };
}

template <>
std::function<sound::PCM(void)> createFunc(JNIEnv* env, jobject pcm_func_obj) {
  CHECK(pcm_func_obj);
  auto jni_pcm_func = Wrap(env, pcm_func_obj);
  PREVENT_IMMEDIATE_COLLECTION_OF(jni_pcm_func);
  return [env, jni_pcm_func = move(jni_pcm_func)]() -> sound::PCM {
    bool has_cleaner = false;
    jobject pcm_obj = nullptr;
    if (jni_pcm_func.is_subclass_of_class_named("com/twitter/vireo/common/Native")) {
      has_cleaner = true;
      pcm_obj = jni_pcm_func.call<jobject>("apply", "(Z)Lcom/twitter/vireo/sound/PCM;", false);
    } else {
      pcm_obj = jni_pcm_func.call<jobject>("apply", "()Lcom/twitter/vireo/sound/PCM;");
    }
    if (!pcm_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(pcm_obj);
    return createPCM(env, pcm_obj, jni_pcm_func, has_cleaner);
  };
}

template <typename DATA, typename TYPE>
static auto _CreateData(JNIEnv* env, jobject byte_data_obj, const jni::Wrap& jni_native, bool has_cleaner) -> DATA {
  CHECK(byte_data_obj);
  auto jni_byte_data = Wrap(env, byte_data_obj, has_cleaner ? (sizeof(TYPE) == 1 ? "com/twitter/vireo/common/ByteData" : "com/twitter/vireo/common/ShortData") : nullptr);
  auto byte_buffer_obj = sizeof(TYPE) == 1 ? jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;") :  jni_byte_data.call<jobject>("shortBuffer", "()Ljava/nio/ShortBuffer;");
  if (!byte_buffer_obj) {
    jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
  }
  CHECK(byte_buffer_obj);
  const jlong dbb_capacity = env->GetDirectBufferCapacity(byte_buffer_obj);
  if (dbb_capacity >= 0) {  // Is direct byte buffer
    auto jni_byte_buffer = Wrap(env, byte_buffer_obj);
    auto position = jni_byte_buffer.call<jint>("position", "()I");
    auto length = jni_byte_buffer.call<jint>("remaining", "()I");
    if (length > 0) {
      if (has_cleaner) {
        return DATA((const TYPE*)env->GetDirectBufferAddress(byte_buffer_obj) + position, (uint32_t)length, [env=env, jni_native = move(jni_native), jni_byte_data = move(jni_byte_data)](TYPE* p) {
          jni_native.call<void>("cleaner", "(Lcom/twitter/vireo/common/Data;)V", *jni_byte_data);
        });
      } else {
        return DATA((const TYPE*)env->GetDirectBufferAddress(byte_buffer_obj) + position, (uint32_t)length, [env=env, jni_byte_buffer = move(jni_byte_buffer)](TYPE* p) {});
      }
    }
  } else {
    DATA data = jni_byte_data.call<DATA>("array", sizeof(TYPE) == 1 ? "()[B" : "()[S");
    if(data.count()) {
      return data;
    } else {
      auto size = (uint32_t)jni_byte_data.call<jint>("size", "()I");
      if (size) {
        return DATA(size);
      }
    }
  }
  return DATA();
}

template <>
common::Sample16 createData(JNIEnv* env, jobject byte_data_obj, const jni::Wrap& jni_native, bool has_cleaner) {
  return _CreateData<common::Sample16, int16_t>(env, byte_data_obj, jni_native, has_cleaner);
}

template <>
common::Data32 createData(JNIEnv* env, jobject byte_data_obj, const jni::Wrap& jni_native, bool has_cleaner) {
  return _CreateData<common::Data32, uint8_t>(env, byte_data_obj, jni_native, has_cleaner);
}

template <>
std::function<encode::Sample(void)> createFunc(JNIEnv* env, jobject sample_func_obj) {
  CHECK(sample_func_obj);
  auto jni_sample_func = Wrap(env, sample_func_obj);
  PREVENT_IMMEDIATE_COLLECTION_OF(jni_sample_func);
  return [env, jni_sample_func = move(jni_sample_func)]() -> encode::Sample {
    bool has_cleaner = false;
    jobject sample_obj = nullptr;
    if (jni_sample_func.is_subclass_of_class_named("com/twitter/vireo/common/Native")) {
      has_cleaner = true;
      sample_obj = jni_sample_func.call<jobject>("apply", "(Z)Lcom/twitter/vireo/encode/Sample;", false);
    } else {
      sample_obj = jni_sample_func.call<jobject>("apply", "()Lcom/twitter/vireo/encode/Sample;");
    }
    if (!sample_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(sample_obj);
    auto jni_sample = Wrap(env, sample_obj);
    jobject nal_obj = jni_sample.get("nal", "Lcom/twitter/vireo/common/Data;");
    auto sample = encode::Sample((int64_t)jni_sample.get<jlong>("pts"),
                                 (int64_t)jni_sample.get<jlong>("dts"),
                                 (bool)jni_sample.get<jboolean>("keyframe"),
                                 (SampleType)jni_sample.get<jbyte>("sampleType"),
                                 createData<common::Data32>(env, nal_obj, jni_sample_func, has_cleaner));
    return sample;
  };
}

sound::PCM createPCM(JNIEnv* env, jobject pcm_object, const jni::Wrap& jni_native, bool has_cleaner) {
  CHECK(pcm_object);
  Wrap jni_pcm(env, pcm_object);
  return sound::PCM(jni_pcm.get<jshort>("size"),
                    jni_pcm.get<jbyte>("channels"),
                    createData<common::Sample16>(env, jni_pcm.get("samples", "Lcom/twitter/vireo/common/Data;"), jni_native, has_cleaner));
}

frame::RGB createRGB(JNIEnv* env, jobject rgb_obj, const jni::Wrap& jni_native, bool has_cleaner) {
  CHECK(rgb_obj);
  Wrap jni_rgb(env, rgb_obj);
  Wrap jni_rgb_plane(env, jni_rgb.call<jobject>("plane", "()Lcom/twitter/vireo/frame/Plane;"));
  auto rgb_plane = frame::Plane(jni_rgb_plane.get<jshort>("row"),
                                jni_rgb_plane.get<jshort>("width"),
                                jni_rgb_plane.get<jshort>("height"),
                                createData<common::Data32>(env, jni_rgb_plane.get("bytes", "Lcom/twitter/vireo/common/Data;"), jni_native, has_cleaner));
  return frame::RGB(jni_rgb.get<jbyte>("component_count"), move(rgb_plane));
}

frame::YUV createYUV(JNIEnv* env, jobject yuv_obj, const jni::Wrap& jni_native, bool has_cleaner) {
  CHECK(yuv_obj);
  Wrap jni_yuv(env, yuv_obj);
  Wrap jni_y(env, jni_yuv.call<jobject>("y", "()Lcom/twitter/vireo/frame/Plane;"));
  Wrap jni_u(env, jni_yuv.call<jobject>("u", "()Lcom/twitter/vireo/frame/Plane;"));
  Wrap jni_v(env, jni_yuv.call<jobject>("v", "()Lcom/twitter/vireo/frame/Plane;"));
  auto y = frame::Plane(jni_y.get<jshort>("row"),
                        jni_y.get<jshort>("width"),
                        jni_y.get<jshort>("height"),
                        createData<common::Data32>(env, jni_y.get("bytes", "Lcom/twitter/vireo/common/Data;"), jni_native, has_cleaner));
  auto u = frame::Plane(jni_u.get<jshort>("row"),
                        jni_u.get<jshort>("width"),
                        jni_u.get<jshort>("height"),
                        createData<common::Data32>(env, jni_u.get("bytes", "Lcom/twitter/vireo/common/Data;"), jni_native, has_cleaner));
  auto v = frame::Plane(jni_v.get<jshort>("row"),
                        jni_v.get<jshort>("width"),
                        jni_v.get<jshort>("height"),
                        createData<common::Data32>(env, jni_v.get("bytes", "Lcom/twitter/vireo/common/Data;"), jni_native, has_cleaner));
  auto full_range = (bool)jni_yuv.get<jboolean>("fullRange");
  return frame::YUV(move(y), move(u), move(v), full_range);
}

settings::Audio createAudioSettings(JNIEnv* env, jobject audio_settings_object) {
  CHECK(audio_settings_object);
  Wrap jni_audio_settings(env, audio_settings_object);
  return (settings::Audio) {
    (settings::Audio::Codec)jni_audio_settings.get<jbyte>("codec"),
    (uint32_t)jni_audio_settings.get<jint>("timescale"),
    (uint32_t)jni_audio_settings.get<jint>("sampleRate"),
    (uint8_t)jni_audio_settings.get<jbyte>("channels"),
    (uint32_t)jni_audio_settings.get<jint>("bitrate")
  };
}

settings::Video createVideoSettings(JNIEnv* env, jobject video_settings_object) {
  CHECK(video_settings_object);
  Wrap jni_video_settings(env, video_settings_object);
  Wrap jni_sps_pps(env, jni_video_settings.get("spsPps", "Lcom/twitter/vireo/header/SPS_PPS;"));
  auto s = (settings::Video){
    (settings::Video::Codec)jni_video_settings.get<jbyte>("codec"),
    (uint16_t)jni_video_settings.get<jshort>("codedWidth"),
    (uint16_t)jni_video_settings.get<jshort>("codedHeight"),
    (uint16_t)jni_video_settings.get<jshort>("parWidth"),
    (uint16_t)jni_video_settings.get<jshort>("parHeight"),
    (uint32_t)jni_video_settings.get<jint>("timescale"),
    (settings::Video::Orientation)jni_video_settings.get<jbyte>("orientation"),
    (const header::SPS_PPS){ jni_sps_pps.get<common::Data16>("sps"),
                             jni_sps_pps.get<common::Data16>("pps"),
                             (uint8_t)jni_sps_pps.get<jbyte>("naluLengthSize") },
  };
  return s;
}

settings::Caption createCaptionSettings(JNIEnv* env, jobject caption_settings_object) {
  CHECK(caption_settings_object);
  Wrap jni_caption_settings(env, caption_settings_object);
  return (settings::Caption) {
    (settings::Caption::Codec)jni_caption_settings.get<jbyte>("codec"),
    (uint32_t)jni_caption_settings.get<jint>("timescale")
  };
}

template <typename T>
vector<T> createVectorFromMedia(JNIEnv* env, jobject media_obj, function<T(jobject elem_obj)> convert_function) {
  vector<T> elems;
  auto jni_media = Wrap(env, media_obj);
  const uint32_t a = jni_media.get<jint>("a");
  const uint32_t b = jni_media.get<jint>("b");
  for (uint32_t i = a; i < b; ++i) {
    auto elem_obj = jni_media.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)i);
    if (!elem_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(elem_obj);
    elems.push_back(convert_function(elem_obj));
  }
  return elems;
}

template vector<decode::Sample> createVectorFromMedia<decode::Sample>(JNIEnv* env, jobject samples_obj, function<decode::Sample(jobject sample_obj)> convert_function);

template vector<sound::Sound> createVectorFromMedia<sound::Sound>(JNIEnv* env, jobject sounds_obj, function<sound::Sound(jobject sound_obj)> convert_function);

template vector<frame::Frame> createVectorFromMedia<frame::Frame>(JNIEnv* env, jobject frames_obj, function<frame::Frame(jobject frame_obj)> convert_function);

template <typename T>
vector<T> createVectorFromSeq(JNIEnv* env, jobject seq_obj, function<T(jobject elem_obj)> convert_function) {
  vector<T> elems;
  auto jni_seq = jni::Wrap(env, seq_obj);
  auto size = jni_seq.call<jint>("length", "()I");
  for (auto i = 0; i < size; ++i) {
    auto elem_obj = jni_seq.call<jobject>("apply", "(I)Ljava/lang/Object;", (jint)i);
    if (!elem_obj) {
      jni::ExceptionHandler::CatchJavaExceptionThrowNativeException(env);
    }
    CHECK(elem_obj);
    auto jni_elem = jni::Wrap(env, elem_obj);
    elems.push_back(convert_function(elem_obj));
  }
  return elems;
}

template vector<common::EditBox> createVectorFromSeq<common::EditBox>(JNIEnv* env, jobject edit_boxes_obj, function<common::EditBox(jobject)> convert_function);

template vector<vector<common::EditBox>> createVectorFromSeq<vector<common::EditBox>>(JNIEnv* env, jobject edit_boxes_obj, function<vector<common::EditBox>(jobject)> convert_function);

template vector<functional::Audio<decode::Sample>> createVectorFromSeq<functional::Audio<decode::Sample>>(JNIEnv* env, jobject audio_tracks_obj, function<functional::Audio<decode::Sample>(jobject)> convert_function);

template vector<functional::Video<decode::Sample>> createVectorFromSeq<functional::Video<decode::Sample>>(JNIEnv* env, jobject video_tracks_obj, function<functional::Video<decode::Sample>(jobject)> convert_function);

template vector<functional::Caption<decode::Sample>> createVectorFromSeq<functional::Caption<decode::Sample>>(JNIEnv* env, jobject caption_tracks_obj, function<functional::Caption<decode::Sample>(jobject)> convert_function);

void setAudioSettings(JNIEnv* env, Wrap& jni, const settings::Audio& settings) {
  auto jni_settings = *Wrap(env, "com/twitter/vireo/settings/Audio", "(BIIBI)V",
                            (jbyte)settings.codec,
                            (jint)settings.timescale,
                            (jint)settings.sample_rate,
                            (jbyte)settings.channels,
                            (jint)settings.bitrate);

  jni.set("settings", "Ljava/lang/Object;", jni_settings);
}

void setVideoSettings(JNIEnv* env, Wrap& jni, const settings::Video& settings) {
  auto sps_pps = settings.sps_pps;

  auto sps_object = env->NewByteArray(sps_pps.sps.count());
  env->SetByteArrayRegion(sps_object, 0, sps_pps.sps.count(), (jbyte*)sps_pps.sps.data());

  auto pps_object = env->NewByteArray(sps_pps.pps.count());
  env->SetByteArrayRegion(pps_object, 0, sps_pps.pps.count(), (jbyte*)sps_pps.pps.data());

  auto jni_sps_pps = Wrap(env, "com/twitter/vireo/header/SPS_PPS", "([B[BB)V", sps_object, pps_object, sps_pps.nalu_length_size);

  auto jni_settings = *Wrap(env, "com/twitter/vireo/settings/Video", "(BSSSSIBLcom/twitter/vireo/header/SPS_PPS;)V",
                            (jbyte)settings.codec,
                            (jshort)settings.coded_width,
                            (jshort)settings.coded_height,
                            (jshort)settings.par_width,
                            (jshort)settings.par_height,
                            (jint)settings.timescale,
                            (jbyte)settings.orientation,
                            *jni_sps_pps);

  jni.set("settings", "Ljava/lang/Object;", jni_settings);
}

void setDataSettings(JNIEnv* env, Wrap& jni, const settings::Data& settings) {
  auto jni_settings = *Wrap(env, "com/twitter/vireo/settings/Data", "(BI)V",
                            (jbyte)settings.codec,
                            (jint)settings.timescale);
  jni.set("settings", "Ljava/lang/Object;", jni_settings);
}

void setCaptionSettings(JNIEnv* env, Wrap& jni, const settings::Caption& settings) {
  auto jni_settings = *Wrap(env, "com/twitter/vireo/settings/Caption", "(BI)V",
                            (jbyte)settings.codec,
                            (jint)settings.timescale);
  jni.set("settings", "Ljava/lang/Object;", jni_settings);
}

}}
