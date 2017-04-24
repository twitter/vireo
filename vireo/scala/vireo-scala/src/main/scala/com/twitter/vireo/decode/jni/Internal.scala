package com.twitter.vireo.decode.jni

import com.twitter.vireo.common
import com.twitter.vireo.common.Data
import com.twitter.vireo.decode.Sample
import com.twitter.vireo.frame._
import com.twitter.vireo.functional
import com.twitter.vireo.sound._

class Audio {
  protected[this] val jni: Long = 0

  private[this] class PCMFunc(index: Int)
    extends Function0[PCM]
    with common.Native[Short, PCM] {
    def apply(copy: Boolean): PCM = {
      pcm(jni, index, copy)
    }
    def cleaner(data: Data[Short]) = {
      freePcm(jni, data)
    }
  }

  private[this] class Sound(pts: Long, index: Int)
    extends com.twitter.vireo.sound.Sound(pts, new PCMFunc(index))

  protected[this] def decode(index: Int): com.twitter.vireo.sound.Sound = decode(jni, index)

  @native protected[this] def jniInit(samples: functional.types.Audio[Sample]): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def decode(jni: Long, index: Int): com.twitter.vireo.sound.Sound
  @native protected[this] def pcm(jni: Long, index: Int, copy: Boolean): PCM
  @native protected[this] def freePcm(jni: Long, data: Data[Short]): Unit

  common.Load()
}

class Video {
  protected[this] val jni: Long = 0

  private[this] class YUVFunc(index: Int) extends Function0[YUV] with common.Native[Byte, YUV] {
    def apply(copy: Boolean): YUV = yuv(jni, index, copy)
    def cleaner(data: Data[Byte]) = freeYuv(jni, data)
  }

  private[this] class RGBFunc(index: Int) extends Function0[RGB] with common.Native[Byte, RGB] {
    def apply(copy: Boolean): RGB = rgb(jni, index, copy)
    def cleaner(data: Data[Byte]) = freeRgb(jni, data)
  }

  private[this] class Frame(pts: Long, index: Int)
    extends com.twitter.vireo.frame.Frame(pts, new YUVFunc(index), new RGBFunc(index))

  protected[this] def decode(index: Int): com.twitter.vireo.frame.Frame = decode(jni, index)

  @native protected[this] def jniInit(samples: functional.types.Video[Sample], threadCount: Int): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def decode(jni: Long, index: Int): com.twitter.vireo.frame.Frame
  @native protected[this] def yuv(jni: Long, index: Int, copy: Boolean): YUV
  @native protected[this] def freeYuv(jni:Long, data: Data[Byte]): Unit
  @native protected[this] def rgb(jni: Long, index: Int, copy: Boolean): RGB
  @native protected[this] def freeRgb(jni:Long, data: Data[Byte]): Unit

  common.Load()
}
