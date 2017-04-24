package com.twitter.vireo.frame

import com.twitter.vireo.common._

object Rotation extends Enumeration {
  type Rotation = Byte
  val None: Rotation = 0
  val Right: Rotation = 1
  val Down: Rotation = 2
  val Left: Rotation = 3
}

class Plane(val row: Short, val width: Short, val height: Short, val bytes: Data[Byte])
  extends Function[Int, Array[Byte]] {
  def apply(y: Int): Array[Byte] = {
    bytes.array().slice(row * y, row * y + width)
  }
}
