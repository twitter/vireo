package com.twitter.vireo.demux

import com.twitter.vireo.decode.Sample
import com.twitter.vireo.SampleType
import com.twitter.vireo.FileType._
import com.twitter.vireo.common.{Data, EditBox, Reader}
import com.twitter.vireo.functional.types._
import com.twitter.vireo.functional.types.{Data => DataMedia}
import java.nio.ByteBuffer

final class Movie(input: Either[Data[Byte], Reader])
  extends jni.Movie
  with AutoCloseable {

  def this(data: Data[Byte]) = this(Left(data))
  def this(reader: Reader) = this(Right(reader))

  final class VideoTrack extends Video[Sample] {
    def apply(index: Int): Sample = Movie.this.decode(SampleType.Video, index)
    def duration(): Long = Movie.this.duration(SampleType.Video)
    def editBoxes(): Seq[EditBox] = Movie.this.editBoxes(SampleType.Video)
    def fps(): Float = count.toFloat / duration * settings.timescale
  }
  val videoTrack = new VideoTrack()

  final class AudioTrack extends Audio[Sample] {
    def apply(index: Int): Sample = Movie.this.decode(SampleType.Audio, index)
    def duration(): Long = Movie.this.duration(SampleType.Audio)
    def editBoxes(): Seq[EditBox] = Movie.this.editBoxes(SampleType.Audio)
  }
  val audioTrack = new AudioTrack()

  final class DataTrack extends DataMedia[Sample] {
    def apply(index: Int): Sample = Movie.this.decode(SampleType.Data, index)
  }
  val dataTrack = new DataTrack()

  final class CaptionTrack extends Caption[Sample] {
    def apply(index: Int): Sample = Movie.this.decode(SampleType.Caption, index)
    def duration(): Long = Movie.this.duration(SampleType.Caption)
    def editBoxes(): Seq[EditBox] = Movie.this.editBoxes(SampleType.Caption)
  }
  val captionTrack = new CaptionTrack()

  def close(): Unit = jniClose()
  def fileType(): FileType = jniFileType()

  input match {
    case Left(data) => jniInit(data)
    case Right(reader) => jniInit(reader)
  }
}

object Movie {
  def apply(name: String) = {
    new Movie(Data(name))
  }
  def apply(buffer: Array[Byte]) = {
    new Movie(Data(buffer))
  }
  def apply(buffer: ByteBuffer) = {
    new Movie(Data(buffer))
  }
  def apply(data: Data[Byte]) = {
    new Movie(data)
  }
  def apply(reader: Reader) = {
    new Movie(reader)
  }
}

