package com.twitter.vireo.transform.jni

import com.twitter.vireo._
import com.twitter.vireo.common.EditBox
import com.twitter.vireo.decode.Sample
import com.twitter.vireo.functional.types._
import com.twitter.vireo.SampleType._

class Stitch {
  protected[this] val jni: Long = 0

  protected[this] def decode(sampleType: SampleType, index: Int): com.twitter.vireo.decode.Sample = decode(jni, sampleType, index)
  protected[this] def duration(sampleType: SampleType): Long = duration(jni, sampleType)
  protected[this] def editBoxes(sampleType: SampleType): Seq[EditBox] = editBoxes(jni, sampleType)

  @native protected[this] def jniInit(audioTracks: Seq[Audio[Sample]], videoTracks: Seq[Video[Sample]], editBoxesPerTrack: Seq[Seq[EditBox]])
  @native protected[this] def jniClose(): Unit
  @native protected[this] def decode(jni: Long, sampleType: SampleType, index: Int): com.twitter.vireo.decode.Sample
  @native protected[this] def duration(jni: Long, sampleType: SampleType): Long
  @native protected[this] def editBoxes(jni: Long, sampleType: SampleType): Seq[EditBox]

  common.Load()
}

class Trim[S] {
  protected[this] val jni: Long = 0

  protected[this] def decode(index: Int): Sample = decode(jni, index)
  protected[this] def duration(): Long = duration(jni)
  protected[this] def editBoxes(): Seq[EditBox] = editBoxes(jni)

  @native protected[this] def jniInit(track: Media[Sample, S], editBoxes: Seq[EditBox], startMs: Long, durationMs: Long)
  @native protected[this] def jniClose(): Unit
  @native protected[this] def decode(jni: Long, index: Int): Sample
  @native protected[this] def duration(jni: Long): Long
  @native protected[this] def editBoxes(jni: Long): Seq[EditBox]

  common.Load()
}
