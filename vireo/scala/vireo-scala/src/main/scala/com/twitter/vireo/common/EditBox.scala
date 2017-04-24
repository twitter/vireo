package com.twitter.vireo.common

import com.twitter.vireo.SampleType._

class EditBox(
  val startPts: Long,
  val durationPts: Long,
  val sampleType: SampleType) {
  override def toString: String = {
    "(startPts = " + startPts + ", durationPts = " + durationPts + ")"
  }
}

object EditBox {
  def apply(startPts: Long, durationPts: Long, sampleType: SampleType) = new EditBox(startPts, durationPts, sampleType)

  def realPts(editBoxes: Seq[EditBox], pts: Long): Long = {
    var insideEditBox = true
    var newPts = pts
    if (editBoxes.nonEmpty) {
      insideEditBox = false
      newPts = 0
      var lastEndPts: Long = 0
      var continue = true
      editBoxes.toStream.takeWhile(_ => continue) foreach { editBox =>
        val startPts = editBox.startPts
        if (startPts == -1) {
          if (newPts != 0) {
            throw new UnsupportedOperationException("EditBox with startPts = -1 is not the first EditBox")
          }
          newPts = editBox.durationPts
        } else {
          val endPts = startPts + editBox.durationPts
          if (startPts < lastEndPts) {
            throw new UnsupportedOperationException("Overlapping EditBoxes exist")
          }
          lastEndPts = endPts
          if (pts >= startPts && pts < endPts) {
            newPts += (pts - editBox.startPts)
            insideEditBox = true
            continue = false
          } else if (pts > endPts) {
            newPts += editBox.durationPts
          } else {
            continue = false
          }
        }
      }
    }
    if (insideEditBox) newPts else -1
  }
}