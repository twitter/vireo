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

#include <list>
#include <fstream>
#include <mutex>

#include "vireo/base_h.h"
#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/common/enum.hpp"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/error/error.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/demux.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/util/util.h"

using std::ifstream;
using std::list;
using std::mutex;
using std::lock_guard;

using namespace vireo;

// Movie Implementation

struct _JNIReader {
  JNIEnv* env;
  jni::Wrap jni_reader;
  _JNIReader(JNIEnv* env, jobject reader_obj) : env(env), jni_reader(jni::Wrap(env, reader_obj)) {}
};

struct _JNIMovieStruct : public jni::Struct<common::Data32> {
  mutex lock;
  unique_ptr<demux::Movie> movie;
  unique_ptr<_JNIReader> reader;
  map<tuple<SampleType, uint32_t>, function<common::Data32(void)>> nal_funcs;
  ~_JNIMovieStruct() {
    CHECK(empty());
  }
};

static void _InitMovieWithData(JNIEnv* env, jobject movie_obj, common::Data32&& data) {
  auto jni_movie = jni::Wrap(env, movie_obj);
  auto jni = (_JNIMovieStruct*)jni_movie.get<jlong>("jni");
  CHECK(jni && !jni->movie.get());

  jni->movie.reset(new demux::Movie(move(data)));

  jni::Wrap jni_movie_video_track = jni::Wrap(env, jni_movie.get("videoTrack", "Lcom/twitter/vireo/demux/Movie$VideoTrack;"));
  jni_movie_video_track.set<jint>("b", jni->movie->video_track.count());

  jni::Wrap jni_movie_audio_track = jni::Wrap(env, jni_movie.get("audioTrack", "Lcom/twitter/vireo/demux/Movie$AudioTrack;"));
  jni_movie_audio_track.set<jint>("b", jni->movie->audio_track.count());

  jni::Wrap jni_movie_data_track = jni::Wrap(env, jni_movie.get("dataTrack", "Lcom/twitter/vireo/demux/Movie$DataTrack;"));
  jni_movie_data_track.set<jint>("b", jni->movie->data_track.count());

  jni::Wrap jni_movie_caption_track = jni::Wrap(env, jni_movie.get("captionTrack", "Lcom/twitter/vireo/demux/Movie$CaptionTrack;"));
  jni_movie_caption_track.set<jint>("b", jni->movie->caption_track.count());

  setAudioSettings(env, jni_movie_audio_track, jni->movie->audio_track.settings());
  setVideoSettings(env, jni_movie_video_track, jni->movie->video_track.settings());
  setDataSettings(env, jni_movie_data_track, jni->movie->data_track.settings());
  setCaptionSettings(env, jni_movie_caption_track, jni->movie->caption_track.settings());
}

static void _InitMovieWithReader(JNIEnv* env, jobject movie_obj, jobject reader_obj) {
  auto jni_movie = jni::Wrap(env, movie_obj);
  auto jni = (_JNIMovieStruct*)jni_movie.get<jlong>("jni");
  CHECK(jni && !jni->movie.get());
  jni->reader.reset(new _JNIReader(env, reader_obj));

  const int32_t size = jni->reader->jni_reader.call<jint>("size", "()I");
  CHECK(size >= 0);
  _JNIReader* reader = jni->reader.get();
  auto read_func = [reader](const uint32_t offset, const uint32_t size) -> common::Data32 {
    CHECK(reader);
    JNIEnv* env = reader->env;
    THROW_IF(offset > numeric_limits<int32_t>::max(), Overflow);
    THROW_IF(size > numeric_limits<int32_t>::max(), Overflow);
    jobject byte_data_obj = reader->jni_reader.call<jobject>("read", "(II)Lcom/twitter/vireo/common/Data;", (jint)offset, (jint)size);
    THROW_IF(!byte_data_obj, ReaderError);
    return jni::createData<common::Data32>(env, byte_data_obj, reader->jni_reader, false);
  };
  jni->movie.reset(new demux::Movie(common::Reader(size, read_func)));

  jni::Wrap jni_movie_video_track = jni::Wrap(env, jni_movie.get("videoTrack", "Lcom/twitter/vireo/demux/Movie$VideoTrack;"));
  jni_movie_video_track.set<jint>("b", jni->movie->video_track.count());

  jni::Wrap jni_movie_audio_track = jni::Wrap(env, jni_movie.get("audioTrack", "Lcom/twitter/vireo/demux/Movie$AudioTrack;"));
  jni_movie_audio_track.set<jint>("b", jni->movie->audio_track.count());

  jni::Wrap jni_movie_data_track = jni::Wrap(env, jni_movie.get("dataTrack", "Lcom/twitter/vireo/demux/Movie$DataTrack;"));
  jni_movie_data_track.set<jint>("b", jni->movie->data_track.count());

  jni::Wrap jni_movie_caption_track = jni::Wrap(env, jni_movie.get("captionTrack", "Lcom/twitter/vireo/demux/Movie$CaptionTrack;"));
  jni_movie_caption_track.set<jint>("b", jni->movie->caption_track.count());

  setAudioSettings(env, jni_movie_audio_track, jni->movie->audio_track.settings());
  setVideoSettings(env, jni_movie_video_track, jni->movie->video_track.settings());
  setDataSettings(env, jni_movie_data_track, jni->movie->data_track.settings());
  setCaptionSettings(env, jni_movie_caption_track, jni->movie->caption_track.settings());
}

void JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniInit(JNIEnv* env, jobject movie_obj, jobject input_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&]{
    auto jni = new _JNIMovieStruct();
    auto jni_movie = jni::Wrap(env, movie_obj);
    jni_movie.set<jlong>("jni", (jlong)jni);

    auto jni_input = jni::Wrap(env, input_obj);
    if (jni_input.is_subclass_of_class_named("com/twitter/vireo/common/Data")) {
      auto data = jni::createData<common::Data32>(env, input_obj, jni_input, false);
      if (data.count()) {
        _InitMovieWithData(env, movie_obj, move(data));
      }
    } else {
      _InitMovieWithReader(env, movie_obj, input_obj);
    }
  }, [env, movie_obj] {
    Java_com_twitter_vireo_demux_jni_Movie_jniClose(env, movie_obj);
  });
}

void JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniClose(JNIEnv* env, jobject movie_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_movie = jni::Wrap(env, movie_obj);
    auto jni = (_JNIMovieStruct*)jni_movie.get<jlong>("jni");
    delete jni;
    jni_movie.set<jlong>("jni", 0);
  });
}

jbyte JNICALL Java_com_twitter_vireo_demux_jni_Movie_jniFileType(JNIEnv* env, jobject movie_obj, jlong _jni) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jbyte>(env, [&] {
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie.get());
    auto& decoder = *jni->movie;
    return (jbyte)decoder.file_type();
  }, (jbyte)FileType::UnknownFileType);
}

jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_decode(JNIEnv* env, jobject movie_obj, jlong _jni, jbyte sample_type, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio && type != SampleType::Data && type != SampleType::Caption, InvalidArguments);
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie.get());
    auto& decoder = *jni->movie;
    lock_guard<mutex>(jni->lock);
    decode::Sample sample = [type, index, &decoder]() -> decode::Sample {
      if (type == SampleType::Video) {
        return decoder.video_track(index);
      } else if (type == SampleType::Audio) {
        return decoder.audio_track(index);
      } else if (type == SampleType::Data) {
        return decoder.data_track(index);
      } else {
        CHECK(type == SampleType::Caption)
        return decoder.caption_track(index);
      }
    }();
    jni->nal_funcs[make_tuple((SampleType)sample_type, index)] = move(sample.nal);
    auto jni_sample = [&]() -> jni::Wrap {
      if (sample.byte_range.available) {
        return jni::Wrap(env, "com/twitter/vireo/demux/jni/Movie$Sample", "(Lcom/twitter/vireo/demux/jni/Movie;JJZBIII)V",
                         movie_obj, sample.pts, sample.dts, sample.keyframe, sample.type, sample.byte_range.pos, sample.byte_range.size, index);
      } else {
        // jni has no way of creating Option[ByteRange], we use size < 0 to signal None
        return jni::Wrap(env, "com/twitter/vireo/demux/jni/Movie$Sample", "(Lcom/twitter/vireo/demux/jni/Movie;JJZBIII)V",
                         movie_obj, sample.pts, sample.dts, sample.keyframe, sample.type, 0, -1, index);
      }
    }();
    return *jni_sample;
  }, NULL);
}

jlong JNICALL Java_com_twitter_vireo_demux_jni_Movie_duration(JNIEnv* env, jobject movie_obj, jlong _jni, jbyte sample_type) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jlong>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio && type != SampleType::Caption, InvalidArguments);
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie.get());
    auto& decoder = *jni->movie;
    if (type == SampleType::Video) {
      return (jlong)decoder.video_track.duration();
    } else if (type == SampleType::Audio) {
      return (jlong)decoder.audio_track.duration();
    } else {
      CHECK(type == SampleType::Caption);
      return (jlong)decoder.caption_track.duration();
    }
  }, 0);
}

jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_editBoxes(JNIEnv* env, jobject movie_obj, jlong _jni, jbyte sample_type) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio && type != SampleType::Caption, InvalidArguments);
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie);
    auto& decoder = *jni->movie;
    vector<common::EditBox> edit_boxes;
    if (type == SampleType::Video) {
      edit_boxes = decoder.video_track.edit_boxes();
    } else if (type == SampleType::Audio) {
      edit_boxes = decoder.audio_track.edit_boxes();
    } else {
      CHECK(type == SampleType::Caption)
      edit_boxes = decoder.caption_track.edit_boxes();
    }
    auto jni_edit_boxes = jni::Wrap(env, "scala/collection/mutable/ArrayBuffer", "(I)V", edit_boxes.size());
    for (const auto& edit_box: edit_boxes) {
      auto jni_edit_box = jni::Wrap(env, "com/twitter/vireo/common/EditBox", "(JJB)V", edit_box.start_pts, edit_box.duration_pts, edit_box.type);
      jni_edit_boxes.call<jobject>("$plus$eq", "(Ljava/lang/Object;)Lscala/collection/mutable/ArrayBuffer;", *jni_edit_box);
    }
    return jni_edit_boxes.call<jobject>("toSeq", "()Lscala/collection/GenSeq;");
  }, NULL);
}

jobject JNICALL Java_com_twitter_vireo_demux_jni_Movie_nal(JNIEnv* env, jobject movie_obj, jlong _jni, jbyte sample_type, jint index, jboolean copy) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio && type != SampleType::Data && type != SampleType::Caption, InvalidArguments);
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie);
    lock_guard<mutex>(jni->lock);
    if (type == SampleType::Video) {
      THROW_IF(index >= jni->movie->video_track.count(), OutOfRange);
    } else if (type == SampleType::Audio) {
      THROW_IF(index >= jni->movie->audio_track.count(), OutOfRange);
    } else if (type == SampleType::Data) {
      THROW_IF(index >= jni->movie->data_track.count(), OutOfRange);
    } else {
      CHECK(type == SampleType::Caption)
      THROW_IF(index >= jni->movie->caption_track.count(), OutOfRange);
    }
    CHECK(jni->nal_funcs.find(make_tuple(type, index)) != jni->nal_funcs.end());
    function<common::Data32(void)>& nal_func = jni->nal_funcs[make_tuple(type, index)];
    auto nal = nal_func();

    return *jni::create(env, jni, move(nal), copy);
  }, NULL);
}

void JNICALL Java_com_twitter_vireo_demux_jni_Movie_freeNal(JNIEnv* env, jobject movie_obj, jlong _jni, jobject byte_data_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = (_JNIMovieStruct*)_jni;
    CHECK(jni && jni->movie);

    auto jni_byte_data = jni::Wrap(env, byte_data_obj);
    jobject byte_buffer_obj = jni_byte_data.call<jobject>("byteBuffer", "()Ljava/nio/ByteBuffer;");
    const void* ptr = env->GetDirectBufferAddress(byte_buffer_obj);
    CHECK(ptr);

    jni->remove_buffer_ref(ptr);
  });
}
