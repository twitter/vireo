package com.twitter.vireo.encode

import com.twitter.vireo.functional.types._
import com.twitter.vireo.sound.Sound

class Vorbis(
  sounds: Audio[Sound],
  channels: Byte,
  bitrate: Int)
  extends jni.Vorbis
  with Audio[() => Sample]
  with AutoCloseable {

  def apply(index: Int) = encode(index)
  def close(): Unit = jniClose()

  jniInit(sounds, channels, bitrate)
}

object Vorbis {
  def apply(sounds: Audio[Sound], channels: Byte, bitrate: Int) = {
    new Vorbis(sounds, channels, bitrate)
  }
}