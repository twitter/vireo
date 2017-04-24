import com.twitter.vireo.MP4;

class MP4Test {
  public static void main(String[] args) {
    System.loadLibrary("vireo-java");
    try (MP4 decoder = new MP4()) {
      decoder.read(args[0]);
      System.out.println(java.util.Arrays.toString(decoder.timestamps));
    }
  }
}
