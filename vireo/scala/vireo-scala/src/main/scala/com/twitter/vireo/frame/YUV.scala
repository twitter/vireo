package com.twitter.vireo.frame

import com.twitter.vireo.frame.Rotation._

class YUV(val y: Plane, val u: Plane, val v: Plane, val fullRange: Boolean)
  extends jni.YUV {

  val width: Short = y.width
  val height: Short = y.height

  @native def rgb(componentCount: Int): RGB
  @native def fullRange(full: Boolean): YUV
  @native def crop(xOffset: Int, yOffset: Int, width: Int, height: Int): YUV
  @native def rotate(direction: Rotation): YUV
  @native def stretch(numX: Int, denomX: Int, numY: Int, denomY: Int): YUV
  def scale(num: Int, denom: Int): YUV = stretch(num, denom, num, denom)
}
