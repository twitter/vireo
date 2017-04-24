package com.twitter.vireo

object SampleType extends Enumeration {
  type SampleType = Byte
  val Unknown: SampleType = 0
  val Video: SampleType = 1
  val Audio: SampleType = 2
  val Data: SampleType = 3
  val Caption: SampleType = 4
}

import SampleType._

abstract class Sample(
  val pts: Long,
  val dts: Long,
  val keyframe: Boolean,
  val sampleType: SampleType) {
  def info: String = {
    val sampleTypeString = sampleType match {
      case Audio => "Audio"
      case Video => "Video"
      case Caption => "Caption"
      case Data => "Data"
      case _ => "Unknown"
    }
    "pts = %d, dts = %d, keyframe = %b, type = %s".format(pts, dts, keyframe, sampleTypeString)
  }
  override def toString: String = "(" + info + ")"
}

object FileType extends Enumeration {
  type FileType = Byte
  val UnknownFileType: FileType = 0
  val MP4: FileType = 1
  val MP2TS: FileType = 2
  val WebM: FileType = 3
  val Image: FileType = 4
}

object FileFormat extends Enumeration {
  type FileFormat = Byte
  val Regular: FileFormat = 0
  val DashInitializer: FileFormat = 1
  val DashData: FileFormat = 2
  val HeaderOnly: FileFormat = 3
  val SamplesOnly: FileFormat = 4
}
