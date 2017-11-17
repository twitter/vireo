package com.twitter.vireo.encode

import com.twitter.vireo.functional.types._
import com.twitter.vireo.sound.Sound

class AAC(
  sounds: Audio[Sound],
  channels: Byte,
  bitrate: Int)
  extends jni.AAC
  with Audio[() => Sample]
  with AutoCloseable {

  def apply(index: Int) = encode(index)
  def close(): Unit = jniClose()

  jniInit(sounds, channels, bitrate)
}

object AAC {
  def apply(sounds: Audio[Sound], channels: Byte, bitrate: Int) = {
    new AAC(sounds, channels, bitrate)
  }
}