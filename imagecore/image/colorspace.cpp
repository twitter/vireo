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

#include "imagecore.h"
#include "imagecore/utils/mathutils.h"
#include "imagecore/image/colorspace.h"
#include <algorithm>

float3 ColorSpace::byteToFloat(const RGBA& c)
{
	return float3((float)c.r / 255.0f, (float)c.g / 255.0f, (float)c.b / 255.0f);
}

RGBA ColorSpace::floatToByte(const float3& v)
{
	unsigned char cr = clamp(0, 255, (int)(v.x * 255.0f));
	unsigned char cg = clamp(0, 255, (int)(v.y * 255.0f));
	unsigned char cb = clamp(0, 255, (int)(v.z * 255.0f));
	return RGBA(cr, cg, cb);
}

static constexpr float sRGBGamma = 2.2f;
static constexpr float sRGBInvGamma = 1.0f / sRGBGamma;

// sRGB gamma correction

float3 ColorSpace::srgbToLinear(const float3& v)
{
	return float3(powf(v.x, sRGBGamma), powf(v.y, sRGBGamma), powf(v.z, sRGBGamma));
}

float3 ColorSpace::linearToSrgb(const float3& v)
{
	return float3(powf(v.x, sRGBInvGamma), powf(v.y, sRGBInvGamma), powf(v.z, sRGBInvGamma));
}

// From http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
float3 ColorSpace::srgbToHsv(const float3& cf)
{
	float3 c = cf;
	float K = 0.f;

	if (c.y < c.z) {
		std::swap(c.y, c.z);
		K = -1.f;
	}

	if (c.x < c.y) {
		std::swap(c.x, c.y);
		K = -2.f / 6.f - K;
	}

	float chroma = c.x - std::min(c.y, c.z);
	float h = fabs(K + (c.y - c.z) / (6.0f * chroma + 1e-20f));
	float s = chroma / (c.x + 1e-20f);
	float v = c.x;

	return float3(h, s, v);
}

static const float3 labSpaceMin(0.0f, -86.1777648f, 94.4802017f);
static const float3 labSpaceMax(100.0f, 98.2391204f, -107.852676f);
static const float3 labSpaceDelta = labSpaceMax - labSpaceMin;
static const float3 labWhitePointD65(95.0429f, 100.0f, 108.8900f);

static const float3 labMtxX(0.412453f, 0.357580f, 0.180423f);
static const float3 labMtxY(0.212671f, 0.715160f, 0.072169f);
static const float3 labMtxZ(0.019334f, 0.119193f, 0.950220f);

static const float3 invMtxX(3.2404542f, -1.5371385f, -0.4985314f);
static const float3 invMtxY(-0.9692660f, 1.8760108f, 0.0415560f);
static const float3 invMtxZ(0.0556434f, -0.2040259f, 1.0572252f);

float3 ColorSpace::srgbToLab(const float3& c)
{
	float eps = 216.0f / 24389.0f;
	float k = 24389.0f / 27.0f;
	float3 cf = srgbToLinear(c) * 100.0f;
	float3 l;
	l.x = labMtxX.dot(cf);
	l.y = labMtxY.dot(cf);
	l.z = labMtxZ.dot(cf);
	float3 r = l / labWhitePointD65;
	float fx = r.x > eps ? powf(r.x, 0.333333f) : ((k * r.x + 16.0f) / 116.0f);
	float fy = r.y > eps ? powf(r.y, 0.333333f) : ((k * r.y + 16.0f) / 116.0f);
	float fz = r.z > eps ? powf(r.z, 0.333333f) : ((k * r.z + 16.0f) / 116.0f);
	float3 labAbs(116.0f * fy - 16.0f, 500.0f * (fx - fy), 200.0f * (fy - fz));
	float3 labRel = (labAbs - labSpaceMin) / labSpaceDelta;
	return labRel;
}

float3 ColorSpace::labToSrgb(const float3& lab)
{
	float eps = 216.0f / 24389.0f;
	float k = 24389.0f / 27.0f;

	float3 absLab = labSpaceMin + lab * labSpaceDelta;

	float fy = (absLab.x + 16.0f) / 116.0f;
	float fz = fy - (absLab.z / 200.0f);
	float fx = fy + (absLab.y / 500.0f);

	float3 r;
	float fx3 = fx * fx * fx;
	float fz3 = fz * fz * fz;
	r.x = fx3 > eps ? fx3 : ((116.0f * fx - 16.0f) / k);
	r.z = fz3 > eps ? fz3 : ((116.0f * fz - 16.0f) / k);
	float yl = ((absLab.x + 16.0f) / 116.0f);
	r.y = absLab.x > eps ? (yl * yl * yl) : (absLab.x / k);

	float3 rw = (r * labWhitePointD65) / 100.0f;

	float3 rgb;
	rgb.x = invMtxX.dot(rw);
	rgb.y = invMtxY.dot(rw);
	rgb.z = invMtxZ.dot(rw);

	return linearToSrgb(rgb);
}
