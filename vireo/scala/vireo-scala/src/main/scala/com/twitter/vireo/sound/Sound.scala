package com.twitter.vireo.sound

case class Sound(pts: Long, pcm: () => PCM) {
  override def toString: String = {
    "(pts = " + pts + ")"
  }
}
