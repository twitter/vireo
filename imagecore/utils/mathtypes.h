//
//  mathtypes.h
//  ImageCore
//
//  Created by Paul Melamed on 9/30/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

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
