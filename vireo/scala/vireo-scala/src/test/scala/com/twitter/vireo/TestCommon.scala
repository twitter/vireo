package com.twitter.vireo

object TestCommon {
  val TestPath = "../../tests"
  val TestSrcPath = TestPath + "/data";
  val TestDstPath = TestPath + "/results/scala";

  val resultsFile = new java.io.File(TestDstPath);
  resultsFile.mkdirs();
}
