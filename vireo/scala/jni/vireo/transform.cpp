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

#include "vireo/error/error.h"
#include "vireo/scala/jni/common/jni.h"
#include "vireo/scala/jni/vireo/transform.h"
#include "vireo/scala/jni/vireo/util.h"
#include "vireo/transform/stitch.h"
#include "vireo/transform/trim.h"

using namespace vireo;

// Stitch Implementation

struct _JNIStitchStruct {
  vector<jni::Wrap> jni_audio_samples;
  vector<common::EditBox> audio_edit_boxes;
  uint64_t audio_duration;
  vector<jni::Wrap> jni_video_samples;
  vector<common::EditBox> video_edit_boxes;
  uint64_t video_duration;
};

void JNICALL Java_com_twitter_vireo_transform_jni_Stitch_jniInit(JNIEnv* env, jobject stitch_obj, jobject audio_tracks_obj, jobject video_tracks_obj, jobject edit_boxes_per_track_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&]{
    auto jni = new _JNIStitchStruct();
    auto jni_stitch = jni::Wrap(env, stitch_obj);
    jni_stitch.set<jlong>("jni", (jlong)jni);

    // store a local list of sample jobjects as well
    uint64_t index = 0;
    vector<jobject> sample_objs;

    // collect input audio tracks
    auto audio_tracks = jni::createVectorFromSeq<functional::Audio<decode::Sample>>(env, audio_tracks_obj, function<functional::Audio<decode::Sample>(jobject)>([env, &index, &sample_objs](jobject track_obj) -> functional::Audio<decode::Sample> {
      vector<decode::Sample> samples = jni::createVectorFromMedia(env, track_obj, function<decode::Sample(jobject)>([env, &index, &sample_objs](jobject sample_obj) -> decode::Sample {
        auto jni_sample = jni::Wrap(env, sample_obj);

        int64_t pts = (int64_t)jni_sample.get<jlong>("pts");
        int64_t dts = (int64_t)jni_sample.get<jlong>("dts");
        bool keyframe = (bool)jni_sample.get<jboolean>("keyframe");
        SampleType type = (SampleType)jni_sample.get<jbyte>("sampleType");

        sample_objs.push_back(sample_obj);

        const uint8_t* _index = (const uint8_t*)index++;  // not pointing to a valid memory address, used for storing original index in a dummy common::Data32 class
        return (decode::Sample){ pts, dts, keyframe, type, [_index](){ return common::Data32(_index, 0, NULL); } };
      }));
      auto settings_obj = jni::Wrap(env, track_obj).get("settings", "Ljava/lang/Object;");
      auto settings = jni::createAudioSettings(env, settings_obj);
      return functional::Audio<decode::Sample>(samples, settings);
    }));

    // collect input video tracks
    auto video_tracks = jni::createVectorFromSeq<functional::Video<decode::Sample>>(env, video_tracks_obj, function<functional::Video<decode::Sample>(jobject)>([env, &index, &sample_objs](jobject track_obj) -> functional::Video<decode::Sample> {
      vector<decode::Sample> samples = jni::createVectorFromMedia(env, track_obj, function<decode::Sample(jobject)>([env, &index, &sample_objs](jobject sample_obj) -> decode::Sample {
        auto jni_sample = jni::Wrap(env, sample_obj);

        int64_t pts = (int64_t)jni_sample.get<jlong>("pts");
        int64_t dts = (int64_t)jni_sample.get<jlong>("dts");
        bool keyframe = (bool)jni_sample.get<jboolean>("keyframe");
        SampleType type = (SampleType)jni_sample.get<jbyte>("sampleType");

        sample_objs.push_back(sample_obj);

        const uint8_t* _index = (const uint8_t*)index++;  // not pointing to a valid memory address, used for storing original index in a dummy common::Data32 class
        return (decode::Sample){ pts, dts, keyframe, type, [_index](){ return common::Data32(_index, 0, NULL); } };
      }));
      auto settings_obj = jni::Wrap(env, track_obj).get("settings", "Ljava/lang/Object;");
      auto settings = jni::createVideoSettings(env, settings_obj);
      return functional::Video<decode::Sample>(samples, settings);
    }));

    // collect the input edit boxes
    auto edit_boxes_per_track = jni::createVectorFromSeq<vector<common::EditBox>>(env, edit_boxes_per_track_obj, function<vector<common::EditBox>(jobject)>([env](jobject edit_boxes_obj) -> vector<common::EditBox> {
      return jni::createVectorFromSeq<common::EditBox>(env, edit_boxes_obj, function<common::EditBox(jobject)>([env](jobject edit_box_obj) -> common::EditBox {
        auto jni_edit_box = jni::Wrap(env, edit_box_obj);
        return common::EditBox((int64_t)jni_edit_box.get<jlong>("startPts"),
                               (uint64_t)jni_edit_box.get<jlong>("durationPts"),
                               1.0f,
                               (SampleType)jni_edit_box.get<jbyte>("sampleType"));

      }));
    }));

    // stitch
    auto stitched = transform::Stitch(audio_tracks, video_tracks, edit_boxes_per_track);

    // setup output audio track
    for (auto sample: stitched.audio_track) {
      uint64_t index = (const uint64_t)sample.nal().data();
      jobject sample_obj = sample_objs[index];
      jni::Wrap jni_sample = jni::Wrap(env, sample_obj);
      jni_sample.set<jlong>("pts", (int64_t)sample.pts);
      jni_sample.set<jlong>("dts", (int64_t)sample.dts);
      jni->jni_audio_samples.push_back(move(jni_sample));
    }
    jni->audio_edit_boxes.insert(jni->audio_edit_boxes.end(), stitched.audio_track.edit_boxes().begin(), stitched.audio_track.edit_boxes().end());
    jni->audio_duration = stitched.audio_track.duration();
    jni::Wrap jni_audio_track = jni::Wrap(env, jni_stitch.get("audioTrack", "Lcom/twitter/vireo/transform/Stitch$AudioTrack;"));
    setAudioSettings(env, jni_audio_track, stitched.audio_track.settings());
    jni_audio_track.set<jint>("b", (uint32_t)jni->jni_audio_samples.size());

    // setup output video track
    for (auto sample: stitched.video_track) {
      uint64_t index = (const uint64_t)sample.nal().data();
      jobject sample_obj = sample_objs[index];
      jni::Wrap jni_sample = jni::Wrap(env, sample_obj);
      jni_sample.set<jlong>("pts", (int64_t)sample.pts);
      jni_sample.set<jlong>("dts", (int64_t)sample.dts);
      jni->jni_video_samples.push_back(move(jni_sample));
    }
    jni->video_edit_boxes.insert(jni->video_edit_boxes.end(), stitched.video_track.edit_boxes().begin(), stitched.video_track.edit_boxes().end());
    jni->video_duration = stitched.video_track.duration();
    jni::Wrap jni_video_track = jni::Wrap(env, jni_stitch.get("videoTrack", "Lcom/twitter/vireo/transform/Stitch$VideoTrack;"));
    setVideoSettings(env, jni_video_track, stitched.video_track.settings());
    jni_video_track.set<jint>("b", (uint32_t)jni->jni_video_samples.size());
  }, [env, stitch_obj] {
    Java_com_twitter_vireo_transform_jni_Stitch_jniClose(env, stitch_obj);
  });
}

void JNICALL Java_com_twitter_vireo_transform_jni_Stitch_jniClose(JNIEnv* env, jobject stitch_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_stitch = jni::Wrap(env, stitch_obj);
    auto jni = (_JNIStitchStruct*)jni_stitch.get<jlong>("jni");
    delete jni;
    jni_stitch.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_transform_jni_Stitch_decode(JNIEnv* env, jobject stitch_obj, jlong _jni, jbyte sample_type, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio, InvalidArguments);
    auto jni = (_JNIStitchStruct*)_jni;
    CHECK(jni);
    if (type == SampleType::Audio) {
      THROW_IF(index >= jni->jni_audio_samples.size(), OutOfRange);
      return *jni->jni_audio_samples[index];
    } else {
      THROW_IF(index >= jni->jni_video_samples.size(), OutOfRange);
      return *jni->jni_video_samples[index];
    }
  }, NULL);
}

jlong JNICALL Java_com_twitter_vireo_transform_jni_Stitch_duration(JNIEnv* env, jobject stitch_obj, jlong _jni, jbyte sample_type) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jlong>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio, InvalidArguments);
    auto jni = (_JNIStitchStruct*)_jni;
    CHECK(jni);
    return (jlong)((type == SampleType::Video) ? jni->video_duration : jni->audio_duration);
  }, 0);
}

jobject JNICALL Java_com_twitter_vireo_transform_jni_Stitch_editBoxes(JNIEnv* env, jobject stitch_obj, jlong _jni, jbyte sample_type) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    const SampleType type = (SampleType)sample_type;
    THROW_IF(type != SampleType::Video && type != SampleType::Audio, InvalidArguments);
    auto jni = (_JNIStitchStruct*)_jni;
    CHECK(jni);
    auto edit_boxes = (type == SampleType::Video) ? jni->video_edit_boxes : jni->audio_edit_boxes;
    auto jni_edit_boxes = jni::Wrap(env, "scala/collection/mutable/ArrayBuffer", "(I)V", edit_boxes.size());
    for (const auto& edit_box: edit_boxes) {
      auto jni_edit_box = jni::Wrap(env, "com/twitter/vireo/common/EditBox", "(JJB)V", edit_box.start_pts, edit_box.duration_pts, edit_box.type);
      jni_edit_boxes.call<jobject>("$plus$eq", "(Ljava/lang/Object;)Lscala/collection/mutable/ArrayBuffer;", *jni_edit_box);
    }
    return jni_edit_boxes.call<jobject>("toSeq", "()Lscala/collection/GenSeq;");
  }, NULL);
}

// Trim Implementation

struct _JNITrimStruct {
  vector<jni::Wrap> jni_samples;
  vector<common::EditBox> edit_boxes;
  uint64_t duration;
};

void JNICALL Java_com_twitter_vireo_transform_jni_Trim_jniInit(JNIEnv* env, jobject trim_obj, jobject samples_obj, jobject edit_boxes_obj, jlong start_ms, jlong duration_ms) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni = new _JNITrimStruct();
    auto jni_trim = jni::Wrap(env, trim_obj);
    jni_trim.set<jlong>("jni", (jlong)jni);

    // collect the input samples
    uint64_t index = 0;
    vector<jobject> sample_objs;
    vector<decode::Sample> samples = jni::createVectorFromMedia(env, samples_obj, function<decode::Sample(jobject)>([env, &index, &sample_objs](jobject sample_obj) -> decode::Sample {
      auto jni_sample = jni::Wrap(env, sample_obj);

      int64_t pts = (int64_t)jni_sample.get<jlong>("pts");
      int64_t dts = (int64_t)jni_sample.get<jlong>("dts");
      bool keyframe = (bool)jni_sample.get<jboolean>("keyframe");
      SampleType type = (SampleType)jni_sample.get<jbyte>("sampleType");

      sample_objs.push_back(sample_obj);  // store a local list of sample jobjects as well

      const uint8_t* _index = (const uint8_t*)index++;  // not pointing to a valid memory address, used for storing original index in a dummy common::Data32 class
      return (decode::Sample){ pts, dts, keyframe, type, [_index](){ return common::Data32(_index, 0, NULL); } };
    }));

    // collect the input edit boxes
    vector<common::EditBox> edit_boxes = jni::createVectorFromSeq<common::EditBox>(env, edit_boxes_obj, function<common::EditBox(jobject)>([env](jobject edit_box_obj) -> common::EditBox {
      auto jni_edit_box = jni::Wrap(env, edit_box_obj);
      return common::EditBox((int64_t)jni_edit_box.get<jlong>("startPts"),
                             (uint64_t)jni_edit_box.get<jlong>("durationPts"),
                             1.0f,
                             (SampleType)jni_edit_box.get<jbyte>("sampleType"));

    }));

    // get settings object, classify type of track
    auto settings_obj = jni::Wrap(env, samples_obj).get("settings", "Ljava/lang/Object;");
    auto jni_settings = jni::Wrap(env, settings_obj);
    string settings_type = jni_settings.class_name();

    // trim the track
    if (settings_type.find("com/twitter/vireo/settings/Video") == 0) {
      auto settings = jni::createVideoSettings(env, settings_obj);
      auto track = functional::Video<decode::Sample>(samples, settings);
      auto trimmed = transform::Trim<SampleType::Video>(track, edit_boxes, (uint64_t)start_ms, (uint64_t)duration_ms);
      for (auto sample: trimmed.track) {
        uint64_t index = (const uint64_t)sample.nal().data();
        jobject sample_obj = sample_objs[index];
        jni::Wrap jni_sample = jni::Wrap(env, sample_obj);
        jni_sample.set<jlong>("pts", (int64_t)sample.pts);
        jni_sample.set<jlong>("dts", (int64_t)sample.dts);
        jni->jni_samples.push_back(move(jni_sample));
      }
      jni->edit_boxes.insert(jni->edit_boxes.end(), trimmed.track.edit_boxes().begin(), trimmed.track.edit_boxes().end());
      jni->duration = trimmed.track.duration();

      // final setup for the associated Scala object
      jni::Wrap jni_trimmed_track = jni::Wrap(env, jni_trim.get("track", "Lcom/twitter/vireo/transform/Trim$Track;"));
      setVideoSettings(env, jni_trimmed_track, trimmed.track.settings());
      jni_trimmed_track.set<jint>("b", (uint32_t)jni->jni_samples.size());
    } else if (settings_type.find("com/twitter/vireo/settings/Audio") == 0) {
      auto settings = jni::createAudioSettings(env, settings_obj);
      auto track = functional::Audio<decode::Sample>(samples, settings);
      auto trimmed = transform::Trim<SampleType::Audio>(track, edit_boxes, (uint64_t)start_ms, (uint64_t)duration_ms);
      for (auto sample: trimmed.track) {
        uint64_t index = (const uint64_t)sample.nal().data();
        jobject sample_obj = sample_objs[index];
        jni::Wrap jni_sample = jni::Wrap(env, sample_obj);
        jni_sample.set<jlong>("pts", (int64_t)sample.pts);
        jni_sample.set<jlong>("dts", (int64_t)sample.dts);
        jni->jni_samples.push_back(move(jni_sample));
      }
      jni->edit_boxes.insert(jni->edit_boxes.end(), trimmed.track.edit_boxes().begin(), trimmed.track.edit_boxes().end());
      jni->duration = trimmed.track.duration();

      // final setup for the associated Scala object
      jni::Wrap jni_trimmed_track = jni::Wrap(env, jni_trim.get("track", "Lcom/twitter/vireo/transform/Trim$Track;"));
      setAudioSettings(env, jni_trimmed_track, trimmed.track.settings());
      jni_trimmed_track.set<jint>("b", (uint32_t)jni->jni_samples.size());
    } else if (settings_type.find("com/twitter/vireo/settings/Caption") == 0) {
      auto settings = jni::createCaptionSettings(env, settings_obj);
      auto track = functional::Caption<decode::Sample>(samples, settings);
      auto trimmed = transform::Trim<SampleType::Caption>(track, edit_boxes, (uint64_t)start_ms, (uint64_t)duration_ms);
      for (auto sample: trimmed.track) {
        uint64_t index = (const uint64_t)sample.nal().data();
        jobject sample_obj = sample_objs[index];
        jni::Wrap jni_sample = jni::Wrap(env, sample_obj);
        jni_sample.set<jlong>("pts", (int64_t)sample.pts);
        jni_sample.set<jlong>("dts", (int64_t)sample.dts);
        jni->jni_samples.push_back(move(jni_sample));
      }
      jni->edit_boxes.insert(jni->edit_boxes.end(), trimmed.track.edit_boxes().begin(), trimmed.track.edit_boxes().end());
      jni->duration = trimmed.track.duration();

      // final setup for the associated Scala object
      jni::Wrap jni_trimmed_track = jni::Wrap(env, jni_trim.get("track", "Lcom/twitter/vireo/transform/Trim$Track;"));
      setCaptionSettings(env, jni_trimmed_track, trimmed.track.settings());
      jni_trimmed_track.set<jint>("b", (uint32_t)jni->jni_samples.size());
    } else {
      THROW_IF(true, Invalid);
    }
  }, [env, trim_obj] {
    Java_com_twitter_vireo_transform_jni_Trim_jniClose(env, trim_obj);
  });
}

void JNICALL Java_com_twitter_vireo_transform_jni_Trim_jniClose(JNIEnv* env, jobject trim_obj) {
  jni::ExceptionHandler::SafeExecuteFunction(env, [&] {
    auto jni_trim = jni::Wrap(env, trim_obj);
    auto jni = (_JNITrimStruct*)jni_trim.get<jlong>("jni");
    delete jni;
    jni_trim.set<jlong>("jni", 0);
  });
}

jobject JNICALL Java_com_twitter_vireo_transform_jni_Trim_decode(JNIEnv* env, jobject trim_obj, jlong _jni, jint index) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNITrimStruct*)_jni;
    CHECK(jni);

    THROW_IF(index >= jni->jni_samples.size(), OutOfRange);
    return *jni->jni_samples[index];
  }, NULL);
}

jlong JNICALL Java_com_twitter_vireo_transform_jni_Trim_duration(JNIEnv* env, jobject trim_obj, jlong _jni) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jlong>(env, [&] {
    auto jni = (_JNITrimStruct*)_jni;
    CHECK(jni);

    return (jlong)jni->duration;
  }, 0);
}

jobject JNICALL Java_com_twitter_vireo_transform_jni_Trim_editBoxes(JNIEnv* env, jobject trimmed_track_obj, jlong _jni) {
  return jni::ExceptionHandler::SafeExecuteFunctionAndReturn<jobject>(env, [&] {
    auto jni = (_JNITrimStruct*)_jni;
    CHECK(jni);

    auto edit_boxes = jni->edit_boxes;
    auto jni_edit_boxes = jni::Wrap(env, "scala/collection/mutable/ArrayBuffer", "(I)V", edit_boxes.size());
    for (const auto& edit_box: edit_boxes) {
      auto jni_edit_box = jni::Wrap(env, "com/twitter/vireo/common/EditBox", "(JJB)V", edit_box.start_pts, edit_box.duration_pts, edit_box.type);
      jni_edit_boxes.call<jobject>("$plus$eq", "(Ljava/lang/Object;)Lscala/collection/mutable/ArrayBuffer;", *jni_edit_box);
    }
    return jni_edit_boxes.call<jobject>("toSeq", "()Lscala/collection/GenSeq;");
  }, NULL);
}
