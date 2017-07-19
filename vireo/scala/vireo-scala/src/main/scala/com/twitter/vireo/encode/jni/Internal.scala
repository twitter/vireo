package com.twitter.vireo.encode.jni

import com.twitter.vireo.common
import com.twitter.vireo.common.Data
import com.twitter.vireo.encode._
import com.twitter.vireo.frame._
import com.twitter.vireo.functional.types._

class AAC {
  protected[this] val jni: Long = 0

  protected class SampleFunc(index: Int)
    extends Function0[com.twitter.vireo.encode.Sample]
    with common.Native[Byte, com.twitter.vireo.encode.Sample] {
    def apply(copy: Boolean): com.twitter.vireo.encode.Sample = {
      sample(jni, index, copy)
    }
    def cleaner(data: Data[Byte]) = {
      freeSample(jni, data)
    }
  }

  protected[this] def encode(index: Int) = new SampleFunc(index)

  @native protected[this] def jniInit(sounds: Audio[com.twitter.vireo.sound.Sound], channels: Byte, bitrate: Int): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def sample(jni: Long, index: Int, copy: Boolean): com.twitter.vireo.encode.Sample
  @native protected[this] def freeSample(jni: Long, data: Data[Byte]): Unit

  common.Load()
}

class Vorbis {
  protected[this] val jni: Long = 0

  protected class SampleFunc(index: Int)
    extends Function0[com.twitter.vireo.encode.Sample]
    with common.Native[Byte, com.twitter.vireo.encode.Sample] {
    def apply(copy: Boolean): com.twitter.vireo.encode.Sample = {
      sample(jni, index, copy)
    }
    def cleaner(data: Data[Byte]) = {
      freeSample(jni, data)
    }
  }

  protected[this] def encode(index: Int) = new SampleFunc(index)

  @native protected[this] def jniInit(sounds: Audio[com.twitter.vireo.sound.Sound], channels: Byte, bitrate: Int): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def sample(jni: Long, index: Int, copy: Boolean): com.twitter.vireo.encode.Sample
  @native protected[this] def freeSample(jni: Long, data: Data[Byte]): Unit

  common.Load()
}

class H264 {
  protected[this] val jni: Long = 0

  protected class SampleFunc(index: Int)
    extends Function0[com.twitter.vireo.encode.Sample]
    with common.Native[Byte, com.twitter.vireo.encode.Sample] {
    def apply(copy: Boolean): com.twitter.vireo.encode.Sample = {
      sample(jni, index, copy)
    }
    def cleaner(data: Data[Byte]) = {
      freeSample(jni, data)
    }
  }

  protected[this] def encode(index: Int) = new SampleFunc(index)

  def jniInit(frames: Video[com.twitter.vireo.frame.Frame], params: H264Params): Unit = {
    jniInit(
      frames,
      params.computation.optimization,
      params.computation.threadCount,
      params.rc.rcMethod,
      params.rc.crf,
      params.rc.maxBitrate,
      params.rc.bitrate,
      params.rc.bufferSize,
      params.rc.bufferInit,
      params.rc.lookAhead,
      params.rc.isSecondPass,
      params.rc.enableMbTree,
      params.rc.aqMode,
      params.rc.qpMin,
      params.rc.statsLogPath,
      params.rc.mixedRefs,
      params.rc.trellis,
      params.rc.meMethod,
      params.rc.subpelRefine,
      params.gop.numBframes,
      params.gop.mode,
      params.gop.keyintMax,
      params.gop.keyintMin,
      params.gop.frameReferences,
      params.profile,
      params.fps)
  }

  @native protected[this] def jniInit(
    frames: Video[com.twitter.vireo.frame.Frame],
    optimization: Int,
    threadCount: Int,
    rcMethod: RCMethod.RCMethod,
    crf: Float,
    maxBitrate: Int,
    bitrate: Int,
    bufferSize: Int,
    bufferInit: Float,
    lookAhead: Int,
    isSecondPass: Boolean,
    mbTree: Boolean,
    aqMode: AdaptiveQuantizationMode.AdaptiveQuantizationMode,
    qpMin: Int,
    statsLogPath: String,
    mixedRefs: Boolean,
    trellis: Int,
    meMethod: MotionEstimationMethod.MotionEstimationMethod,
    subpelRefine: Int,
    numBframes: Int,
    mode: PyramidMode.PyramidMode,
    keyintMax: Int,
    keyintMin: Int,
    frameReferences: Int,
    profile: VideoProfile.VideoProfile,
    fps: Float
  ): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def sample(jni: Long, index: Int, copy: Boolean): com.twitter.vireo.encode.Sample
  @native protected[this] def freeSample(jni: Long, data: Data[Byte]): Unit

  common.Load()
}

class VP8 {
  protected[this] val jni: Long = 0

  protected class SampleFunc(index: Int)
    extends Function0[com.twitter.vireo.encode.Sample]
    with common.Native[Byte, com.twitter.vireo.encode.Sample] {
    def apply(copy: Boolean): com.twitter.vireo.encode.Sample = {
      sample(jni, index, copy)
    }
    def cleaner(data: Data[Byte]) = {
      freeSample(jni, data)
    }
  }

  protected[this] def encode(index: Int) = new SampleFunc(index)

  @native protected[this] def jniInit(frames: Video[com.twitter.vireo.frame.Frame], quantizer: Int, optimization: Int, fps: Float, bitrate: Int): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def sample(jni: Long, index: Int, copy: Boolean): com.twitter.vireo.encode.Sample
  @native protected[this] def freeSample(jni: Long, data: Data[Byte]): Unit

  common.Load()
}

class JPG {
  protected[this] val jni: Long = 0

  protected[this] def encode(index: Int): Data[Byte] = encode(jni, index)

  @native protected[this] def jniInit(yuvs: Video[() => YUV], quality: Int, optimization: Int): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def encode(jni: Long, index: Int): Data[Byte]

  common.Load()
}

class PNG {
  protected[this] val jni: Long = 0

  protected[this] def encode(index: Int): Data[Byte] = encode(jni, index)

  @native protected[this] def jniInit(rgbs: Video[() => RGB]): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def encode(jni: Long, index: Int): Data[Byte]

  common.Load()
}
