package com.twitter.vireo_tools.chunk

import com.twitter.vireo._
import com.twitter.vireo.common.Math
import java.io.File
import resource._
import scala.collection.mutable.ArrayBuffer

object Main {
  def main(args: Array[String]): Unit = {
    case class Config(
      iterations: Int = 1,
      infile: File = new File("."),
      outdir: File = new File(".")
    )
    var timeMeasurements = ArrayBuffer[Double]()
    val parser = new scopt.OptionParser[Config]("chunk") {
      head("chunk", "0.3")
      opt[Int]('i', "iterations") optional() action { (x, c) =>
        c.copy(iterations = x)
      } text("iteration count of transcoding (for profiling, default 1)")
      arg[File]("infile") action { (x, c) =>
        c.copy(infile = x)
      } text("input file")
      arg[File]("outdir") action { (x, c) =>
        c.copy(outdir = x)
      } text("output dir")
    }
    parser.parse(args, Config()) map { config =>
      try {
        if (!config.outdir.exists()) {
          config.outdir.mkdir()
        }
        val data = common.Data(config.infile.getAbsolutePath)
        // First iteration takes longer due to dynamic loading. Do an extra iteration and don't count first iteration towards profiling
        val iterations = if (config.iterations == 1) 1 else config.iterations + 1
        for (i <- 0 until iterations) {
          val iterationStartTime = System.currentTimeMillis()
          for (movie <- managed(demux.Movie(data))) {

            // Video
            def writeChunk(mp4Encoder: mux.MP4, chunkIndex: Int, startPts: Long, endPts: Long) {
              if (i == config.iterations) {
                println("%s.mp4 (start time: %.3f s, duration %.3f s)".format(
                  chunkIndex,
                  startPts.toDouble / movie.videoTrack.settings.timescale,
                  (endPts - startPts).toDouble / movie.videoTrack.settings.timescale))
              }
              val outChunkFile = "%s/%d.mp4".format(config.outdir.getAbsolutePath, chunkIndex)
              for (outFile <- managed(new java.io.FileOutputStream(outChunkFile))) {
                outFile.write(mp4Encoder().array())
              }
            }

            val keyFrameIndices = (
              movie.videoTrack.zipWithIndex filter {
                (x: (decode.Sample, Int)) => x._1.keyframe
              } map {
                (x: (decode.Sample, Int)) => x._2
              }
            ).toSeq :+ movie.videoTrack.count

            keyFrameIndices.sliding(2).toSeq.zipWithIndex foreach { case (Seq(from, until), index) =>
              managed(mux.MP4(
                movie.videoTrack.slice(from, until) map(encode.Sample(_: decode.Sample)))
              ) acquireAndGet { mp4Encoder =>
                writeChunk(mp4Encoder, index, movie.videoTrack(from).pts, if (until == movie.videoTrack.count) {
                  movie.videoTrack.duration()
                } else {
                  movie.videoTrack(until).pts
                })
              }
            }

            // Audio
            for (mp4Encoder <- managed(mux.MP4(
              movie.audioTrack.map(encode.Sample(_: decode.Sample)),
              functional.Video[() => encode.Sample]()))
            ) {
              if (i == config.iterations) {
                val duration = movie.audioTrack.duration().toDouble / movie.audioTrack.settings.sampleRate
                println("audio.m4a (duration: %.3f s)".format(duration))
              }
              val outAudioFile = "%s/audio.m4a".format(config.outdir.getAbsolutePath)
              for (outFile <- managed(new java.io.FileOutputStream(outAudioFile))) {
                outFile.write(mp4Encoder().array())
              }
            }
          }
          val iterationEndTime = System.currentTimeMillis()
          if (iterations == 1 || i > 0) {
            timeMeasurements += (iterationEndTime - iterationStartTime)
          }
        }
        println("Chunking time stats over %d iterations:".format(timeMeasurements.size))
        if (iterations > 1) {
          println("[Mean]      %.3f ms".format(Math.mean(timeMeasurements)))
          println("[Variance]  %.3f ms".format(Math.variance(timeMeasurements)))
          println("[Std. Dev.] %.3f ms".format(Math.stdDev(timeMeasurements)))
        } else {
          println("[Total] %.3f ms".format(Math.mean(timeMeasurements)))
        }
      } catch {
        case e: Throwable => {
          println("Error chunking movie: %s ".format(e.getMessage()))
        }
      }
    } getOrElse {
    }
  }
}
