package com.twitter.vireo_tools.transcode

import com.twitter.vireo._
import com.twitter.vireo.common.{EditBox, Math}
import com.twitter.vireo.frame.Frame
import com.twitter.vireo.sound.Sound
import java.io.File
import resource._
import scala.collection.mutable.ArrayBuffer

object Main {
  abstract class OutputFileType
  private object OutputFileType {
    case object MP4 extends OutputFileType
    case object MP2TS extends OutputFileType
    case object WebM extends OutputFileType
    case object UnknownFileType extends OutputFileType
  }
  import OutputFileType._

  // H264 related
  val H264MinCRF = 0.0f
  val H264MaxCRF = 51.0f
  val H264DefaultCRF = 28.0f
  val H264MinOptimization = 0
  val H264MaxOptimization = 9
  val H264DefaultOptimization = 3
  // VP8 related
  val VP8MinQuantizer = 5
  val VP8MaxQuantizer = 68
  val VP8DefaultQuantizer = 25
  val VP8MinOptimization = 0
  val VP8MaxOptimization = 2
  val VP8DefaultOptimization = 0
  // Audio related
  val DefaultAudioBitrateInKbps = 48
  // Security related
  val MaxIterations = 10000
  val MaxThreads = 64

  def main(args: Array[String]): Unit = {
    case class Config(
      iterations: Int = 1,
      start: Int = 0,
      duration: Int = Int.MaxValue,
      height: Int = 0,
      square: Boolean = false,
      optimization: Int = -1, // -1 indicates not set by user, default value is populated later
      crf: Double = H264DefaultCRF,
      quantizer: Int = VP8DefaultQuantizer,
      maxVideoBitrate: Int = 0,
      decoderThreads: Int = 1,
      encoderThreads: Int = 1,
      videoOnly: Boolean = false,
      audioBitrate: Int = DefaultAudioBitrateInKbps * 1024,
      audioOnly: Boolean = false,
      infile: File = new File("."),
      outfile: File = new File(".")
    )
    val parser = new scopt.OptionParser[Config]("transcode") {
      head("transcode", "0.7")

      opt[Int]('i', "iterations") optional() action { (x, c) =>
        c.copy(iterations = x)
      } validate { x =>
        if (x > 0 && x <= MaxIterations) success else failure("iterations must be between 1 and %d".format(MaxIterations))
      } text ("iteration count of transcoding (for profiling, default: 1)")

      opt[Int]('s', "start") optional() action { (x, c) =>
        c.copy(start = x)
      } validate { x =>
        if (x >= 0) success else failure("start cannot be non-negative")
      } text ("start time in milliseconds (default: 0)")

      opt[Int]('d', "duration") optional() action { (x, c) =>
        c.copy(duration = x)
      } validate { x =>
        if (x > 0) success else failure("duration must be positive")
      } text ("duration in milliseconds (default: video track duration)")

      opt[Int]('h', "height") optional() action { (x, c) =>
        c.copy(height = x)
      } validate { x =>
        if (x > 0 && x <= 4096) success else failure("height has to be positive and less than 4096")
      } text ("output height (default: infile height)")

      opt[Unit]("square") optional() action { (_, c) =>
        c.copy(square = true)
      } text ("crop to 1:1 aspect ratio (default: false)")

      opt[Int]('o', "optimization") optional() action { (x, c) =>
        c.copy(optimization = x)
      } validate { x =>
        if (x >= 0) success else failure("optimization level has to be non-negative")
      } text ("video encoding optimization, H264:(%d fastest, %d slowest, default: %d) / VP8:(%d fastest, %d slowest, default: %d)".format(
        H264MinOptimization,
        H264MaxOptimization,
        H264DefaultOptimization,
        VP8MinOptimization,
        VP8MaxOptimization,
        VP8DefaultOptimization))

      opt[Double]('r', "crf") optional() action { (x, c) =>
        c.copy(crf = x)
      } validate { x =>
        if (x >= H264MinCRF && x <= H264MaxCRF) success else failure("crf has to be between %.1d - %.1f".format(H264MinCRF, H264MaxCRF))
      } text ("H.264 constant rate factor (%.1f to %.1f, %.1f best, default: %.1f)".format(H264MinCRF, H264MaxCRF, H264MinCRF, H264DefaultCRF))

      opt[Int]('q', "quantizer") optional() action { (x, c) =>
        c.copy(quantizer = x)
      } validate { x =>
        if (x >= VP8MinQuantizer && x <= VP8MaxQuantizer) success else failure("quantizer has to be between %d - %d".format(VP8MinQuantizer, VP8MaxQuantizer))
      } text ("VP8 quantizer (%d to %d, %d best, default: %d)".format(VP8MinQuantizer, VP8MaxQuantizer, VP8MinQuantizer, VP8DefaultQuantizer))

      opt[Int]("vbitrate") optional() action { (x, c) =>
        c.copy(maxVideoBitrate = x)
      } validate { x =>
        if (x >= 0) success else failure("video max bitrate has to be non-negative")
      } text ("max video bitrate, to cap CRF in extreme (allocate VBV buffer, default: 0)")

      opt[Int]("dthreads") optional() action { (x, c) =>
        c.copy(decoderThreads = x)
      } validate { x =>
        if (x > 0 && x <= MaxThreads) success else failure("decoder thread count has to be between 1 and %d".format(MaxThreads))
      } text ("H.264 decoder thread count (default: 1)")

      opt[Int]("ethreads") optional() action { (x, c) =>
        c.copy(encoderThreads = x)
      } validate { x =>
        if (x > 0 && x <= MaxThreads) success else failure("encoder thread count has to be between 1 and %d".format(MaxThreads))
      } text ("H.264 encoder thread count (default: 1)")

      opt[Unit]("vonly") optional() action { (_, c) =>
        c.copy(videoOnly = true)
      } text ("transcode only video (default: false)")

      opt[Int]("abitrate") optional() action { (x, c) =>
        c.copy(audioBitrate = x)
      } validate { x =>
        if (x > 0) success else failure("audio bitrate has to be non-negative")
      } text ("audio bitrate (default: %d Kbps)".format(DefaultAudioBitrateInKbps))

      opt[Unit]("aonly") optional() action { (_, c) =>
        c.copy(audioOnly = true)
      } text ("transcode only audio (default: false)")

      arg[File]("infile") action { (x, c) =>
        c.copy(infile = x)
      } text ("input file")

      arg[File]("outfile") action { (x, c) =>
        c.copy(outfile = x)
      } text ("output file")
    }
    var timeMeasurements = ArrayBuffer[Double]()
    parser.parse(args, Config()) map { config =>
      try {
        val splitTxt = config.outfile.getAbsolutePath.split('.')
        require(splitTxt.length >= 2, "File path must have an extension")
        val outputFileType: OutputFileType = splitTxt.last match {
          case "mp4" | "m4a" | "m4v" | "mov" => MP4
          case "ts" => MP2TS
          case "webm" => WebM
          case _ => UnknownFileType
        }
        require(outputFileType != UnknownFileType, "Output content type is unknown")

        val data = common.Data(config.infile.getAbsolutePath)
        // First iteration takes longer due to dynamic loading. Do an extra iteration and don't count first iteration towards profiling
        val iterations = if (config.iterations == 1) 1 else config.iterations + 1
        for (i <- 0 until iterations) {
          val iterationStartTime = System.currentTimeMillis()
          for (movie <- managed(demux.Movie(data))) {

            case class PtsAndTimescale(pts: Long, timescale: Long)
            var firstPtsAndTimescale: Option[PtsAndTimescale] = None
            def filterPts(pts: Long, timescale: Long, editBoxes: Seq[EditBox], start: Int, duration: Int): Long = {
              val newPts = EditBox.realPts(editBoxes, pts)
              val time = 1000.0f * newPts / timescale
              if (newPts >= 0 && (time >= start && time < (start + duration))) {
                if (firstPtsAndTimescale.isEmpty) {
                  firstPtsAndTimescale = Some(PtsAndTimescale(newPts, timescale))
                }
                newPts
              } else {
                -1
              }
            }

            val videoSettings = movie.videoTrack.settings
            val audioSettings = movie.audioTrack.settings

            require(!(config.videoOnly && config.audioOnly), "Only use one of the --aonly --vonly parameters")
            require(!(config.videoOnly && videoSettings.timescale == 0), "File does not contain any valid video track")
            require(!(config.audioOnly && audioSettings.sampleRate == 0), "File does not contain any valid audio track")
            require(!(videoSettings.timescale == 0 && audioSettings.sampleRate == 0), "File does not contain any audio/video tracks")

            val transcodeVideo = if (config.audioOnly || (movie.videoTrack.duration() == 0)) false else true
            val transcodeAudio = if (config.videoOnly || (movie.audioTrack.duration() == 0)) false else true

            val (minOptimization, maxOptimization, defaultOptimization) = outputFileType match {
              case MP4 | MP2TS => (H264MinOptimization, H264MaxOptimization, H264DefaultOptimization)
              case WebM => (VP8MinOptimization, VP8MaxOptimization, VP8DefaultOptimization)
            }
            val optimization = if (transcodeVideo) config.optimization match {
              case -1 => defaultOptimization
              case o if o <  minOptimization || o > maxOptimization => throw new IllegalArgumentException("optimization level has to be between %d and %d".format(minOptimization, maxOptimization))
              case _ => config.optimization
            } else -1

            val inputDuration = if (transcodeVideo) movie.videoTrack.duration() else movie.audioTrack.duration()
            val timescale = if (transcodeVideo) videoSettings.timescale else audioSettings.sampleRate
            val inputDurationInMs: Int = (1000.0f * inputDuration / timescale).toInt
            val duration: Int = math.max(math.min(config.duration, inputDurationInMs - config.start), 0)
            require(duration > 0, "No content in the given time range")

            if (i == 0) {
              val content = if (transcodeVideo) { if (transcodeAudio) "video with audio" else "video" } else "audio"
              println("Transcoding %s of duration %d ms, starting from %d ms".format(content, duration, config.start))
            }

            val outputVideoTrackOpt = {
              // Helper functions
              def outputSize() = {
                val inWidth = videoSettings.width
                val inHeight = videoSettings.height
                val outWidth = config.height match {
                  case h if h > 0 => if (inWidth > inHeight) common.Math.roundDivide(inWidth, h, inHeight) else h
                  case _ => inWidth
                }
                val outHeight = config.height match {
                  case h if h > 0 => if (inWidth > inHeight) h else common.Math.roundDivide(inHeight, h, inWidth)
                  case _ => inHeight
                }
                if (config.square) {
                  val minDim: Int = math.min(outWidth, outHeight)
                  (minDim, minDim)
                } else {
                  (outWidth, outHeight)
                }
              }

              def convertFrame(frame: Frame, outWidth: Int, outHeight: Int): Option[Frame] = {
                val inWidth = videoSettings.width
                val inHeight = videoSettings.height
                val (outWidth, outHeight) = outputSize()
                val square = outWidth == outHeight
                val videoEditBoxes = movie.videoTrack.editBoxes()
                val newPts = filterPts(frame.pts, videoSettings.timescale, videoEditBoxes, config.start, duration)
                val firstPts = if (newPts == -1) 0 else firstPtsAndTimescale.get.pts * videoSettings.timescale / firstPtsAndTimescale.get.timescale
                val adjustedPts = newPts - firstPts
                if (adjustedPts >= 0) {
                  Some(Frame(adjustedPts, () => {
                    val minDim = math.min(inWidth, inHeight)
                    val cropXOffset = if (square) (inWidth - minDim) >> 1 else 0
                    val cropYOffset = if (square) (inHeight - minDim) >> 1 else 0
                    val yuvCroppedFunc = {
                      if (cropXOffset == 0 && cropYOffset == 0) {
                        frame.yuv
                      } else {
                        () => frame.yuv().crop(cropXOffset, cropYOffset, minDim, minDim)
                      }
                    }
                    val yuvScaledFunc = {
                      if (outHeight == inHeight) {
                        yuvCroppedFunc
                      } else {
                        () => yuvCroppedFunc().scale(outHeight, inHeight)
                      }
                    }
                    val yuvRotatedFunc = {
                      if (videoSettings.orientation == settings.Video.Orientation.Landscape) {
                        yuvScaledFunc
                      } else {
                        () => yuvScaledFunc().rotate(videoSettings.orientation)
                      }
                    }
                    yuvRotatedFunc()
                  }))
                } else None
              }

              def convertSettings(originalSettings: settings.Video, outWidth: Int, outHeight: Int): settings.Video = {
                val codecType = outputFileType match {
                  case MP4 | MP2TS => settings.Video.Codec.H264
                  case WebM => settings.Video.Codec.VP8
                }
                settings.Video(
                  codecType,
                  if (originalSettings.orientation % 2 == 0) outWidth.toShort else outHeight.toShort,
                  if (originalSettings.orientation % 2 == 0) outHeight.toShort else outWidth.toShort,
                  originalSettings.timescale,
                  settings.Video.Orientation.Landscape,
                  originalSettings.spsPps
                )
              }

              // Setup transcode operation
              if (transcodeVideo) {
                Some(managed(decode.Video(movie.videoTrack)) flatMap { decoder =>
                  require(videoSettings.codec == settings.Video.Codec.H264, "Only H.264 video input is supported")

                  // Get transcoding parameters
                  val inWidth = videoSettings.width
                  val inHeight = videoSettings.height
                  val (outWidth, outHeight) = outputSize()

                  // Print info
                  if (i == 0) {
                    print("Video resolution %dx%d".format(outWidth, outHeight))
                    println(if (outWidth != inWidth || outHeight != inHeight) ", resized from %dx%d".format(inWidth, inHeight) else "")
                    print("Optimization = " + optimization)
                    print(if (outputFileType == MP4 || outputFileType == MP2TS) ", CRF = " + config.crf.toFloat else ", Quantizer = " + config.quantizer)
                    println(if (config.maxVideoBitrate != 0) ", max bitrate = " + config.maxVideoBitrate else "")
                    println("Threads = %d (decoder), %d (encoder)".format(config.decoderThreads, if (outputFileType == MP4 || outputFileType == MP2TS) config.encoderThreads else 1))
                  }

                  val convertedTrack = decoder.flatMap({ frame: Frame =>
                    convertFrame(frame, outWidth, outHeight)
                  },
                  convertSettings(_, outWidth, outHeight))
                  outputFileType match {
                    case MP4 | MP2TS => {
                      managed(encode.H264(convertedTrack,
                        config.crf.toFloat,
                        optimization,
                        movie.videoTrack.fps(),
                        config.maxVideoBitrate,
                        config.encoderThreads))
                    }
                    case WebM => {
                      managed(encode.VP8(convertedTrack,
                        config.quantizer,
                        optimization,
                        movie.videoTrack.fps(),
                        config.maxVideoBitrate))
                    }
                  }
                })
              } else {
                None
              }
            }

            val outputAudioTrackOpt = {
              def convertSound(sound: Sound): Option[Sound] = {
                val audioEditBoxes = movie.audioTrack.editBoxes()
                val newPts = filterPts(sound.pts, audioSettings.sampleRate, audioEditBoxes, config.start, duration)
                val firstPts = if (newPts == -1) 0 else firstPtsAndTimescale.get.pts * audioSettings.sampleRate / firstPtsAndTimescale.get.timescale
                val adjustedPts = newPts - firstPts
                if (adjustedPts >= 0) Some(Sound(adjustedPts, () => sound.pcm())) else None
              }

              // Setup transcode operation
              if (transcodeAudio) {
                Some(managed(decode.Audio(movie.audioTrack)) flatMap { decoder =>
                  // Print info
                  if (i == 0) {
                    println("Audio channels = %d, bitrate = %.1f Kbps".format(audioSettings.channels, config.audioBitrate / 1024.0f))
                  }
                  val convertedTrack = decoder.map(convertSound(_)).flatten
                  outputFileType match {
                    case MP4 | MP2TS => managed(encode.AAC(convertedTrack, audioSettings.channels, config.audioBitrate))
                    case WebM => managed(encode.Vorbis(convertedTrack, audioSettings.channels, config.audioBitrate))
                  }
                })
              } else {
                None
              }
            }

            // Main transcode loop
            def managedOutput(
              outputAudioTrack: functional.types.Media[() => encode.Sample, settings.Audio],
              outputVideoTrack: functional.types.Media[() => encode.Sample, settings.Video]
            ) = outputFileType match {
              case MP4 => managed(mux.MP4(outputAudioTrack, outputVideoTrack)).map(_().array())
              case MP2TS => managed(mux.MP2TS(outputAudioTrack, outputVideoTrack)).map(_().array())
              case WebM => managed(mux.WebM(outputAudioTrack, outputVideoTrack)).map(_().array())
            }

            def saveOutputOnce(output: Array[Byte]): Unit = {
              if (i == 0) {
                for (outFile <- managed(new java.io.FileOutputStream(config.outfile.getAbsolutePath))) {
                  outFile.write(output)
                }
              }
            }

            (outputAudioTrackOpt, outputVideoTrackOpt) match {
              case (Some(audio), Some(video)) => {
                for {
                  outputAudioTrack <- audio
                  outputVideoTrack <- video
                } {
                  managedOutput(outputAudioTrack, outputVideoTrack).acquireAndGet(saveOutputOnce(_))
                }
              }
              case (None, Some(video)) => {
                for (outputVideoTrack <- video) {
                  val outputAudioTrack = functional.Audio[() => encode.Sample]()
                  managedOutput(outputAudioTrack, outputVideoTrack).acquireAndGet(saveOutputOnce(_))
                }
              }
              case (Some(audio), None) => {
                for (outputAudioTrack <- audio) {
                  val outputVideoTrack = functional.Video[() => encode.Sample]()
                  managedOutput(outputAudioTrack, outputVideoTrack).acquireAndGet(saveOutputOnce(_))
                }
              }
              case _ => throw new java.lang.Exception("should not happen")
            }
          }
          val iterationEndTime = System.currentTimeMillis()
          if (iterations == 1 || i > 0) {
            timeMeasurements += (iterationEndTime - iterationStartTime)
          }
        }
        println("Transcoding time stats over %d iterations:".format(timeMeasurements.size))
        if (iterations > 1) {
          println("[Mean]      %.3f ms".format(Math.mean(timeMeasurements)))
          println("[Variance]  %.3f ms".format(Math.variance(timeMeasurements)))
          println("[Std. Dev.] %.3f ms".format(Math.stdDev(timeMeasurements)))
        } else {
          println("[Total] %.3f ms".format(Math.mean(timeMeasurements)))
        }
      } catch {
        case e: Throwable => {
          println("Error transcoding movie: %s".format(e.getMessage()))
        }
      }
    } getOrElse {
    }
  }
}

