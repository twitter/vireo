package com.twitter.vireo.demux.jni

import com.twitter.vireo.common
import com.twitter.vireo.common.{Data, EditBox, Reader}
import com.twitter.vireo.decode.ByteRange
import com.twitter.vireo.FileType._
import com.twitter.vireo.SampleType._

class Movie {
  protected[this] val jni: Long = 0

  private[this] class SampleFunc(sampleType: SampleType, index: Int)
    extends Function0[Data[Byte]]
    with common.Native[Byte, Data[Byte]] {
    def apply(copy: Boolean): Data[Byte] = {
      nal(jni, sampleType, index, copy)
    }
    def cleaner(data: Data[Byte]) = {
      freeNal(jni, data)
    }
  }

  private[this] class Sample(pts: Long, dts: Long, keyframe: Boolean, sampleType: SampleType, pos: Int, size: Int, index: Int)
    extends com.twitter.vireo.decode.Sample(
      pts,
      dts,
      keyframe,
      sampleType,
      new SampleFunc(sampleType, index),
      if (size < 0) None else Some(ByteRange(pos, size))  // jni has no way of creating Option[ByteRange], we use size < 0 to signal None
    )

  protected[this] def decode(sampleType: SampleType, index: Int): com.twitter.vireo.decode.Sample = decode(jni, sampleType, index)
  protected[this] def duration(sampleType: SampleType): Long = duration(jni, sampleType)
  protected[this] def editBoxes(sampleType: SampleType): Seq[EditBox] = editBoxes(jni, sampleType)
  protected[this] def jniFileType(): FileType = jniFileType(jni)

  @native protected[this] def jniInit(data: Data[Byte]): Unit
  @native protected[this] def jniInit(reader: Reader): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def jniFileType(jni: Long): Byte
  @native protected[this] def decode(jni: Long, sampleType: SampleType, index: Int): com.twitter.vireo.decode.Sample
  @native protected[this] def duration(jni: Long, sampleType: SampleType): Long
  @native protected[this] def editBoxes(jni: Long, sampleType: SampleType): Seq[EditBox]
  @native protected[this] def nal(jni: Long, sampleType: SampleType, index: Int, copy: Boolean): Data[Byte]
  @native protected[this] def freeNal(jni: Long, data: Data[Byte]): Unit

  common.Load()
}