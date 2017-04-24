package com.twitter.vireo.transform

import com.twitter.vireo.SampleType
import com.twitter.vireo.common.EditBox
import com.twitter.vireo.decode.Sample
import com.twitter.vireo.functional.types._

class Stitch (
  audioTracks: Seq[Audio[Sample]],
  videoTracks: Seq[Video[Sample]],
  editBoxesPerTrack: Seq[Seq[EditBox]]
) extends jni.Stitch with AutoCloseable {

  final class VideoTrack extends Video[Sample] {
    def apply(index: Int): Sample = Stitch.this.decode(SampleType.Video, index)
    def duration(): Long = Stitch.this.duration(SampleType.Video)
    def editBoxes(): Seq[EditBox] = Stitch.this.editBoxes(SampleType.Video)
    def fps(): Float = count.toFloat / duration * settings.timescale
  }
  val videoTrack = new VideoTrack()

  final class AudioTrack extends Audio[Sample] {
    def apply(index: Int): Sample = Stitch.this.decode(SampleType.Audio, index)
    def duration(): Long = Stitch.this.duration(SampleType.Audio)
    def editBoxes(): Seq[EditBox] = Stitch.this.editBoxes(SampleType.Audio)
  }
  val audioTrack = new AudioTrack()

  def close(): Unit = jniClose()

  jniInit(audioTracks, videoTracks, editBoxesPerTrack)
}

object Stitch {
  def apply(
    audioTracks: Seq[Audio[Sample]],
    videoTracks: Seq[Video[Sample]],
    editBoxesPerTrack: Seq[Seq[EditBox]] = Seq[Seq[EditBox]]()
  ): Stitch = {
    new Stitch(audioTracks, videoTracks, editBoxesPerTrack)
  }
  def apply(videoTracks: Seq[Video[Sample]], editBoxesPerTrack: Seq[Seq[EditBox]]): Stitch = {
    Stitch(Seq[Audio[Sample]](), videoTracks, editBoxesPerTrack)
  }
  def apply(videoTracks: Seq[Video[Sample]]): Stitch = {
    Stitch(Seq[Audio[Sample]](), videoTracks, Seq[Seq[EditBox]]())
  }
}
