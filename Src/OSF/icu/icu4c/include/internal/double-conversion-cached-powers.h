// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
//
// From the double-conversion library. Original license:
//
// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//  * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// ICU PATCH: ifdef around UCONFIG_NO_FORMATTING
#include "unicode/utypes.h"
#if !UCONFIG_NO_FORMATTING

#ifndef DOUBLE_CONVERSION_CACHED_POWERS_H_
#define DOUBLE_CONVERSION_CACHED_POWERS_H_

// ICU PATCH: Customize header file paths for ICU.

#include "double-conversion-diy-fp.h"

// ICU PATCH: Wrap in ICU namespace
U_NAMESPACE_BEGIN

namespace double_conversion {
namespace PowersOfTenCache {
// Not all powers of ten are cached. The decimal exponent of two neighboring
// cached numbers will differ by kDecimalExponentDistance.
static const int kDecimalExponentDistance = 8;

static const int kMinDecimalExponent = -348;
static const int kMaxDecimalExponent = 340;

// Returns a cached power-of-ten with a binary exponent in the range
// [min_exponent; max_exponent] (boundaries included).
void GetCachedPowerForBinaryExponentRange(int min_exponent, int max_exponent, DiyFp* power, int* decimal_exponent);

// Returns a cached power of ten x ~= 10^k such that
//   k <= decimal_exponent < k + kCachedPowersDecimalDistance.
// The given decimal_exponent must satisfy
//   kMinDecimalExponent <= requested_exponent, and
//   requested_exponent < kMaxDecimalExponent + kDecimalExponentDistance.
void GetCachedPowerForDecimalExponent(int requested_exponent, DiyFp* power, int* found_exponent);
}  // namespace PowersOfTenCache
}  // namespace double_conversion

// ICU PATCH: Close ICU namespace
U_NAMESPACE_END

#endif  // DOUBLE_CONVERSION_CACHED_POWERS_H_
#endif // ICU PATCH: close #if !UCONFIG_NO_FORMATTING
