package com.twitter.vireo.encode

import com.twitter.vireo.common
import com.twitter.vireo.decode
import com.twitter.vireo.SampleType._

class Sample(
  pts: Long,
  dts: Long,
  keyframe: Boolean,
  sampleType: SampleType,
  val nal: common.Data[Byte])
  extends com.twitter.vireo.Sample(pts, dts, keyframe, sampleType) {
  override def info: String = super.info + ", nal = %d bytes".format(nal.size)
}

object Sample {
  def apply(pts: Long, dts: Long, keyframe: Boolean, sampleType: SampleType, nal: common.Data[Byte]) = {
    new Sample(pts, dts, keyframe, sampleType, nal)
  }

  def apply(sample: decode.Sample): () => Sample = {
    trait NativeByteDataFunc extends common.Native[Byte, common.Data[Byte]]

    class SampleFunc(pts: Long, dts: Long, keyframe: Boolean, sampleType: SampleType, nal: NativeByteDataFunc)
      extends Function0[Sample]
      with common.Native[Byte, Sample] {
      def apply(copy: Boolean): Sample = {
        Sample(pts, dts, keyframe, sampleType, nal(copy))
      }
      def cleaner(data: common.Data[Byte]) = {
        nal.cleaner(data)
      }
    }

    sample.nal match {
      case nal: NativeByteDataFunc =>
        new SampleFunc(sample.pts, sample.dts, sample.keyframe, sample.sampleType, nal)
      case _ =>
        () => Sample(sample.pts, sample.dts, sample.keyframe, sample.sampleType, sample.nal())
    }
  }
}
