package com.twitter.vireo.encode

import com.twitter.vireo.frame.Frame
import com.twitter.vireo.functional.types._

object RCMethod extends Enumeration {
  type RCMethod = Byte
  val CRF: RCMethod = 0
  val CBR: RCMethod = 1
  val ABR: RCMethod = 2
}

object PyramidMode extends Enumeration {
  type PyramidMode = Byte
  val None: PyramidMode = 0
  val Strict: PyramidMode = 1
  val Normal: PyramidMode = 2
}

object VideoProfile extends Enumeration {
  type VideoProfile = Byte
  val ConstrainedBaseline: VideoProfile = 0
  val Baseline: VideoProfile = 1
  val Main: VideoProfile = 2
  val High: VideoProfile = 3
}

object AdaptiveQuantizationMode extends Enumeration {
  type AdaptiveQuantizationMode = Byte
  val None: AdaptiveQuantizationMode = 0
  val Variance: AdaptiveQuantizationMode = 1
  val AutoVariance: AdaptiveQuantizationMode = 2
  val AutoVarianceBiased: AdaptiveQuantizationMode = 3
}

object MotionEstimationMethod extends Enumeration {
  type MotionEstimationMethod = Byte
  val Diamond: MotionEstimationMethod = 0
  val Hexagon: MotionEstimationMethod = 1
  val MultiHex: MotionEstimationMethod = 2
  val Exhaustive: MotionEstimationMethod = 3
  val TransformedExhaustive: MotionEstimationMethod = 4
}

case class ComputationalParams(
  optimization: Int = 3,
  threadCount: Int = 0
)

case class RateControlParams(
  rcMethod: RCMethod.RCMethod = RCMethod.CRF,
  crf: Float = 28.0f,
  maxBitrate: Int = 0,
  bitrate: Int = 0,
  bufferSize: Int = 0,
  bufferInit: Float = 0.0f,
  lookAhead: Int = 40,
  isSecondPass: Boolean = false,
  enableMbTree: Boolean = true,
  aqMode: AdaptiveQuantizationMode.AdaptiveQuantizationMode = AdaptiveQuantizationMode.Variance,
  qpMin: Int = 0,
  statsLogPath: String = "x264_2pass.log",
  mixedRefs: Boolean = true,
  trellis: Int = 1,
  meMethod: MotionEstimationMethod.MotionEstimationMethod = MotionEstimationMethod.Hexagon,
  subpelRefine: Int = 7
)

case class GopParams(
  numBframes: Int = 0,
  mode: PyramidMode.PyramidMode = PyramidMode.Normal ,
  keyintMax: Int = (1 << 30),
  keyintMin: Int = 0,
  frameReferences: Int = 3
)

case class H264Params(
  computation: ComputationalParams,
  rc: RateControlParams,
  gop: GopParams,
  profile: VideoProfile.VideoProfile,
  fps: Float = 30.0f,
  overridePresets: Boolean = false)

class H264(
  frames: Video[Frame],
  param: H264Params)
  extends jni.H264
  with Video[() => Sample]
  with AutoCloseable {

  def apply(index: Int) = encode(index)
  def close(): Unit = jniClose()

  jniInit(frames, param)
}

object  H264 {
  def apply(frames: Video[Frame], param: H264Params) = {
    new H264(frames, param)
  }
  def apply(frames: Video[Frame], rate: Float, optimization: Int, fps: Float, maxBitrate: Int = 0, threadCount: Int = 0) = {
    val minDimension = math.min(frames. settings.width, frames.settings.height)
    val profile = if (minDimension <= 480) {
      VideoProfile.Baseline
    } else if (minDimension <= 720) {
      VideoProfile.Main
    } else {
      VideoProfile.High
    }
    new H264(frames, H264Params(ComputationalParams(optimization, threadCount), RateControlParams(RCMethod.CRF, rate,
      maxBitrate), GopParams(0), profile, fps))
  }
}
