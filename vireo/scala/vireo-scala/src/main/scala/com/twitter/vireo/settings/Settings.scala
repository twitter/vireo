package com.twitter.vireo.settings

import com.twitter.vireo.header._

class Video private (
  val codec: Video.Codec,
  val codedWidth: Short,
  val codedHeight: Short,
  val parWidth: Short,
  val parHeight: Short,
  val timescale: Int,
  val orientation: Video.Orientation,
  val spsPps: SPS_PPS
) {
  val (width: Short, height: Short) = if (parWidth > parHeight) {
    (codedWidth, ((codedHeight * parHeight / parWidth) & ~1).toShort)
  } else if (parWidth < parHeight) {
    (((codedWidth * parWidth / parHeight) & ~1).toShort, codedHeight)
  } else {
    (codedWidth, codedHeight)
  }
}

object Video {
  type Codec = Byte
  object Codec extends Enumeration {
    val Unknown: Codec = 0
    val H264: Codec = 1
    val VP8: Codec = 2
    val JPG: Codec = 3
    val PNG: Codec = 4
    val MPEG4: Codec = 5
    val ProRes: Codec = 6
    val GIF: Codec = 7
    val BMP: Codec = 8
    val WebP: Codec = 9
    val TIFF: Codec = 10
  }

  type Orientation = Byte
  object Orientation extends Enumeration {
    val Landscape: Orientation = 0
    val Portrait: Orientation = 1
    val LandscapeReverse: Orientation = 2
    val PortraitReverse: Orientation = 3
    val UnknownOrientation: Orientation = 4
  }

  def apply(codec: Codec, width: Short, height: Short, timescale: Int, orientation: Orientation, spsPps: SPS_PPS) = {
    new Video(codec, width, height, 1, 1, timescale, orientation, spsPps)
  }

  def apply(codec: Codec, codedWidth: Short, codedHeight: Short, parWidth: Short, parHeight: Short, timescale: Int, orientation: Orientation, spsPps: SPS_PPS) = {
    new Video(codec, codedWidth, codedHeight, parWidth, parHeight, timescale, orientation, spsPps)
  }

  val None = new Video(Codec.Unknown, 0, 0, 1, 1, 0, Orientation.UnknownOrientation, new SPS_PPS(Array[Byte](), Array[Byte](), 4))
}

final class Audio private(
  val codec: Audio.Codec,
  val timescale: Int,
  val sampleRate: Int,
  val channels: Byte,
  val bitrate: Int)

object Audio {
  type Codec = Byte
  object Codec extends Enumeration {
    val Unknown: Codec = 0
    val AAC_Main: Codec = 1
    val AAC_LC: Codec = 2
    val AAC_LC_SBR: Codec = 3
    val Vorbis: Codec = 4
    val PCM_S16LE: Codec = 5
    val PCM_S16BE: Codec = 6
    val PCM_S24LE: Codec = 7
    val PCM_S24BE: Codec = 8
  }

  def apply(codec: Codec, timescale: Int, sampleRate: Int, channels: Byte, bitrate: Int) = {
    new Audio(codec, timescale, sampleRate, channels, bitrate)
  }

  val None = new Audio(Codec.Unknown, 0, 0, 0, 0)
}

final class Data private(
  val codec: Data.Codec,
  val timescale: Int)

object Data {
  type Codec = Byte
  object Codec extends Enumeration {
    val Unknown: Codec = 0
    val TimedID3: Codec = 1
  }

  def apply(codec: Codec, timescale: Int) = {
    new Data(codec, timescale)
  }

  val None = new Data(Codec.Unknown, 0)
}

final class Caption private(
  val codec: Caption.Codec,
  val timescale: Int)

object Caption {
  type Codec = Byte
  object Codec extends Enumeration {
    val Unknown: Codec = 0
    val CEA708: Codec = 1
  }

  def apply(codec: Codec, timescale: Int) = {
    new Caption(codec, timescale)
  }

  val None = new Caption(Codec.Unknown, 0)
}
