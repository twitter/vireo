package com.twitter.vireo.transform

import com.twitter.vireo.common.EditBox
import com.twitter.vireo.decode.Sample
import com.twitter.vireo.functional.types._

class Trim[S] (
  inTrack: Media[Sample, S],
  editBoxes: Seq[EditBox],
  startMs: Long,
  durationMs: Long)
  extends jni.Trim[S]
  with AutoCloseable {

  final class Track extends Media[Sample, S] {
    def apply(index: Int) = Trim.this.decode(index)
    def duration() = Trim.this.duration()
    def editBoxes(): Seq[EditBox] = Trim.this.editBoxes()
  }
  val track = new Track()

  def close(): Unit = jniClose()

  jniInit(inTrack, editBoxes, startMs, durationMs)
}

object Trim {
  def apply[S](track: Media[Sample, S], editBoxes: Seq[EditBox], startMs: Long, durationMs: Long): Trim[S] = {
    new Trim[S](track, editBoxes, startMs, durationMs)
  }
}