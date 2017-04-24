package com.twitter.vireo.frame

import com.twitter.vireo.frame.Rotation._

class RGB(val component_count: Byte, val plane: Plane) extends jni.RGB {

  val width: Short = (plane.width / component_count).toShort
  val height: Short = plane.height

  @native def rgb(componentCount: Int): RGB
  @native def yuv(uvXRatio: Int, uvYRatio: Int): YUV
  @native def crop(xOffset: Int, yOffset: Int, width: Int, height: Int): RGB
  @native def rotate(direction: Rotation): RGB
  @native def stretch(numX: Int, denomX: Int, numY: Int, denomY: Int): RGB
  def scale(num: Int, denom: Int): RGB = stretch(num, denom, num, denom)
}
