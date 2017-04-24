//
//  colorPalette.h
//  ImageCore
//
//  Created by Yiming Li on 6/13/16.
//  Copyright © 2016 Twitter. All rights reserved.
//

#pragma once

#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/colorspace.h"
#include <vector>

namespace imagecore {

enum EColorsAlgorithm
{
	kColorAlgorithm_Histogram,
	kColorAlgorithm_KMeans
};

class ColorPalette
{
public:
	static int compute(ImageRGBA* image, RGBA* outColors, double* colorPct, int maxColors, EColorsAlgorithm algorithm);
};

}
