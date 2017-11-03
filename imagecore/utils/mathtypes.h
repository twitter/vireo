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

#pragma once

// unsigned rational
template<typename integralType>
struct Rational
{
	Rational() : m_Nominator(0), m_Denominator(1), m_Signed(false) {}

	Rational(integralType nominator, integralType denominator, bool isSigned) : m_Nominator(nominator), m_Denominator(denominator), m_Signed(isSigned) {}

	integralType getInt() const {
		SECURE_ASSERT(m_Denominator != 0);
		return m_Nominator / m_Denominator;
	}

	float getDecimal() const {
		return (float)(m_Nominator % m_Denominator) / m_Denominator;
	}

	float getFloat() const {
		SECURE_ASSERT(m_Denominator != 0);
		return (float)m_Nominator / m_Denominator;
	}

	bool operator == (const Rational<integralType> &other) const {
		return (m_Nominator == other.m_Nominator) && (m_Denominator == other.m_Denominator) && (m_Signed == other.m_Signed);
	}

	static uint32_t sizeOf() {
		return sizeof(integralType) * 2;
	}

	integralType m_Nominator;
	integralType m_Denominator;
	bool m_Signed; // because some images don't respect the exact exif spec we need an override flag for the sign
};

union type64 {
	int64_t m_64;
	int32_t m_32[2];
	int8_t m_8[8];
};
