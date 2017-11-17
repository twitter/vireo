package com.twitter.vireo.frame

case class Frame(pts: Long, yuv: () => YUV, rgb: () => RGB = null) {
  override def toString: String = {
    "(pts = " + pts + ")"
  }
}
