package com.twitter.vireo

object ErrorMessagePatternMatcher {
  val Pattern = """(.*)?\[(.+) \((\d+)\)] (\w+\s?\w*\s?\w*): \"(.+)\" condition failed; reason: (.+)""".r
  def results(message: String) = {
    message match {
      case Pattern(_, function, line, what, condition, reason) => (what, function, line.toInt, condition, reason)
      case _ => ("unknown", "", -1, "", "")
    }
  }
}

sealed abstract class ExceptionType
object ExceptionType {
  case object InternalError extends ExceptionType
  case object InvalidFile extends ExceptionType
  case object ReaderError extends ExceptionType
  case object UnsupportedFile extends ExceptionType
  case object UnknownError extends ExceptionType
}

case class VireoException(message: String) extends Exception(message) {
  val (what, function, line, condition, reason) = ErrorMessagePatternMatcher.results(message)
  val exceptionType = what match {
    case "imagecore" => ExceptionType.InternalError
    case "internal inconsistency" => ExceptionType.InternalError
    case "invalid" => ExceptionType.InvalidFile
    case "invalid arguments" => ExceptionType.InternalError
    case "reader error" => ExceptionType.ReaderError
    case "out of memory" => ExceptionType.InternalError
    case "out of range" => ExceptionType.InternalError
    case "overflow" => ExceptionType.InternalError
    case "uninitialized" => ExceptionType.InternalError
    case "unsafe" => ExceptionType.UnsupportedFile
    case "unsupported" => ExceptionType.UnsupportedFile
    case _ => ExceptionType.UnknownError
  }
}
