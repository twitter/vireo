package com.twitter.vireo.domain

trait Interval[+Y]
  extends Function1[Int, Y]
  with IndexedSeq[Y] {
  protected[this] var a: Int = 0
  protected[this] var b: Int = 0
  def count: Int = b - a
  override def length: Int = count
}
