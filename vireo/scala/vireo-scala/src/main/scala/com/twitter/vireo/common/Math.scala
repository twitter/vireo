package com.twitter.vireo.common

object Math {
  def mean(numbers: Seq[Double]): Double = {
    if (numbers.size > 0) numbers.sum / numbers.length else Double.NaN
  }

  def variance(numbers: Seq[Double]): Double = {
    val avg = mean(numbers)
    val squares = numbers.map(x => x * x)
    mean(squares) - avg * avg
  }

  def stdDev(numbers: Seq[Double]): Double = {
    math.sqrt(variance(numbers))
  }

  def roundDivide(x: Int, num: Int, denom: Int): Int = {
    (x * num + (denom + 1) / 2) / denom
  }

  def roundDivide(x: Long, num: Long, denom: Long): Long = {
    (x * num + (denom + 1) / 2) / denom
  }
}