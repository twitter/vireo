package com.twitter.vireo.decode

import com.twitter.vireo.common._
import com.twitter.vireo.SampleType._

case class ByteRange(pos: Int, size: Int)

class Sample(
  pts: Long,
  dts: Long,
  keyframe: Boolean,
  sampleType: SampleType,
  val nal: () => Data[Byte],
  val byteRange: Option[ByteRange])
  extends com.twitter.vireo.Sample(pts, dts, keyframe, sampleType) {
  override def info: String = {
    if (byteRange.isDefined) {
      super.info + ", pos = %d, size = %d".format(byteRange.get.pos, byteRange.get.size)
    } else {
      super.info
    }
  }
}

object Sample {
  def apply(pts: Long, dts: Long, keyframe: Boolean, sampleType: SampleType, nal: () => Data[Byte]) = {
    new Sample(pts, dts, keyframe, sampleType, nal, None)
  }
  def apply(pts: Long, dts: Long, keyframe: Boolean, sampleType: SampleType, nal: () => Data[Byte], pos: Int, size: Int) = {
    new Sample(pts, dts, keyframe, sampleType, nal, Some(ByteRange(pos, size)))
  }
}
