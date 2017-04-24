ImageTool
=========

* Usage:

    ImageTool identify <input>
    - Report the dimensions and image type of 'input'
    - input: image file or '-' for stdin

    ImageTool forcergb <input> <output> [options]
    - If 'input' is a JPEG with an ICC color profile, convert it to sRGB, writing the result to
      'output'. Otherwise, copy 'input' directly to 'output'. Note that compression only happens
      if the image is a JPEG with an ICC color profile.
    - input: image file or '-' for stdin
    - output: destination file or '-' for stdout
    - options:
      + '-filequality 0-100': default is 75
        The JPEG compression quality to encode with if the image is a JPEG.

    ImageTool resize <input> <output> <size> [options]
    - Resize 'input' according to 'size' and 'options', writing the result to 'output'
    - input: image file or '-' for stdin
    - output: destination file for result or '-' for stdout
    - size: target image dimenstions
      + format <width>x<height> or <width> where width/height is a number or a percentage
        (e.g. 120x200, 75%x65%, 120, 50%)
        note that <width> evaulates to <width>x<width>
    - options:
      + '-crop true|false': default is true
        If true, resize the image while keeping the same aspect ratio, then crop to target size.
      + '-gravity center|left|top|right|bottom': default is top for portraits, else center
        Determines which area of the image to keep if cropping is necessary.
      + '-region <width>x<height>L<left>T<top>': default is entire image
        Only applies the resizing to the region of the image specified. This is as if the image
        were cropped to that region before resizing.
      + '-minaxis true|false': default is true
        Only applies when crop is set to false.  If true, resize based on the smaller of
        the image's target dimensions, modifying the other target dimension to fit the image's
        current aspect ratio (else use the larger of the two).
      + '-resizequality 0-2': default is 2
        0 - do fast, normal quality resize using a 4x4 filter
        1 - do slower, high quality resize using an 8x8 filter
        2 - do slowest, highest quality resize using a 12x12 filter
      + '-quality 0-100': default is 75
        The JPEG compression quality to encode with if the input image is a JPEG.

    EXAMPLES:

      ImageTool identify -
      - Run identify on data from stdin

      ImageTool identify /usr/local/foo.png
      - Run identify on file /usr/local/foo.png

      ImageTool resize - - 500x750
      - Resize the image data from stdin to be 500x750 and write the result to stdout

      ImageTool resize /user/foo/img.jpg - 50% -filequality 100
      - Resize /user/foo/img.jpg to be half its size using JPEG compression quality of 100% and write to stdout

    ERROR CODES:

      0 -- SUCCESS
      1 -- UNKNOWN_ERROR
      2 -- OUT_OF_MEMORY
      3 -- INVALID_USAGE
      4 -- INVALID_FORMAT
      5 -- READ_ERROR
      6 -- WRITE_ERROR
      7 -- INVALID_IMAGE_SIZE
      8 -- INVALID_OUTPUT_SIZE

* Build requirements:

  - autotools
  - libjpeg or libjpeg-turbo. libjpeg-turbo is STRONGLY recommended, performance will suffer greatly with libjpeg and many features (RGBX output, YUV resampling) won't work or are implemented inefficiently.
  - libpng16
  - libwebp
  - liblcms2

* Building:

  autoreconf -fiv
  make
  make install
