/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "colorpalette.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/utils/securemath.h"
#include "imagecore/formats/format.h"
#include "imagecore/image/image.h"
#include "imagecore/image/rgba.h"
#include "imagecore/image/resizecrop.h"
#include "imagecore/image/colorspace.h"
#include <algorithm>
#include <cstdlib>

namespace imagecore {
using namespace std;

// Histogram based algorithm

#define INDEX_HIST(x,y,z) ((z) + (y) * kHistTotalSize + (x) * kHistTotalSize * kHistTotalSize)

int wrappedDist(int ha, int hb, int dist) {
	return abs(ha - hb) % dist;
}

struct WeightedColor {
	RGBA color;
	float pct;
	bool operator<(const WeightedColor& color) const {
		return pct < color.pct;
	}
};

static int computeHistogram(ImageRGBA* frameImage, RGBA* ccolors, double* colorPct, int numColors)
{
	unsigned int width = frameImage->getWidth();
	unsigned int height = frameImage->getHeight();

	bool useHsv = numColors == 1;

	int kHistSize = useHsv ? 16 : 24;

	const int searchSizeX = useHsv ? 1 : 4;
	const int searchSizeY = useHsv ? 2 : 1;
	const int searchSizeZ = useHsv ? 4 : 1;

	const int kHistPadding = max(searchSizeX, max(searchSizeY, searchSizeZ));
	const int kHistTotalSize = kHistSize + kHistPadding * 2;
	float* histogramAlloc = (float*)calloc(kHistTotalSize * kHistTotalSize * kHistTotalSize, sizeof(float));
	float* histogram = histogramAlloc + (kHistPadding + kHistPadding * kHistTotalSize + kHistPadding * kHistTotalSize * kHistTotalSize);

	float3* floatImage = (float3*)malloc(width * height * sizeof(float3));

	unsigned int framePitch;
	uint8_t* buffer = frameImage->lockRect(width, height, framePitch);


	START_CLOCK(ColorConversionHistogram);

	for( unsigned int y = 0; y < height; y++ ) {
		for( unsigned int x = 0; x < width; x++ ) {
			const RGBA* rgba = (RGBA*)(&buffer[y * framePitch + x * 4]);
			if (useHsv) {
				floatImage[y * width + x] = ColorSpace::srgbToHsv(ColorSpace::byteToFloat(*rgba));
			} else {
				floatImage[y * width + x] = ColorSpace::srgbToLab(ColorSpace::byteToFloat(*rgba));
			}
			if( rgba->a > 128 ) {
				const float3& c = floatImage[y * width + x];
				unsigned int hx = clamp(0, kHistSize - 1, (int)(c.x * (float)kHistSize));
				unsigned int hy = clamp(0, kHistSize - 1, (int)(c.y * (float)kHistSize));
				unsigned int hz = clamp(0, kHistSize - 1, (int)(c.z * (float)kHistSize));
				int idx = INDEX_HIST(hx, hy, hz);
				float saturation = sqrtf(((c.y - 0.5f) * (c.y - 0.5f) + (c.z - 0.5f) * (c.z - 0.5f)));
				float luminance = fabs(c.x - 0.5f);
				histogram[idx] += useHsv ? 1.0f : fmaxf(luminance, saturation);
			}
		}
	}

	END_CLOCK(ColorConversionHistogram);


	START_CLOCK(Search);

	std::vector<int> vAreaMaxX;
	std::vector<int> vAreaMaxY;
	std::vector<int> vAreaMaxZ;

	for( int i = 0; i < numColors; i++ ) {
		int areaMaxX = 0;
		int areaMaxY = 0;
		int areaMaxZ = 0;
		float maxAreaSum = 0.0f;

		for( int x = 0; x < kHistSize; x++ ) {
			for( int y = 0; y < kHistSize; y++ ) {
				for( int z = 0; z < kHistSize; z++ ) {
					float sum = 0;
					for( int sx = -searchSizeX; sx <= searchSizeX; sx++ ) {
						int wrappedX = x + sx;
						if (useHsv) {
							// Hue in HSV is circular, so we wrap instead of clamp.
							if( wrappedX >= kHistSize ) {
								wrappedX = wrappedX - kHistSize;
							} else if( wrappedX < 0 ) {
								wrappedX = kHistSize + wrappedX - 1;
							}
						}
						for( int sy = -searchSizeY; sy <= searchSizeY; sy++ ) {
							int index = INDEX_HIST(wrappedX, y + sy, z);
							for( int sz = -searchSizeZ; sz <= searchSizeZ; sz++ ) {
								sum += histogram[index + sz];
							}
						}
					}
					if( sum > maxAreaSum ) {
						areaMaxX = x;
						areaMaxY = y;
						areaMaxZ = z;
						maxAreaSum = sum;
					}
				}
			}
		}

		if( maxAreaSum > 0.0f ) {
			vAreaMaxX.push_back(areaMaxX);
			vAreaMaxY.push_back(areaMaxY);
			vAreaMaxZ.push_back(areaMaxZ);
			if( numColors > 1 ) {
				for( int sx = -searchSizeX; sx <= searchSizeX; sx++ ) {
					int wrappedX = areaMaxX + sx;
					if( useHsv ) {
						// Hue in HSV is circular, so we wrap instead of clamp.
						if( wrappedX >= kHistSize ) {
							wrappedX = wrappedX - kHistSize;
						} else if( wrappedX < 0 ) {
							wrappedX = kHistSize + wrappedX - 1;
						}
					}
					for( int sy = -searchSizeY; sy <= searchSizeY; sy++ ) {
						int index = INDEX_HIST(wrappedX, areaMaxY + sy, areaMaxZ);
						for( int sz = -searchSizeZ; sz <= searchSizeZ; sz++ ) {
							histogram[index + sz] = -fabs(histogram[index + sz]);
						}
					}
				}
			}
		}
	}

	END_CLOCK(Search);


	START_CLOCK(Sort);

	int numOutColors = 0;
	WeightedColor* weightedColors = (WeightedColor*)malloc(numColors * sizeof(WeightedColor));

	SECURE_ASSERT(vAreaMaxX.size() == vAreaMaxY.size() && vAreaMaxX.size() == vAreaMaxZ.size());
	for( int m = 0; m < min((int)vAreaMaxX.size(), numColors); m++ ) {
		int areaMaxX = vAreaMaxX[m];
		int areaMaxY = vAreaMaxY[m];
		int areaMaxZ = vAreaMaxZ[m];
		std::vector<unsigned char> rDim;
		std::vector<unsigned char> gDim;
		std::vector<unsigned char> bDim;
		std::vector<RGBA> colors;
		int rAvg = 0;
		int gAvg = 0;
		int bAvg = 0;
		int mmax = 0;
		for( unsigned int y = 0; y < height; y++ ) {
			for( unsigned int x = 0; x < width; x++ ) {
				const RGBA* rgba = (RGBA*)(&buffer[y * framePitch + x * 4]);
				const float3& c = floatImage[y * width + x];
				int hx = clamp(0, kHistSize - 1, (int)(c.x * (float)kHistSize));
				int hy = clamp(0, kHistSize - 1, (int)(c.y * (float)kHistSize));
				int hz = clamp(0, kHistSize - 1, (int)(c.z * (float)kHistSize));
				if( (useHsv && rgba->a > 128 && (wrappedDist(hx, areaMaxX, kHistSize) <= searchSizeX && abs(hy - areaMaxY) <= searchSizeY && abs(hz - areaMaxZ) <= searchSizeZ))
				   || (!useHsv && rgba->a > 128 && (abs(hx - areaMaxX) <= searchSizeX && abs(hy - areaMaxY) <= searchSizeY && abs(hz - areaMaxZ) <= searchSizeZ))) {
					rAvg += rgba->r;
					gAvg += rgba->g;
					bAvg += rgba->b;
					rDim.push_back(rgba->r);
					gDim.push_back(rgba->g);
					bDim.push_back(rgba->b);
					colors.push_back(*rgba);
					mmax++;
				}
			}
		}

		double pct = (double)colors.size() / (double)(width * height);

		if( pct > 0.001f) {
			std::sort(rDim.begin(), rDim.end());
			std::sort(gDim.begin(), gDim.end());
			std::sort(bDim.begin(), bDim.end());

			int medianR = rDim.size() > 0 ? rDim.at(rDim.size() / 2) : 0;
			int medianG = gDim.size() > 0 ? gDim.at(gDim.size() / 2) : 0;
			int medianB = bDim.size() > 0 ? bDim.at(bDim.size() / 2) : 0;

			RGBA outColor(0, 0, 0, 255);

			int closestColorDist = 10000000;
			for( unsigned i = 0; i < colors.size(); i++ ) {
				const RGBA& c = colors.at(i);
				float dr = c.r - medianR;
				float dg = c.g - medianG;
				float db = c.b - medianB;
				float dist = (dr * dr) + (dg * dg) + (db * db);
				if( dist < closestColorDist ) {
					weightedColors[numOutColors].color = c;
					weightedColors[numOutColors].pct = pct;
					closestColorDist = dist;
					if( dist == 0 ) {
						break;
					}
				}
			}
			numOutColors++;
		}
	}

	std::sort(weightedColors, weightedColors + numOutColors);

	for( int i = 0; i < numOutColors; i++ ) {
		ccolors[i] = weightedColors[numOutColors - i - 1].color;
		colorPct[i] = weightedColors[numOutColors - i - 1].pct;
	}

	END_CLOCK(Sort);

	free(weightedColors);
	weightedColors = NULL;

	free(floatImage);
	floatImage = NULL;

	free(histogramAlloc);
	histogramAlloc = NULL;

	return numOutColors;
}

// Kmeans based algorithm

#define MAX_ITERATIONS 100
#define DELTA_LIMIT 0.00000001

struct colorSample {
	RGBA rgba;
	float3 lab;
	int label = -1;
	uint64_t clusterSize = 0;
	colorSample() {}
	colorSample(RGBA rgba, float3 lab, int label, int size) : rgba(rgba), lab(lab), label(label), clusterSize(size) {}
	colorSample& operator=(const colorSample& s)
	{
		rgba = s.rgba;
		lab = s.lab;
		label = s.label;
		clusterSize = s.clusterSize;
		return *this;
	}
	bool operator<(const colorSample& color) const {
		return clusterSize < color.clusterSize;
	}
};

static vector<colorSample> initializeCentroids(unsigned int numCluster, vector<colorSample>& samples) {
	vector<colorSample> centroids;
	uint64_t size = samples.size();
	// initialize using random samples in the image
	srand(817504253); // feed a fixed prime seed for deterministic results and easy testing
	for( unsigned int k = 0; k < numCluster; k++ ) {
		uint64_t randomIdx = rand() % size;
		unsigned int count = 0;
		for( unsigned int i = 0; i < k; i++ ) {
			while( samples[randomIdx].rgba == centroids[i].rgba && count < 10 ) { // get rid of same colors as initial centroids, but do not try too hard
				randomIdx = rand() % size;
				count++;
			}
		}
		samples[randomIdx].label = k;
		samples[randomIdx].clusterSize++;
		centroids.push_back(samples[randomIdx]);
	}
	return centroids;
}

static float squaredDist(const colorSample& color1, const colorSample& color2) {
	float dx = color1.lab.x - color2.lab.x;
	float dy = color1.lab.y - color2.lab.y;
	float dz = color1.lab.z - color2.lab.z;
	return dx * dx + dy * dy + dz * dz;
}

static void sampleLabeling(vector<colorSample> &samples, vector<colorSample> &centroids) {
	for( colorSample& sample : samples ) {
		float currMinDist = std::numeric_limits<float>::max();
		for( colorSample& centroid : centroids ) {
			float currDist = squaredDist(sample, centroid);
			if( currDist < currMinDist ) {
				currMinDist = currDist;
				sample.label = centroid.label;
			}
		}
		centroids[sample.label].clusterSize++;
	}
	for( colorSample& sample : samples ) {
		sample.clusterSize = centroids[sample.label].clusterSize;
	}
}

static vector<colorSample> getCentroids(vector<colorSample> samples, unsigned int numCluster) {
	vector<float3> sums(numCluster, 0);
	vector<uint64_t> clusterSize(numCluster, 0);
	for( colorSample& sample : samples ){
		sums[sample.label].x += sample.lab.x;
		sums[sample.label].y += sample.lab.y;
		sums[sample.label].z += sample.lab.z;
		clusterSize[sample.label] = sample.clusterSize;
	}

	vector<colorSample> centroids;
	for( unsigned int k = 0; k < numCluster; k++ ) {
		float3 lab(sums[k].x / clusterSize[k], sums[k].y / clusterSize[k], sums[k].z / clusterSize[k]);
		RGBA rgba = ColorSpace::floatToByte(ColorSpace::labToSrgb(lab));
		centroids.push_back(colorSample(rgba, lab, k, 0));
	}
	return centroids;
}

static int computeKmeans(imagecore::ImageRGBA *frameImage, unsigned int numCluster, RGBA* colorPalette, double* colorPct) {
	SECURE_ASSERT(frameImage != NULL);
	SECURE_ASSERT(numCluster >= 2 && numCluster <= 10); // don't want large numbers for k
	unsigned int width = frameImage->getWidth();
	unsigned int height = frameImage->getHeight();
	vector<colorSample> samples;
	unsigned int framePitch;
	uint8_t* buffer = frameImage->lockRect(width, height, framePitch);

	// get all the samples (whose alpha value is > 128)
	for( unsigned int y = 0; y < height; y++ ) {
		for( unsigned int x = 0; x < width; x++ ) {
			const RGBA rgba = *(RGBA*)(&buffer[y * framePitch + x * 4]);
			if( rgba.a > 128 ) {
				float3 lab = ColorSpace::srgbToLab(ColorSpace::byteToFloat(rgba));
				colorSample sample(rgba, lab, -1, 0);
				samples.push_back(sample);
			}
		}
	}
	SECURE_ASSERT(samples.size() > 0 && samples.size() <= width * height);

	vector<colorSample> centroids = initializeCentroids(numCluster, samples);
	// main loop for k-means clustering
	for( unsigned int iteration = 0; iteration < MAX_ITERATIONS; iteration++ ) {
		sampleLabeling(samples, centroids);
		vector<colorSample> newCentroids = getCentroids(samples, numCluster);
		//if newCentroids are closed enough to old centroids, stop iteration
		bool closeEnough = true;
		for( unsigned int k = 0; k < numCluster; k++ ) {
			if( squaredDist(centroids[k], newCentroids[k]) > DELTA_LIMIT ) {
				closeEnough = false;
			}
		}
		if( closeEnough ) {
			break;
		}
		centroids = newCentroids;
	}

	sort(centroids.begin(), centroids.end());
	unsigned int totalSize = width * height;
	unsigned int numOutColors = 0;
	colorPalette[numOutColors] = centroids[numCluster-1].rgba;
	colorPct[numOutColors] = (double)centroids[numCluster - 1].clusterSize / totalSize;
	for( int k = numCluster - 2; k >= 0; k-- ) {
		double pct = (double)centroids[k].clusterSize / totalSize;
		if( pct < 0.001f)  {
			continue;
		}
		if( centroids[k].rgba == colorPalette[numOutColors] ) {
			colorPct[numOutColors] += pct;
		} else {
			numOutColors++;
			colorPalette[numOutColors] = centroids[k].rgba;
			colorPct[numOutColors] = pct;
		}
	}
	return numOutColors + 1;
}

int ColorPalette::compute(ImageRGBA* image, RGBA* outColors, double* colorPct, int maxColors, EColorsAlgorithm algorithm) {
	if( algorithm == kColorAlgorithm_Histogram ) {
		return computeHistogram(image, outColors, colorPct, maxColors);
	} else if( algorithm == kColorAlgorithm_KMeans ) {
		return computeKmeans(image, maxColors, outColors, colorPct);
	}
	return 0;
}

}
