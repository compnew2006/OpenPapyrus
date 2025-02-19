// normalizer2impl.cpp
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *   Copyright (C) 2009-2014, International Business Machines Corporation and others.  All Rights Reserved.
 *   encoding:   UTF-8
 *   created on: 2009nov22
 *   created by: Markus W. Scherer
 */
#include <icu-internal.h>
#pragma hdrstop
// #define UCPTRIE_DEBUG

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/ucptrie.h"
#include "bytesinkutil.h"
#include "ucptrie_impl.h"
#include "uset_imp.h"

U_NAMESPACE_BEGIN

namespace {
/**
 * UTF-8 lead byte for minNoMaybeCP.
 * Can be lower than the actual lead byte for c.
 * Typically U+0300 for NFC/NFD, U+00A0 for NFKC/NFKD, U+0041 for NFKC_Casefold.
 */
inline uint8 leadByteForCP(UChar32 c) {
	if(c <= 0x7f) {
		return (uint8)c;
	}
	else if(c <= 0x7ff) {
		return (uint8)(0xc0+(c>>6));
	}
	else {
		// Should not occur because ccc(U+0300)!=0.
		return 0xe0;
	}
}

/**
 * Returns the code point from one single well-formed UTF-8 byte sequence
 * between cpStart and cpLimit.
 *
 * Trie UTF-8 macros do not assemble whole code points (for efficiency).
 * When we do need the code point, we call this function.
 * We should not need it for normalization-inert data (norm16==0).
 * Illegal sequences yield the error value norm16==0 just like real normalization-inert code points.
 */
UChar32 codePointFromValidUTF8(const uint8 * cpStart, const uint8 * cpLimit) {
	// Similar to U8_NEXT_UNSAFE(s, i, c).
	U_ASSERT(cpStart < cpLimit);
	uint8 c = *cpStart;
	switch(cpLimit-cpStart) {
		case 1:
		    return c;
		case 2:
		    return ((c&0x1f)<<6) | (cpStart[1]&0x3f);
		case 3:
		    // no need for (c&0xf) because the upper bits are truncated after <<12 in the cast to (UChar)
		    return (UChar)((c<<12) | ((cpStart[1]&0x3f)<<6) | (cpStart[2]&0x3f));
		case 4:
		    return ((c&7)<<18) | ((cpStart[1]&0x3f)<<12) | ((cpStart[2]&0x3f)<<6) | (cpStart[3]&0x3f);
		default:
		    UPRV_UNREACHABLE_EXIT; // Should not occur.
	}
}

/**
 * Returns the last code point in [start, p[ if it is valid and in U+1000..U+D7FF.
 * Otherwise returns a negative value.
 */
UChar32 previousHangulOrJamo(const uint8 * start, const uint8 * p) {
	if((p - start) >= 3) {
		p -= 3;
		uint8 l = *p;
		uint8 t1, t2;
		if(0xe1 <= l && l <= 0xed &&
		    (t1 = (uint8)(p[1] - 0x80)) <= 0x3f &&
		    (t2 = (uint8)(p[2] - 0x80)) <= 0x3f &&
		    (l < 0xed || t1 <= 0x1f)) {
			return ((l & 0xf) << 12) | (t1 << 6) | t2;
		}
	}
	return U_SENTINEL;
}

/**
 * Returns the offset from the Jamo T base if [src, limit[ starts with a single Jamo T code point.
 * Otherwise returns a negative value.
 */
int32_t getJamoTMinusBase(const uint8 * src, const uint8 * limit) {
	// Jamo T: E1 86 A8..E1 87 82
	if((limit - src) >= 3 && *src == 0xe1) {
		if(src[1] == 0x86) {
			uint8 t = src[2];
			// The first Jamo T is U+11A8 but JAMO_T_BASE is 11A7.
			// Offset 0 does not correspond to any conjoining Jamo.
			if(0xa8 <= t && t <= 0xbf) {
				return t - 0xa7;
			}
		}
		else if(src[1] == 0x87) {
			uint8 t = src[2];
			if((int8_t)t <= (int8_t)0x82u) {
				return t - (0xa7 - 0x40);
			}
		}
	}
	return -1;
}

void appendCodePointDelta(const uint8 * cpStart, const uint8 * cpLimit, int32_t delta,
    ByteSink &sink, Edits * edits) {
	char buffer[U8_MAX_LENGTH];
	int32_t length;
	int32_t cpLength = (int32_t)(cpLimit - cpStart);
	if(cpLength == 1) {
		// The builder makes ASCII map to ASCII.
		buffer[0] = (uint8)(*cpStart + delta);
		length = 1;
	}
	else {
		int32_t trail = *(cpLimit-1) + delta;
		if(0x80 <= trail && trail <= 0xbf) {
			// The delta only changes the last trail byte.
			--cpLimit;
			length = 0;
			do {
				buffer[length++] = *cpStart++;
			} while(cpStart < cpLimit);
			buffer[length++] = (uint8)trail;
		}
		else {
			// Decode the code point, add the delta, re-encode.
			UChar32 c = codePointFromValidUTF8(cpStart, cpLimit) + delta;
			length = 0;
			U8_APPEND_UNSAFE(buffer, length, c);
		}
	}
	if(edits != nullptr) {
		edits->addReplace(cpLength, length);
	}
	sink.Append(buffer, length);
}
}  // namespace

// ReorderingBuffer -------------------------------------------------------- ***

ReorderingBuffer::ReorderingBuffer(const Normalizer2Impl &ni, UnicodeString & dest,
    UErrorCode & errorCode) :
	impl(ni), str(dest),
	start(str.getBuffer(8)), reorderStart(start), limit(start),
	remainingCapacity(str.getCapacity()), lastCC(0) {
	if(start == nullptr && U_SUCCESS(errorCode)) {
		// getBuffer() already did str.setToBogus()
		errorCode = U_MEMORY_ALLOCATION_ERROR;
	}
}

bool ReorderingBuffer::init(int32_t destCapacity, UErrorCode & errorCode) {
	int32_t length = str.length();
	start = str.getBuffer(destCapacity);
	if(start==NULL) {
		// getBuffer() already did str.setToBogus()
		errorCode = U_MEMORY_ALLOCATION_ERROR;
		return FALSE;
	}
	limit = start+length;
	remainingCapacity = str.getCapacity()-length;
	reorderStart = start;
	if(start==limit) {
		lastCC = 0;
	}
	else {
		setIterator();
		lastCC = previousCC();
		// Set reorderStart after the last code point with cc<=1 if there is one.
		if(lastCC>1) {
			while(previousCC()>1) {
			}
		}
		reorderStart = codePointLimit;
	}
	return TRUE;
}

bool ReorderingBuffer::equals(const UChar * otherStart, const UChar * otherLimit) const {
	int32_t length = (int32_t)(limit-start);
	return
		length==(int32_t)(otherLimit-otherStart) &&
		0==u_memcmp(start, otherStart, length);
}

bool ReorderingBuffer::equals(const uint8 * otherStart, const uint8 * otherLimit) const {
	U_ASSERT((otherLimit - otherStart) <= INT32_MAX); // ensured by caller
	int32_t length = (int32_t)(limit - start);
	int32_t otherLength = (int32_t)(otherLimit - otherStart);
	// For equal strings, UTF-8 is at least as long as UTF-16, and at most three times as long.
	if(otherLength < length || (otherLength / 3) > length) {
		return FALSE;
	}
	// Compare valid strings from between normalization boundaries.
	// (Invalid sequences are normalization-inert.)
	for(int32_t i = 0, j = 0;;) {
		if(i >= length) {
			return j >= otherLength;
		}
		else if(j >= otherLength) {
			return FALSE;
		}
		// Not at the end of either string yet.
		UChar32 c, other;
		U16_NEXT_UNSAFE(start, i, c);
		U8_NEXT_UNSAFE(otherStart, j, other);
		if(c != other) {
			return FALSE;
		}
	}
}

bool ReorderingBuffer::appendSupplementary(UChar32 c, uint8 cc, UErrorCode & errorCode) {
	if(remainingCapacity<2 && !resize(2, errorCode)) {
		return FALSE;
	}
	if(lastCC<=cc || cc==0) {
		limit[0] = U16_LEAD(c);
		limit[1] = U16_TRAIL(c);
		limit += 2;
		lastCC = cc;
		if(cc<=1) {
			reorderStart = limit;
		}
	}
	else {
		insert(c, cc);
	}
	remainingCapacity -= 2;
	return TRUE;
}

bool ReorderingBuffer::append(const UChar * s, int32_t length, bool isNFD,
    uint8 leadCC, uint8 trailCC,
    UErrorCode & errorCode) {
	if(!length) {
		return TRUE;
	}
	if(remainingCapacity<length && !resize(length, errorCode)) {
		return FALSE;
	}
	remainingCapacity -= length;
	if(lastCC<=leadCC || leadCC==0) {
		if(trailCC<=1) {
			reorderStart = limit+length;
		}
		else if(leadCC<=1) {
			reorderStart = limit+1; // Ok if not a code point boundary.
		}
		const UChar * sLimit = s+length;
		do {
			*limit++ = *s++;
		} while(s!=sLimit);
		lastCC = trailCC;
	}
	else {
		int32_t i = 0;
		UChar32 c;
		U16_NEXT(s, i, length, c);
		insert(c, leadCC); // insert first code point
		while(i<length) {
			U16_NEXT(s, i, length, c);
			if(i<length) {
				if(isNFD) {
					leadCC = Normalizer2Impl::getCCFromYesOrMaybe(impl.getRawNorm16(c));
				}
				else {
					leadCC = impl.getCC(impl.getNorm16(c));
				}
			}
			else {
				leadCC = trailCC;
			}
			append(c, leadCC, errorCode);
		}
	}
	return TRUE;
}

bool ReorderingBuffer::appendZeroCC(UChar32 c, UErrorCode & errorCode) {
	int32_t cpLength = U16_LENGTH(c);
	if(remainingCapacity<cpLength && !resize(cpLength, errorCode)) {
		return FALSE;
	}
	remainingCapacity -= cpLength;
	if(cpLength==1) {
		*limit++ = (UChar)c;
	}
	else {
		limit[0] = U16_LEAD(c);
		limit[1] = U16_TRAIL(c);
		limit += 2;
	}
	lastCC = 0;
	reorderStart = limit;
	return TRUE;
}

bool ReorderingBuffer::appendZeroCC(const UChar * s, const UChar * sLimit, UErrorCode & errorCode) {
	if(s==sLimit) {
		return TRUE;
	}
	int32_t length = (int32_t)(sLimit-s);
	if(remainingCapacity<length && !resize(length, errorCode)) {
		return FALSE;
	}
	u_memcpy(limit, s, length);
	limit += length;
	remainingCapacity -= length;
	lastCC = 0;
	reorderStart = limit;
	return TRUE;
}

void ReorderingBuffer::remove() {
	reorderStart = limit = start;
	remainingCapacity = str.getCapacity();
	lastCC = 0;
}

void ReorderingBuffer::removeSuffix(int32_t suffixLength) {
	if(suffixLength<(limit-start)) {
		limit -= suffixLength;
		remainingCapacity += suffixLength;
	}
	else {
		limit = start;
		remainingCapacity = str.getCapacity();
	}
	lastCC = 0;
	reorderStart = limit;
}

bool ReorderingBuffer::resize(int32_t appendLength, UErrorCode & errorCode) {
	int32_t reorderStartIndex = (int32_t)(reorderStart-start);
	int32_t length = (int32_t)(limit-start);
	str.releaseBuffer(length);
	int32_t newCapacity = length+appendLength;
	int32_t doubleCapacity = 2*str.getCapacity();
	if(newCapacity<doubleCapacity) {
		newCapacity = doubleCapacity;
	}
	if(newCapacity<256) {
		newCapacity = 256;
	}
	start = str.getBuffer(newCapacity);
	if(start==NULL) {
		// getBuffer() already did str.setToBogus()
		errorCode = U_MEMORY_ALLOCATION_ERROR;
		return FALSE;
	}
	reorderStart = start+reorderStartIndex;
	limit = start+length;
	remainingCapacity = str.getCapacity()-length;
	return TRUE;
}

void ReorderingBuffer::skipPrevious() {
	codePointLimit = codePointStart;
	UChar c = *--codePointStart;
	if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(*(codePointStart-1))) {
		--codePointStart;
	}
}

uint8 ReorderingBuffer::previousCC() {
	codePointLimit = codePointStart;
	if(reorderStart>=codePointStart) {
		return 0;
	}
	UChar32 c = *--codePointStart;
	UChar c2;
	if(U16_IS_TRAIL(c) && start<codePointStart && U16_IS_LEAD(c2 = *(codePointStart-1))) {
		--codePointStart;
		c = U16_GET_SUPPLEMENTARY(c2, c);
	}
	return impl.getCCFromYesOrMaybeCP(c);
}

// Inserts c somewhere before the last character.
// Requires 0<cc<lastCC which implies reorderStart<limit.
void ReorderingBuffer::insert(UChar32 c, uint8 cc) {
	for(setIterator(), skipPrevious(); previousCC()>cc;) {
	}
	// insert c at codePointLimit, after the character with prevCC<=cc
	UChar * q = limit;
	UChar * r = limit += U16_LENGTH(c);
	do {
		*--r = *--q;
	} while(codePointLimit!=q);
	writeCodePoint(q, c);
	if(cc<=1) {
		reorderStart = r;
	}
}

// Normalizer2Impl --------------------------------------------------------- ***

struct CanonIterData : public UMemory {
	CanonIterData(UErrorCode & errorCode);
	~CanonIterData();
	void addToStartSet(UChar32 origin, UChar32 decompLead, UErrorCode & errorCode);
	UMutableCPTrie * mutableTrie;
	UCPTrie * trie;
	UVector canonStartSets; // contains UnicodeSet *
};

Normalizer2Impl::~Normalizer2Impl() {
	delete fCanonIterData;
}

void Normalizer2Impl::init(const int32_t * inIndexes, const UCPTrie * inTrie,
    const uint16_t * inExtraData, const uint8 * inSmallFCD) {
	minDecompNoCP = static_cast<UChar>(inIndexes[IX_MIN_DECOMP_NO_CP]);
	minCompNoMaybeCP = static_cast<UChar>(inIndexes[IX_MIN_COMP_NO_MAYBE_CP]);
	minLcccCP = static_cast<UChar>(inIndexes[IX_MIN_LCCC_CP]);

	minYesNo = static_cast<uint16_t>(inIndexes[IX_MIN_YES_NO]);
	minYesNoMappingsOnly = static_cast<uint16_t>(inIndexes[IX_MIN_YES_NO_MAPPINGS_ONLY]);
	minNoNo = static_cast<uint16_t>(inIndexes[IX_MIN_NO_NO]);
	minNoNoCompBoundaryBefore = static_cast<uint16_t>(inIndexes[IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE]);
	minNoNoCompNoMaybeCC = static_cast<uint16_t>(inIndexes[IX_MIN_NO_NO_COMP_NO_MAYBE_CC]);
	minNoNoEmpty = static_cast<uint16_t>(inIndexes[IX_MIN_NO_NO_EMPTY]);
	limitNoNo = static_cast<uint16_t>(inIndexes[IX_LIMIT_NO_NO]);
	minMaybeYes = static_cast<uint16_t>(inIndexes[IX_MIN_MAYBE_YES]);
	U_ASSERT((minMaybeYes & 7) == 0); // 8-aligned for noNoDelta bit fields
	centerNoNoDelta = (minMaybeYes >> DELTA_SHIFT) - MAX_DELTA - 1;

	normTrie = inTrie;

	maybeYesCompositions = inExtraData;
	extraData = maybeYesCompositions+((MIN_NORMAL_MAYBE_YES-minMaybeYes)>>OFFSET_SHIFT);

	smallFCD = inSmallFCD;
}

U_CDECL_BEGIN

static uint32_t U_CALLCONV segmentStarterMapper(const void * /*context*/, uint32_t value) {
	return value&CANON_NOT_SEGMENT_STARTER;
}

U_CDECL_END

void Normalizer2Impl::addLcccChars(UnicodeSet & set) const {
	UChar32 start = 0, end;
	uint32_t norm16;
	while((end = ucptrie_getRange(normTrie, start, UCPMAP_RANGE_FIXED_LEAD_SURROGATES, INERT,
	    nullptr, nullptr, &norm16)) >= 0) {
		if(norm16 > Normalizer2Impl::MIN_NORMAL_MAYBE_YES &&
		    norm16 != Normalizer2Impl::JAMO_VT) {
			set.add(start, end);
		}
		else if(minNoNoCompNoMaybeCC <= norm16 && norm16 < limitNoNo) {
			uint16_t fcd16 = getFCD16(start);
			if(fcd16 > 0xff) {
				set.add(start, end);
			}
		}
		start = end + 1;
	}
}

void Normalizer2Impl::addPropertyStarts(const USetAdder * sa, UErrorCode & /*errorCode*/) const {
	// Add the start code point of each same-value range of the trie.
	UChar32 start = 0, end;
	uint32_t value;
	while((end = ucptrie_getRange(normTrie, start, UCPMAP_RANGE_FIXED_LEAD_SURROGATES, INERT,
	    nullptr, nullptr, &value)) >= 0) {
		sa->add(sa->set, start);
		if(start != end && isAlgorithmicNoNo((uint16_t)value) &&
		    (value & Normalizer2Impl::DELTA_TCCC_MASK) > Normalizer2Impl::DELTA_TCCC_1) {
			// Range of code points with same-norm16-value algorithmic decompositions.
			// They might have different non-zero FCD16 values.
			uint16_t prevFCD16 = getFCD16(start);
			while(++start <= end) {
				uint16_t fcd16 = getFCD16(start);
				if(fcd16 != prevFCD16) {
					sa->add(sa->set, start);
					prevFCD16 = fcd16;
				}
			}
		}
		start = end + 1;
	}

	/* add Hangul LV syllables and LV+1 because of skippables */
	for(UChar c = Hangul::HANGUL_BASE; c<Hangul::HANGUL_LIMIT; c += Hangul::JAMO_T_COUNT) {
		sa->add(sa->set, c);
		sa->add(sa->set, c+1);
	}
	sa->add(sa->set, Hangul::HANGUL_LIMIT); /* add Hangul+1 to continue with other properties */
}

void Normalizer2Impl::addCanonIterPropertyStarts(const USetAdder * sa, UErrorCode & errorCode) const {
	// Add the start code point of each same-value range of the canonical iterator data trie.
	if(!ensureCanonIterData(errorCode)) {
		return;
	}
	// Currently only used for the SEGMENT_STARTER property.
	UChar32 start = 0, end;
	uint32_t value;
	while((end = ucptrie_getRange(fCanonIterData->trie, start, UCPMAP_RANGE_NORMAL, 0,
	    segmentStarterMapper, nullptr, &value)) >= 0) {
		sa->add(sa->set, start);
		start = end + 1;
	}
}

const UChar * Normalizer2Impl::copyLowPrefixFromNulTerminated(const UChar * src,
    UChar32 minNeedDataCP,
    ReorderingBuffer * buffer,
    UErrorCode & errorCode) const {
	// Make some effort to support NUL-terminated strings reasonably.
	// Take the part of the fast quick check loop that does not look up
	// data and check the first part of the string.
	// After this prefix, determine the string length to simplify the rest
	// of the code.
	const UChar * prevSrc = src;
	UChar c;
	while((c = *src++)<minNeedDataCP && c!=0) {
	}
	// Back out the last character for full processing.
	// Copy this prefix.
	if(--src!=prevSrc) {
		if(buffer!=NULL) {
			buffer->appendZeroCC(prevSrc, src, errorCode);
		}
	}
	return src;
}

UnicodeString & Normalizer2Impl::decompose(const UnicodeString & src, UnicodeString & dest,
    UErrorCode & errorCode) const {
	if(U_FAILURE(errorCode)) {
		dest.setToBogus();
		return dest;
	}
	const UChar * sArray = src.getBuffer();
	if(&dest==&src || sArray==NULL) {
		errorCode = U_ILLEGAL_ARGUMENT_ERROR;
		dest.setToBogus();
		return dest;
	}
	decompose(sArray, sArray+src.length(), dest, src.length(), errorCode);
	return dest;
}

void Normalizer2Impl::decompose(const UChar * src, const UChar * limit,
    UnicodeString & dest,
    int32_t destLengthEstimate,
    UErrorCode & errorCode) const {
	if(destLengthEstimate<0 && limit!=NULL) {
		destLengthEstimate = (int32_t)(limit-src);
	}
	dest.remove();
	ReorderingBuffer buffer(*this, dest);
	if(buffer.init(destLengthEstimate, errorCode)) {
		decompose(src, limit, &buffer, errorCode);
	}
}

// Dual functionality:
// buffer!=NULL: normalize
// buffer==NULL: isNormalized/spanQuickCheckYes
const UChar * Normalizer2Impl::decompose(const UChar * src, const UChar * limit,
    ReorderingBuffer * buffer,
    UErrorCode & errorCode) const {
	UChar32 minNoCP = minDecompNoCP;
	if(!limit) {
		src = copyLowPrefixFromNulTerminated(src, minNoCP, buffer, errorCode);
		if(U_FAILURE(errorCode)) {
			return src;
		}
		limit = u_strchr(src, 0);
	}

	const UChar * prevSrc;
	UChar32 c = 0;
	uint16_t norm16 = 0;

	// only for quick check
	const UChar * prevBoundary = src;
	uint8 prevCC = 0;

	for(;;) {
		// count code units below the minimum or with irrelevant data for the quick check
		for(prevSrc = src; src!=limit;) {
			if((c = *src)<minNoCP ||
			    isMostDecompYesAndZeroCC(norm16 = UCPTRIE_FAST_BMP_GET(normTrie, UCPTRIE_16, c))
			    ) {
				++src;
			}
			else if(!U16_IS_LEAD(c)) {
				break;
			}
			else {
				UChar c2;
				if((src+1)!=limit && U16_IS_TRAIL(c2 = src[1])) {
					c = U16_GET_SUPPLEMENTARY(c, c2);
					norm16 = UCPTRIE_FAST_SUPP_GET(normTrie, UCPTRIE_16, c);
					if(isMostDecompYesAndZeroCC(norm16)) {
						src += 2;
					}
					else {
						break;
					}
				}
				else {
					++src; // unpaired lead surrogate: inert
				}
			}
		}
		// copy these code units all at once
		if(src!=prevSrc) {
			if(buffer!=NULL) {
				if(!buffer->appendZeroCC(prevSrc, src, errorCode)) {
					break;
				}
			}
			else {
				prevCC = 0;
				prevBoundary = src;
			}
		}
		if(src==limit) {
			break;
		}

		// Check one above-minimum, relevant code point.
		src += U16_LENGTH(c);
		if(buffer!=NULL) {
			if(!decompose(c, norm16, *buffer, errorCode)) {
				break;
			}
		}
		else {
			if(isDecompYes(norm16)) {
				uint8 cc = getCCFromYesOrMaybe(norm16);
				if(prevCC<=cc || cc==0) {
					prevCC = cc;
					if(cc<=1) {
						prevBoundary = src;
					}
					continue;
				}
			}
			return prevBoundary; // "no" or cc out of order
		}
	}
	return src;
}

// Decompose a short piece of text which is likely to contain characters that
// fail the quick check loop and/or where the quick check loop's overhead
// is unlikely to be amortized.
// Called by the compose() and makeFCD() implementations.
const UChar * Normalizer2Impl::decomposeShort(const UChar * src, const UChar * limit,
    bool stopAtCompBoundary, bool onlyContiguous,
    ReorderingBuffer &buffer, UErrorCode & errorCode) const {
	if(U_FAILURE(errorCode)) {
		return nullptr;
	}
	while(src<limit) {
		if(stopAtCompBoundary && *src < minCompNoMaybeCP) {
			return src;
		}
		const UChar * prevSrc = src;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, src, limit, c, norm16);
		if(stopAtCompBoundary && norm16HasCompBoundaryBefore(norm16)) {
			return prevSrc;
		}
		if(!decompose(c, norm16, buffer, errorCode)) {
			return nullptr;
		}
		if(stopAtCompBoundary && norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return src;
		}
	}
	return src;
}

bool Normalizer2Impl::decompose(UChar32 c, uint16_t norm16,
    ReorderingBuffer &buffer,
    UErrorCode & errorCode) const {
	// get the decomposition and the lead and trail cc's
	if(norm16 >= limitNoNo) {
		if(isMaybeOrNonZeroCC(norm16)) {
			return buffer.append(c, getCCFromYesOrMaybe(norm16), errorCode);
		}
		// Maps to an isCompYesAndZeroCC.
		c = mapAlgorithmic(c, norm16);
		norm16 = getRawNorm16(c);
	}
	if(norm16 < minYesNo) {
		// c does not decompose
		return buffer.append(c, 0, errorCode);
	}
	else if(isHangulLV(norm16) || isHangulLVT(norm16)) {
		// Hangul syllable: decompose algorithmically
		UChar jamos[3];
		return buffer.appendZeroCC(jamos, jamos+Hangul::decompose(c, jamos), errorCode);
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	int32_t length = firstUnit&MAPPING_LENGTH_MASK;
	uint8 leadCC, trailCC;
	trailCC = (uint8)(firstUnit>>8);
	if(firstUnit&MAPPING_HAS_CCC_LCCC_WORD) {
		leadCC = (uint8)(*(mapping-1)>>8);
	}
	else {
		leadCC = 0;
	}
	return buffer.append((const UChar *)mapping+1, length, TRUE, leadCC, trailCC, errorCode);
}

// Dual functionality:
// sink != nullptr: normalize
// sink == nullptr: isNormalized/spanQuickCheckYes
const uint8 * Normalizer2Impl::decomposeUTF8(uint32_t options,
    const uint8 * src, const uint8 * limit,
    ByteSink * sink, Edits * edits, UErrorCode & errorCode) const {
	U_ASSERT(limit != nullptr);
	UnicodeString s16;
	uint8 minNoLead = leadByteForCP(minDecompNoCP);

	const uint8 * prevBoundary = src;
	// only for quick check
	uint8 prevCC = 0;

	for(;;) {
		// Fast path: Scan over a sequence of characters below the minimum "no" code point,
		// or with (decompYes && ccc==0) properties.
		const uint8 * fastStart = src;
		const uint8 * prevSrc;
		uint16_t norm16 = 0;

		for(;;) {
			if(src == limit) {
				if(prevBoundary != limit && sink != nullptr) {
					ByteSinkUtil::appendUnchanged(prevBoundary, limit,
					    *sink, options, edits, errorCode);
				}
				return src;
			}
			if(*src < minNoLead) {
				++src;
			}
			else {
				prevSrc = src;
				UCPTRIE_FAST_U8_NEXT(normTrie, UCPTRIE_16, src, limit, norm16);
				if(!isMostDecompYesAndZeroCC(norm16)) {
					break;
				}
			}
		}
		// isMostDecompYesAndZeroCC(norm16) is false, that is, norm16>=minYesNo,
		// and the current character at [prevSrc..src[ is not a common case with cc=0
		// (MIN_NORMAL_MAYBE_YES or JAMO_VT).
		// It could still be a maybeYes with cc=0.
		if(prevSrc != fastStart) {
			// The fast path looped over yes/0 characters before the current one.
			if(sink != nullptr && !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
			    *sink, options, edits, errorCode)) {
				break;
			}
			prevBoundary = prevSrc;
			prevCC = 0;
		}

		// Medium-fast path: Quick check.
		if(isMaybeOrNonZeroCC(norm16)) {
			// Does not decompose.
			uint8 cc = getCCFromYesOrMaybe(norm16);
			if(prevCC <= cc || cc == 0) {
				prevCC = cc;
				if(cc <= 1) {
					if(sink != nullptr && !ByteSinkUtil::appendUnchanged(prevBoundary, src, *sink, options, edits, errorCode)) {
						break;
					}
					prevBoundary = src;
				}
				continue;
			}
		}
		if(sink == nullptr) {
			return prevBoundary; // quick check: "no" or cc out of order
		}
		// Slow path
		// Decompose up to and including the current character.
		if(prevBoundary != prevSrc && norm16HasDecompBoundaryBefore(norm16)) {
			if(!ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
			    *sink, options, edits, errorCode)) {
				break;
			}
			prevBoundary = prevSrc;
		}
		ReorderingBuffer buffer(*this, s16, errorCode);
		if(U_FAILURE(errorCode)) {
			break;
		}
		decomposeShort(prevBoundary, src, STOP_AT_LIMIT, FALSE /* onlyContiguous */,
		    buffer, errorCode);
		// Decompose until the next boundary.
		if(buffer.getLastCC() > 1) {
			src = decomposeShort(src, limit, STOP_AT_DECOMP_BOUNDARY, FALSE /* onlyContiguous */,
				buffer, errorCode);
		}
		if(U_FAILURE(errorCode)) {
			break;
		}
		if((src - prevSrc) > INT32_MAX) { // guard before buffer.equals()
			errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
			break;
		}
		// We already know there was a change if the original character decomposed;
		// otherwise compare.
		if(isMaybeOrNonZeroCC(norm16) && buffer.equals(prevBoundary, src)) {
			if(!ByteSinkUtil::appendUnchanged(prevBoundary, src,
			    *sink, options, edits, errorCode)) {
				break;
			}
		}
		else {
			if(!ByteSinkUtil::appendChange(prevBoundary, src, buffer.getStart(), buffer.length(),
			    *sink, edits, errorCode)) {
				break;
			}
		}
		prevBoundary = src;
		prevCC = 0;
	}
	return src;
}

const uint8 * Normalizer2Impl::decomposeShort(const uint8 * src, const uint8 * limit,
    StopAt stopAt, bool onlyContiguous,
    ReorderingBuffer &buffer, UErrorCode & errorCode) const {
	if(U_FAILURE(errorCode)) {
		return nullptr;
	}
	while(src < limit) {
		const uint8 * prevSrc = src;
		uint16_t norm16;
		UCPTRIE_FAST_U8_NEXT(normTrie, UCPTRIE_16, src, limit, norm16);
		// Get the decomposition and the lead and trail cc's.
		UChar32 c = U_SENTINEL;
		if(norm16 >= limitNoNo) {
			if(isMaybeOrNonZeroCC(norm16)) {
				// No comp boundaries around this character.
				uint8 cc = getCCFromYesOrMaybe(norm16);
				if(cc == 0 && stopAt == STOP_AT_DECOMP_BOUNDARY) {
					return prevSrc;
				}
				c = codePointFromValidUTF8(prevSrc, src);
				if(!buffer.append(c, cc, errorCode)) {
					return nullptr;
				}
				if(stopAt == STOP_AT_DECOMP_BOUNDARY && buffer.getLastCC() <= 1) {
					return src;
				}
				continue;
			}
			// Maps to an isCompYesAndZeroCC.
			if(stopAt != STOP_AT_LIMIT) {
				return prevSrc;
			}
			c = codePointFromValidUTF8(prevSrc, src);
			c = mapAlgorithmic(c, norm16);
			norm16 = getRawNorm16(c);
		}
		else if(stopAt != STOP_AT_LIMIT && norm16 < minNoNoCompNoMaybeCC) {
			return prevSrc;
		}
		// norm16!=INERT guarantees that [prevSrc, src[ is valid UTF-8.
		// We do not see invalid UTF-8 here because
		// its norm16==INERT is normalization-inert,
		// so it gets copied unchanged in the fast path,
		// and we stop the slow path where invalid UTF-8 begins.
		// c >= 0 is the result of an algorithmic mapping.
		U_ASSERT(c >= 0 || norm16 != INERT);
		if(norm16 < minYesNo) {
			if(c < 0) {
				c = codePointFromValidUTF8(prevSrc, src);
			}
			// does not decompose
			if(!buffer.append(c, 0, errorCode)) {
				return nullptr;
			}
		}
		else if(isHangulLV(norm16) || isHangulLVT(norm16)) {
			// Hangul syllable: decompose algorithmically
			if(c < 0) {
				c = codePointFromValidUTF8(prevSrc, src);
			}
			char16_t jamos[3];
			if(!buffer.appendZeroCC(jamos, jamos+Hangul::decompose(c, jamos), errorCode)) {
				return nullptr;
			}
		}
		else {
			// The character decomposes, get everything from the variable-length extra data.
			const uint16_t * mapping = getMapping(norm16);
			uint16_t firstUnit = *mapping;
			int32_t length = firstUnit & MAPPING_LENGTH_MASK;
			uint8 trailCC = (uint8)(firstUnit >> 8);
			uint8 leadCC;
			if(firstUnit & MAPPING_HAS_CCC_LCCC_WORD) {
				leadCC = (uint8)(*(mapping-1) >> 8);
			}
			else {
				leadCC = 0;
			}
			if(leadCC == 0 && stopAt == STOP_AT_DECOMP_BOUNDARY) {
				return prevSrc;
			}
			if(!buffer.append((const char16_t*)mapping+1, length, TRUE, leadCC, trailCC, errorCode)) {
				return nullptr;
			}
		}
		if((stopAt == STOP_AT_COMP_BOUNDARY && norm16HasCompBoundaryAfter(norm16, onlyContiguous)) ||
		    (stopAt == STOP_AT_DECOMP_BOUNDARY && buffer.getLastCC() <= 1)) {
			return src;
		}
	}
	return src;
}

const UChar * Normalizer2Impl::getDecomposition(UChar32 c, UChar buffer[4], int32_t &length) const {
	uint16_t norm16;
	if(c<minDecompNoCP || isMaybeOrNonZeroCC(norm16 = getNorm16(c))) {
		// c does not decompose
		return nullptr;
	}
	const UChar * decomp = nullptr;
	if(isDecompNoAlgorithmic(norm16)) {
		// Maps to an isCompYesAndZeroCC.
		c = mapAlgorithmic(c, norm16);
		decomp = buffer;
		length = 0;
		U16_APPEND_UNSAFE(buffer, length, c);
		// The mapping might decompose further.
		norm16 = getRawNorm16(c);
	}
	if(norm16 < minYesNo) {
		return decomp;
	}
	else if(isHangulLV(norm16) || isHangulLVT(norm16)) {
		// Hangul syllable: decompose algorithmically
		length = Hangul::decompose(c, buffer);
		return buffer;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	length = *mapping&MAPPING_LENGTH_MASK;
	return (const UChar *)mapping+1;
}

// The capacity of the buffer must be 30=MAPPING_LENGTH_MASK-1
// so that a raw mapping fits that consists of one unit ("rm0")
// plus all but the first two code units of the normal mapping.
// The maximum length of a normal mapping is 31=MAPPING_LENGTH_MASK.
const UChar * Normalizer2Impl::getRawDecomposition(UChar32 c, UChar buffer[30], int32_t &length) const {
	uint16_t norm16;
	if(c<minDecompNoCP || isDecompYes(norm16 = getNorm16(c))) {
		// c does not decompose
		return NULL;
	}
	else if(isHangulLV(norm16) || isHangulLVT(norm16)) {
		// Hangul syllable: decompose algorithmically
		Hangul::getRawDecomposition(c, buffer);
		length = 2;
		return buffer;
	}
	else if(isDecompNoAlgorithmic(norm16)) {
		c = mapAlgorithmic(c, norm16);
		length = 0;
		U16_APPEND_UNSAFE(buffer, length, c);
		return buffer;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	int32_t mLength = firstUnit&MAPPING_LENGTH_MASK; // length of normal mapping
	if(firstUnit&MAPPING_HAS_RAW_MAPPING) {
		// Read the raw mapping from before the firstUnit and before the optional ccc/lccc word.
		// Bit 7=MAPPING_HAS_CCC_LCCC_WORD
		const uint16_t * rawMapping = mapping-((firstUnit>>7)&1)-1;
		uint16_t rm0 = *rawMapping;
		if(rm0<=MAPPING_LENGTH_MASK) {
			length = rm0;
			return (const UChar *)rawMapping-rm0;
		}
		else {
			// Copy the normal mapping and replace its first two code units with rm0.
			buffer[0] = (UChar)rm0;
			u_memcpy(buffer+1, (const UChar *)mapping+1+2, mLength-2);
			length = mLength-1;
			return buffer;
		}
	}
	else {
		length = mLength;
		return (const UChar *)mapping+1;
	}
}

void Normalizer2Impl::decomposeAndAppend(const UChar * src, const UChar * limit,
    bool doDecompose,
    UnicodeString & safeMiddle,
    ReorderingBuffer &buffer,
    UErrorCode & errorCode) const {
	buffer.copyReorderableSuffixTo(safeMiddle);
	if(doDecompose) {
		decompose(src, limit, &buffer, errorCode);
		return;
	}
	// Just merge the strings at the boundary.
	bool isFirst = true;
	uint8 firstCC = 0, prevCC = 0, cc;
	const UChar * p = src;
	while(p != limit) {
		const UChar * codePointStart = p;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, p, limit, c, norm16);
		if((cc = getCC(norm16)) == 0) {
			p = codePointStart;
			break;
		}
		if(isFirst) {
			firstCC = cc;
			isFirst = false;
		}
		prevCC = cc;
	}
	if(!limit) { // appendZeroCC() needs limit!=NULL
		limit = u_strchr(p, 0);
	}

	if(buffer.append(src, (int32_t)(p - src), FALSE, firstCC, prevCC, errorCode)) {
		buffer.appendZeroCC(p, limit, errorCode);
	}
}

bool Normalizer2Impl::hasDecompBoundaryBefore(UChar32 c) const {
	return c < minLcccCP || (c <= 0xffff && !singleLeadMightHaveNonZeroFCD16(c)) ||
	       norm16HasDecompBoundaryBefore(getNorm16(c));
}

bool Normalizer2Impl::norm16HasDecompBoundaryBefore(uint16_t norm16) const {
	if(norm16 < minNoNoCompNoMaybeCC) {
		return TRUE;
	}
	if(norm16 >= limitNoNo) {
		return norm16 <= MIN_NORMAL_MAYBE_YES || norm16 == JAMO_VT;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	// TRUE if leadCC==0 (hasFCDBoundaryBefore())
	return (firstUnit&MAPPING_HAS_CCC_LCCC_WORD)==0 || (*(mapping-1)&0xff00)==0;
}

bool Normalizer2Impl::hasDecompBoundaryAfter(UChar32 c) const {
	if(c < minDecompNoCP) {
		return TRUE;
	}
	if(c <= 0xffff && !singleLeadMightHaveNonZeroFCD16(c)) {
		return TRUE;
	}
	return norm16HasDecompBoundaryAfter(getNorm16(c));
}

bool Normalizer2Impl::norm16HasDecompBoundaryAfter(uint16_t norm16) const {
	if(norm16 <= minYesNo || isHangulLVT(norm16)) {
		return TRUE;
	}
	if(norm16 >= limitNoNo) {
		if(isMaybeOrNonZeroCC(norm16)) {
			return norm16 <= MIN_NORMAL_MAYBE_YES || norm16 == JAMO_VT;
		}
		// Maps to an isCompYesAndZeroCC.
		return (norm16 & DELTA_TCCC_MASK) <= DELTA_TCCC_1;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	// decomp after-boundary: same as hasFCDBoundaryAfter(),
	// fcd16<=1 || trailCC==0
	if(firstUnit>0x1ff) {
		return FALSE; // trailCC>1
	}
	if(firstUnit<=0xff) {
		return TRUE; // trailCC==0
	}
	// if(trailCC==1) test leadCC==0, same as checking for before-boundary
	// TRUE if leadCC==0 (hasFCDBoundaryBefore())
	return (firstUnit&MAPPING_HAS_CCC_LCCC_WORD)==0 || (*(mapping-1)&0xff00)==0;
}

/*
 * Finds the recomposition result for
 * a forward-combining "lead" character,
 * specified with a pointer to its compositions list,
 * and a backward-combining "trail" character.
 *
 * If the lead and trail characters combine, then this function returns
 * the following "compositeAndFwd" value:
 * Bits 21..1  composite character
 * Bit      0  set if the composite is a forward-combining starter
 * otherwise it returns -1.
 *
 * The compositions list has (trail, compositeAndFwd) pair entries,
 * encoded as either pairs or triples of 16-bit units.
 * The last entry has the high bit of its first unit set.
 *
 * The list is sorted by ascending trail characters (there are no duplicates).
 * A linear search is used.
 *
 * See normalizer2impl.h for a more detailed description
 * of the compositions list format.
 */
int32_t Normalizer2Impl::combine(const uint16_t * list, UChar32 trail) 
{
	uint16_t key1, firstUnit;
	if(trail<COMP_1_TRAIL_LIMIT) {
		// trail character is 0..33FF
		// result entry may have 2 or 3 units
		key1 = (uint16_t)(trail<<1);
		while(key1>(firstUnit = *list)) {
			list += 2+(firstUnit&COMP_1_TRIPLE);
		}
		if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
			if(firstUnit&COMP_1_TRIPLE) {
				return ((int32_t)list[1]<<16)|list[2];
			}
			else {
				return list[1];
			}
		}
	}
	else {
		// trail character is 3400..10FFFF
		// result entry has 3 units
		key1 = (uint16_t)(COMP_1_TRAIL_LIMIT+
		    (((trail>>COMP_1_TRAIL_SHIFT))&
		    ~COMP_1_TRIPLE));
		uint16_t key2 = (uint16_t)(trail<<COMP_2_TRAIL_SHIFT);
		uint16_t secondUnit;
		for(;;) {
			if(key1>(firstUnit = *list)) {
				list += 2+(firstUnit&COMP_1_TRIPLE);
			}
			else if(key1==(firstUnit&COMP_1_TRAIL_MASK)) {
				if(key2>(secondUnit = list[1])) {
					if(firstUnit&COMP_1_LAST_TUPLE) {
						break;
					}
					else {
						list += 3;
					}
				}
				else if(key2==(secondUnit&COMP_2_TRAIL_MASK)) {
					return ((int32_t)(secondUnit&~COMP_2_TRAIL_MASK)<<16)|list[2];
				}
				else {
					break;
				}
			}
			else {
				break;
			}
		}
	}
	return -1;
}
/**
 * @param list some character's compositions list
 * @param set recursively receives the composites from these compositions
 */
void Normalizer2Impl::addComposites(const uint16_t * list, UnicodeSet & set) const 
{
	uint16_t firstUnit;
	int32_t compositeAndFwd;
	do {
		firstUnit = *list;
		if((firstUnit&COMP_1_TRIPLE)==0) {
			compositeAndFwd = list[1];
			list += 2;
		}
		else {
			compositeAndFwd = (((int32_t)list[1]&~COMP_2_TRAIL_MASK)<<16)|list[2];
			list += 3;
		}
		UChar32 composite = compositeAndFwd>>1;
		if((compositeAndFwd&1)!=0) {
			addComposites(getCompositionsListForComposite(getRawNorm16(composite)), set);
		}
		set.add(composite);
	} while((firstUnit&COMP_1_LAST_TUPLE)==0);
}

/*
 * Recomposes the buffer text starting at recomposeStartIndex
 * (which is in NFD - decomposed and canonically ordered),
 * and truncates the buffer contents.
 *
 * Note that recomposition never lengthens the text:
 * Any character consists of either one or two code units;
 * a composition may contain at most one more code unit than the original starter,
 * while the combining mark that is removed has at least one code unit.
 */
void Normalizer2Impl::recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex,
    bool onlyContiguous) const {
	UChar * p = buffer.getStart()+recomposeStartIndex;
	UChar * limit = buffer.getLimit();
	if(p==limit) {
		return;
	}

	UChar * starter, * pRemove, * q, * r;
	const uint16_t * compositionsList;
	UChar32 c, compositeAndFwd;
	uint16_t norm16;
	uint8 cc, prevCC;
	bool starterIsSupplementary;

	// Some of the following variables are not used until we have a forward-combining starter
	// and are only initialized now to avoid compiler warnings.
	compositionsList = NULL; // used as indicator for whether we have a forward-combining starter
	starter = NULL;
	starterIsSupplementary = FALSE;
	prevCC = 0;

	for(;;) {
		UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, p, limit, c, norm16);
		cc = getCCFromYesOrMaybe(norm16);
		if( // this character combines backward and
			isMaybe(norm16) &&
			// we have seen a starter that combines forward and
			compositionsList!=NULL &&
			// the backward-combining character is not blocked
			(prevCC<cc || prevCC==0)
			) {
			if(isJamoVT(norm16)) {
				// c is a Jamo V/T, see if we can compose it with the previous character.
				if(c<Hangul::JAMO_T_BASE) {
					// c is a Jamo Vowel, compose with previous Jamo L and following Jamo T.
					UChar prev = (UChar)(*starter-Hangul::JAMO_L_BASE);
					if(prev<Hangul::JAMO_L_COUNT) {
						pRemove = p-1;
						UChar syllable = (UChar)
						    (Hangul::HANGUL_BASE+
						    (prev*Hangul::JAMO_V_COUNT+(c-Hangul::JAMO_V_BASE))*
						    Hangul::JAMO_T_COUNT);
						UChar t;
						if(p!=limit && (t = (UChar)(*p-Hangul::JAMO_T_BASE))<Hangul::JAMO_T_COUNT) {
							++p;
							syllable += t; // The next character was a Jamo T.
						}
						*starter = syllable;
						// remove the Jamo V/T
						q = pRemove;
						r = p;
						while(r<limit) {
							*q++ = *r++;
						}
						limit = q;
						p = pRemove;
					}
				}
				/*
				 * No "else" for Jamo T:
				 * Since the input is in NFD, there are no Hangul LV syllables that
				 * a Jamo T could combine with.
				 * All Jamo Ts are combined above when handling Jamo Vs.
				 */
				if(p==limit) {
					break;
				}
				compositionsList = NULL;
				continue;
			}
			else if((compositeAndFwd = combine(compositionsList, c))>=0) {
				// The starter and the combining mark (c) do combine.
				UChar32 composite = compositeAndFwd>>1;

				// Replace the starter with the composite, remove the combining mark.
				pRemove = p-U16_LENGTH(c); // pRemove & p: start & limit of the combining mark
				if(starterIsSupplementary) {
					if(U_IS_SUPPLEMENTARY(composite)) {
						// both are supplementary
						starter[0] = U16_LEAD(composite);
						starter[1] = U16_TRAIL(composite);
					}
					else {
						*starter = (UChar)composite;
						// The composite is shorter than the starter,
						// move the intermediate characters forward one.
						starterIsSupplementary = FALSE;
						q = starter+1;
						r = q+1;
						while(r<pRemove) {
							*q++ = *r++;
						}
						--pRemove;
					}
				}
				else if(U_IS_SUPPLEMENTARY(composite)) {
					// The composite is longer than the starter,
					// move the intermediate characters back one.
					starterIsSupplementary = TRUE;
					++starter; // temporarily increment for the loop boundary
					q = pRemove;
					r = ++pRemove;
					while(starter<q) {
						*--r = *--q;
					}
					*starter = U16_TRAIL(composite);
					*--starter = U16_LEAD(composite); // undo the temporary increment
				}
				else {
					// both are on the BMP
					*starter = (UChar)composite;
				}

				/* remove the combining mark by moving the following text over it */
				if(pRemove<p) {
					q = pRemove;
					r = p;
					while(r<limit) {
						*q++ = *r++;
					}
					limit = q;
					p = pRemove;
				}
				// Keep prevCC because we removed the combining mark.

				if(p==limit) {
					break;
				}
				// Is the composite a starter that combines forward?
				if(compositeAndFwd&1) {
					compositionsList =
					    getCompositionsListForComposite(getRawNorm16(composite));
				}
				else {
					compositionsList = NULL;
				}

				// We combined; continue with looking for compositions.
				continue;
			}
		}

		// no combination this time
		prevCC = cc;
		if(p==limit) {
			break;
		}

		// If c did not combine, then check if it is a starter.
		if(cc==0) {
			// Found a new starter.
			if((compositionsList = getCompositionsListForDecompYes(norm16))!=NULL) {
				// It may combine with something, prepare for it.
				if(U_IS_BMP(c)) {
					starterIsSupplementary = FALSE;
					starter = p-1;
				}
				else {
					starterIsSupplementary = TRUE;
					starter = p-2;
				}
			}
		}
		else if(onlyContiguous) {
			// FCC: no discontiguous compositions; any intervening character blocks.
			compositionsList = NULL;
		}
	}
	buffer.setReorderingLimit(limit);
}

UChar32 Normalizer2Impl::composePair(UChar32 a, UChar32 b) const {
	uint16_t norm16 = getNorm16(a); // maps an out-of-range 'a' to inert norm16
	const uint16_t * list;
	if(isInert(norm16)) {
		return U_SENTINEL;
	}
	else if(norm16<minYesNoMappingsOnly) {
		// a combines forward.
		if(isJamoL(norm16)) {
			b -= Hangul::JAMO_V_BASE;
			if(0<=b && b<Hangul::JAMO_V_COUNT) {
				return
					(Hangul::HANGUL_BASE+
					((a-Hangul::JAMO_L_BASE)*Hangul::JAMO_V_COUNT+b)*
					Hangul::JAMO_T_COUNT);
			}
			else {
				return U_SENTINEL;
			}
		}
		else if(isHangulLV(norm16)) {
			b -= Hangul::JAMO_T_BASE;
			if(0<b && b<Hangul::JAMO_T_COUNT) { // not b==0!
				return a+b;
			}
			else {
				return U_SENTINEL;
			}
		}
		else {
			// 'a' has a compositions list in extraData
			list = getMapping(norm16);
			if(norm16>minYesNo) { // composite 'a' has both mapping & compositions list
				list += // mapping pointer
				    1+ // +1 to skip the first unit with the mapping length
				    (*list&MAPPING_LENGTH_MASK); // + mapping length
			}
		}
	}
	else if(norm16<minMaybeYes || MIN_NORMAL_MAYBE_YES<=norm16) {
		return U_SENTINEL;
	}
	else {
		list = getCompositionsListForMaybe(norm16);
	}
	if(b<0 || 0x10ffff<b) { // combine(list, b) requires a valid code point b
		return U_SENTINEL;
	}
#if U_SIGNED_RIGHT_SHIFT_IS_ARITHMETIC
	return combine(list, b)>>1;
#else
	int32_t compositeAndFwd = combine(list, b);
	return compositeAndFwd>=0 ? compositeAndFwd>>1 : U_SENTINEL;
#endif
}

// Very similar to composeQuickCheck(): Make the same changes in both places if relevant.
// doCompose: normalize
// !doCompose: isNormalized (buffer must be empty and initialized)
bool Normalizer2Impl::compose(const UChar * src, const UChar * limit,
    bool onlyContiguous,
    bool doCompose,
    ReorderingBuffer &buffer,
    UErrorCode & errorCode) const {
	const UChar * prevBoundary = src;
	UChar32 minNoMaybeCP = minCompNoMaybeCP;
	if(!limit) {
		src = copyLowPrefixFromNulTerminated(src, minNoMaybeCP,
			doCompose ? &buffer : NULL,
			errorCode);
		if(U_FAILURE(errorCode)) {
			return FALSE;
		}
		limit = u_strchr(src, 0);
		if(prevBoundary != src) {
			if(hasCompBoundaryAfter(*(src-1), onlyContiguous)) {
				prevBoundary = src;
			}
			else {
				buffer.removeSuffix(1);
				prevBoundary = --src;
			}
		}
	}

	for(;;) {
		// Fast path: Scan over a sequence of characters below the minimum "no or maybe" code point,
		// or with (compYes && ccc==0) properties.
		const UChar * prevSrc;
		UChar32 c = 0;
		uint16_t norm16 = 0;
		for(;;) {
			if(src == limit) {
				if(prevBoundary != limit && doCompose) {
					buffer.appendZeroCC(prevBoundary, limit, errorCode);
				}
				return TRUE;
			}
			if((c = *src)<minNoMaybeCP ||
			    isCompYesAndZeroCC(norm16 = UCPTRIE_FAST_BMP_GET(normTrie, UCPTRIE_16, c))
			    ) {
				++src;
			}
			else {
				prevSrc = src++;
				if(!U16_IS_LEAD(c)) {
					break;
				}
				else {
					UChar c2;
					if(src!=limit && U16_IS_TRAIL(c2 = *src)) {
						++src;
						c = U16_GET_SUPPLEMENTARY(c, c2);
						norm16 = UCPTRIE_FAST_SUPP_GET(normTrie, UCPTRIE_16, c);
						if(!isCompYesAndZeroCC(norm16)) {
							break;
						}
					}
				}
			}
		}
		// isCompYesAndZeroCC(norm16) is false, that is, norm16>=minNoNo.
		// The current character is either a "noNo" (has a mapping)
		// or a "maybeYes" (combines backward)
		// or a "yesYes" with ccc!=0.
		// It is not a Hangul syllable or Jamo L because those have "yes" properties.

		// Medium-fast path: Handle cases that do not require full decomposition and recomposition.
		if(!isMaybeOrNonZeroCC(norm16)) { // minNoNo <= norm16 < minMaybeYes
			if(!doCompose) {
				return FALSE;
			}
			// Fast path for mapping a character that is immediately surrounded by boundaries.
			// In this case, we need not decompose around the current character.
			if(isDecompNoAlgorithmic(norm16)) {
				// Maps to a single isCompYesAndZeroCC character
				// which also implies hasCompBoundaryBefore.
				if(norm16HasCompBoundaryAfter(norm16, onlyContiguous) ||
				    hasCompBoundaryBefore(src, limit)) {
					if(prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
						break;
					}
					if(!buffer.append(mapAlgorithmic(c, norm16), 0, errorCode)) {
						break;
					}
					prevBoundary = src;
					continue;
				}
			}
			else if(norm16 < minNoNoCompBoundaryBefore) {
				// The mapping is comp-normalized which also implies hasCompBoundaryBefore.
				if(norm16HasCompBoundaryAfter(norm16, onlyContiguous) ||
				    hasCompBoundaryBefore(src, limit)) {
					if(prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
						break;
					}
					const UChar * mapping = reinterpret_cast<const UChar *>(getMapping(norm16));
					int32_t length = *mapping++ & MAPPING_LENGTH_MASK;
					if(!buffer.appendZeroCC(mapping, mapping + length, errorCode)) {
						break;
					}
					prevBoundary = src;
					continue;
				}
			}
			else if(norm16 >= minNoNoEmpty) {
				// The current character maps to nothing.
				// Simply omit it from the output if there is a boundary before _or_ after it.
				// The character itself implies no boundaries.
				if(hasCompBoundaryBefore(src, limit) ||
				    hasCompBoundaryAfter(prevBoundary, prevSrc, onlyContiguous)) {
					if(prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
						break;
					}
					prevBoundary = src;
					continue;
				}
			}
			// Other "noNo" type, or need to examine more text around this character:
			// Fall through to the slow path.
		}
		else if(isJamoVT(norm16) && prevBoundary != prevSrc) {
			UChar prev = *(prevSrc-1);
			if(c<Hangul::JAMO_T_BASE) {
				// The current character is a Jamo Vowel,
				// compose with previous Jamo L and following Jamo T.
				UChar l = (UChar)(prev-Hangul::JAMO_L_BASE);
				if(l<Hangul::JAMO_L_COUNT) {
					if(!doCompose) {
						return FALSE;
					}
					int32_t t;
					if(src != limit &&
					    0 < (t = ((int32_t)*src - Hangul::JAMO_T_BASE)) &&
					    t < Hangul::JAMO_T_COUNT) {
						// The next character is a Jamo T.
						++src;
					}
					else if(hasCompBoundaryBefore(src, limit)) {
						// No Jamo T follows, not even via decomposition.
						t = 0;
					}
					else {
						t = -1;
					}
					if(t >= 0) {
						UChar32 syllable = Hangul::HANGUL_BASE +
						    (l*Hangul::JAMO_V_COUNT + (c-Hangul::JAMO_V_BASE)) *
						    Hangul::JAMO_T_COUNT + t;
						--prevSrc; // Replace the Jamo L as well.
						if(prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
							break;
						}
						if(!buffer.appendBMP((UChar)syllable, 0, errorCode)) {
							break;
						}
						prevBoundary = src;
						continue;
					}
					// If we see L+V+x where x!=T then we drop to the slow path,
					// decompose and recompose.
					// This is to deal with NFKC finding normal L and V but a
					// compatibility variant of a T.
					// We need to either fully compose that combination here
					// (which would complicate the code and may not work with strange custom data)
					// or use the slow path.
				}
			}
			else if(Hangul::isHangulLV(prev)) {
				// The current character is a Jamo Trailing consonant,
				// compose with previous Hangul LV that does not contain a Jamo T.
				if(!doCompose) {
					return FALSE;
				}
				UChar32 syllable = prev + c - Hangul::JAMO_T_BASE;
				--prevSrc; // Replace the Hangul LV as well.
				if(prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
					break;
				}
				if(!buffer.appendBMP((UChar)syllable, 0, errorCode)) {
					break;
				}
				prevBoundary = src;
				continue;
			}
			// No matching context, or may need to decompose surrounding text first:
			// Fall through to the slow path.
		}
		else if(norm16 > JAMO_VT) { // norm16 >= MIN_YES_YES_WITH_CC
			// One or more combining marks that do not combine-back:
			// Check for canonical order, copy unchanged if ok and
			// if followed by a character with a boundary-before.
			uint8 cc = getCCFromNormalYesOrMaybe(norm16); // cc!=0
			if(onlyContiguous /* FCC */ && getPreviousTrailCC(prevBoundary, prevSrc) > cc) {
				// Fails FCD test, need to decompose and contiguously recompose.
				if(!doCompose) {
					return FALSE;
				}
			}
			else {
				// If !onlyContiguous (not FCC), then we ignore the tccc of
				// the previous character which passed the quick check "yes && ccc==0" test.
				const UChar * nextSrc;
				uint16_t n16;
				for(;;) {
					if(src == limit) {
						if(doCompose) {
							buffer.appendZeroCC(prevBoundary, limit, errorCode);
						}
						return TRUE;
					}
					uint8 prevCC = cc;
					nextSrc = src;
					UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, nextSrc, limit, c, n16);
					if(n16 >= MIN_YES_YES_WITH_CC) {
						cc = getCCFromNormalYesOrMaybe(n16);
						if(prevCC > cc) {
							if(!doCompose) {
								return FALSE;
							}
							break;
						}
					}
					else {
						break;
					}
					src = nextSrc;
				}
				// src is after the last in-order combining mark.
				// If there is a boundary here, then we continue with no change.
				if(norm16HasCompBoundaryBefore(n16)) {
					if(isCompYesAndZeroCC(n16)) {
						src = nextSrc;
					}
					continue;
				}
				// Use the slow path. There is no boundary in [prevSrc, src[.
			}
		}

		// Slow path: Find the nearest boundaries around the current character,
		// decompose and recompose.
		if(prevBoundary != prevSrc && !norm16HasCompBoundaryBefore(norm16)) {
			const UChar * p = prevSrc;
			UCPTRIE_FAST_U16_PREV(normTrie, UCPTRIE_16, prevBoundary, p, c, norm16);
			if(!norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
				prevSrc = p;
			}
		}
		if(doCompose && prevBoundary != prevSrc && !buffer.appendZeroCC(prevBoundary, prevSrc, errorCode)) {
			break;
		}
		int32_t recomposeStartIndex = buffer.length();
		// We know there is not a boundary here.
		decomposeShort(prevSrc, src, FALSE /* !stopAtCompBoundary */, onlyContiguous,
		    buffer, errorCode);
		// Decompose until the next boundary.
		src = decomposeShort(src, limit, TRUE /* stopAtCompBoundary */, onlyContiguous,
			buffer, errorCode);
		if(U_FAILURE(errorCode)) {
			break;
		}
		if((src - prevSrc) > INT32_MAX) { // guard before buffer.equals()
			errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
			return TRUE;
		}
		recompose(buffer, recomposeStartIndex, onlyContiguous);
		if(!doCompose) {
			if(!buffer.equals(prevSrc, src)) {
				return FALSE;
			}
			buffer.remove();
		}
		prevBoundary = src;
	}
	return TRUE;
}

// Very similar to compose(): Make the same changes in both places if relevant.
// pQCResult==NULL: spanQuickCheckYes
// pQCResult!=NULL: quickCheck (*pQCResult must be UNORM_YES)
const UChar * Normalizer2Impl::composeQuickCheck(const UChar * src, const UChar * limit,
    bool onlyContiguous,
    UNormalizationCheckResult * pQCResult) const {
	const UChar * prevBoundary = src;
	UChar32 minNoMaybeCP = minCompNoMaybeCP;
	if(!limit) {
		UErrorCode errorCode = U_ZERO_ERROR;
		src = copyLowPrefixFromNulTerminated(src, minNoMaybeCP, NULL, errorCode);
		limit = u_strchr(src, 0);
		if(prevBoundary != src) {
			if(hasCompBoundaryAfter(*(src-1), onlyContiguous)) {
				prevBoundary = src;
			}
			else {
				prevBoundary = --src;
			}
		}
	}

	for(;;) {
		// Fast path: Scan over a sequence of characters below the minimum "no or maybe" code point,
		// or with (compYes && ccc==0) properties.
		const UChar * prevSrc;
		UChar32 c = 0;
		uint16_t norm16 = 0;
		for(;;) {
			if(src==limit) {
				return src;
			}
			if((c = *src)<minNoMaybeCP ||
			    isCompYesAndZeroCC(norm16 = UCPTRIE_FAST_BMP_GET(normTrie, UCPTRIE_16, c))
			    ) {
				++src;
			}
			else {
				prevSrc = src++;
				if(!U16_IS_LEAD(c)) {
					break;
				}
				else {
					UChar c2;
					if(src!=limit && U16_IS_TRAIL(c2 = *src)) {
						++src;
						c = U16_GET_SUPPLEMENTARY(c, c2);
						norm16 = UCPTRIE_FAST_SUPP_GET(normTrie, UCPTRIE_16, c);
						if(!isCompYesAndZeroCC(norm16)) {
							break;
						}
					}
				}
			}
		}
		// isCompYesAndZeroCC(norm16) is false, that is, norm16>=minNoNo.
		// The current character is either a "noNo" (has a mapping)
		// or a "maybeYes" (combines backward)
		// or a "yesYes" with ccc!=0.
		// It is not a Hangul syllable or Jamo L because those have "yes" properties.

		uint16_t prevNorm16 = INERT;
		if(prevBoundary != prevSrc) {
			if(norm16HasCompBoundaryBefore(norm16)) {
				prevBoundary = prevSrc;
			}
			else {
				const UChar * p = prevSrc;
				uint16_t n16;
				UCPTRIE_FAST_U16_PREV(normTrie, UCPTRIE_16, prevBoundary, p, c, n16);
				if(norm16HasCompBoundaryAfter(n16, onlyContiguous)) {
					prevBoundary = prevSrc;
				}
				else {
					prevBoundary = p;
					prevNorm16 = n16;
				}
			}
		}

		if(isMaybeOrNonZeroCC(norm16)) {
			uint8 cc = getCCFromYesOrMaybe(norm16);
			if(onlyContiguous /* FCC */ && cc != 0 &&
			    getTrailCCFromCompYesAndZeroCC(prevNorm16) > cc) {
				// The [prevBoundary..prevSrc[ character
				// passed the quick check "yes && ccc==0" test
				// but is out of canonical order with the current combining mark.
			}
			else {
				// If !onlyContiguous (not FCC), then we ignore the tccc of
				// the previous character which passed the quick check "yes && ccc==0" test.
				const UChar * nextSrc;
				for(;;) {
					if(norm16 < MIN_YES_YES_WITH_CC) {
						if(pQCResult != nullptr) {
							*pQCResult = UNORM_MAYBE;
						}
						else {
							return prevBoundary;
						}
					}
					if(src == limit) {
						return src;
					}
					uint8 prevCC = cc;
					nextSrc = src;
					UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, nextSrc, limit, c, norm16);
					if(isMaybeOrNonZeroCC(norm16)) {
						cc = getCCFromYesOrMaybe(norm16);
						if(!(prevCC <= cc || cc == 0)) {
							break;
						}
					}
					else {
						break;
					}
					src = nextSrc;
				}
				// src is after the last in-order combining mark.
				if(isCompYesAndZeroCC(norm16)) {
					prevBoundary = src;
					src = nextSrc;
					continue;
				}
			}
		}
		if(pQCResult!=NULL) {
			*pQCResult = UNORM_NO;
		}
		return prevBoundary;
	}
}

void Normalizer2Impl::composeAndAppend(const UChar * src, const UChar * limit,
    bool doCompose,
    bool onlyContiguous,
    UnicodeString & safeMiddle,
    ReorderingBuffer &buffer,
    UErrorCode & errorCode) const {
	if(!buffer.isEmpty()) {
		const UChar * firstStarterInSrc = findNextCompBoundary(src, limit, onlyContiguous);
		if(src!=firstStarterInSrc) {
			const UChar * lastStarterInDest = findPreviousCompBoundary(buffer.getStart(),
				buffer.getLimit(), onlyContiguous);
			int32_t destSuffixLength = (int32_t)(buffer.getLimit()-lastStarterInDest);
			UnicodeString middle(lastStarterInDest, destSuffixLength);
			buffer.removeSuffix(destSuffixLength);
			safeMiddle = middle;
			middle.append(src, (int32_t)(firstStarterInSrc-src));
			const UChar * middleStart = middle.getBuffer();
			compose(middleStart, middleStart+middle.length(), onlyContiguous,
			    TRUE, buffer, errorCode);
			if(U_FAILURE(errorCode)) {
				return;
			}
			src = firstStarterInSrc;
		}
	}
	if(doCompose) {
		compose(src, limit, onlyContiguous, TRUE, buffer, errorCode);
	}
	else {
		if(!limit) { // appendZeroCC() needs limit!=NULL
			limit = u_strchr(src, 0);
		}
		buffer.appendZeroCC(src, limit, errorCode);
	}
}

bool Normalizer2Impl::composeUTF8(uint32_t options, bool onlyContiguous,
    const uint8 * src, const uint8 * limit,
    ByteSink * sink, Edits * edits, UErrorCode & errorCode) const {
	U_ASSERT(limit != nullptr);
	UnicodeString s16;
	uint8 minNoMaybeLead = leadByteForCP(minCompNoMaybeCP);
	const uint8 * prevBoundary = src;

	for(;;) {
		// Fast path: Scan over a sequence of characters below the minimum "no or maybe" code point,
		// or with (compYes && ccc==0) properties.
		const uint8 * prevSrc;
		uint16_t norm16 = 0;
		for(;;) {
			if(src == limit) {
				if(prevBoundary != limit && sink != nullptr) {
					ByteSinkUtil::appendUnchanged(prevBoundary, limit,
					    *sink, options, edits, errorCode);
				}
				return TRUE;
			}
			if(*src < minNoMaybeLead) {
				++src;
			}
			else {
				prevSrc = src;
				UCPTRIE_FAST_U8_NEXT(normTrie, UCPTRIE_16, src, limit, norm16);
				if(!isCompYesAndZeroCC(norm16)) {
					break;
				}
			}
		}
		// isCompYesAndZeroCC(norm16) is false, that is, norm16>=minNoNo.
		// The current character is either a "noNo" (has a mapping)
		// or a "maybeYes" (combines backward)
		// or a "yesYes" with ccc!=0.
		// It is not a Hangul syllable or Jamo L because those have "yes" properties.

		// Medium-fast path: Handle cases that do not require full decomposition and recomposition.
		if(!isMaybeOrNonZeroCC(norm16)) { // minNoNo <= norm16 < minMaybeYes
			if(sink == nullptr) {
				return FALSE;
			}
			// Fast path for mapping a character that is immediately surrounded by boundaries.
			// In this case, we need not decompose around the current character.
			if(isDecompNoAlgorithmic(norm16)) {
				// Maps to a single isCompYesAndZeroCC character
				// which also implies hasCompBoundaryBefore.
				if(norm16HasCompBoundaryAfter(norm16, onlyContiguous) ||
				    hasCompBoundaryBefore(src, limit)) {
					if(prevBoundary != prevSrc &&
					    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
					    *sink, options, edits, errorCode)) {
						break;
					}
					appendCodePointDelta(prevSrc, src, getAlgorithmicDelta(norm16), *sink, edits);
					prevBoundary = src;
					continue;
				}
			}
			else if(norm16 < minNoNoCompBoundaryBefore) {
				// The mapping is comp-normalized which also implies hasCompBoundaryBefore.
				if(norm16HasCompBoundaryAfter(norm16, onlyContiguous) ||
				    hasCompBoundaryBefore(src, limit)) {
					if(prevBoundary != prevSrc &&
					    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
					    *sink, options, edits, errorCode)) {
						break;
					}
					const uint16_t * mapping = getMapping(norm16);
					int32_t length = *mapping++ & MAPPING_LENGTH_MASK;
					if(!ByteSinkUtil::appendChange(prevSrc, src, (const UChar *)mapping, length,
					    *sink, edits, errorCode)) {
						break;
					}
					prevBoundary = src;
					continue;
				}
			}
			else if(norm16 >= minNoNoEmpty) {
				// The current character maps to nothing.
				// Simply omit it from the output if there is a boundary before _or_ after it.
				// The character itself implies no boundaries.
				if(hasCompBoundaryBefore(src, limit) ||
				    hasCompBoundaryAfter(prevBoundary, prevSrc, onlyContiguous)) {
					if(prevBoundary != prevSrc &&
					    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
					    *sink, options, edits, errorCode)) {
						break;
					}
					if(edits != nullptr) {
						edits->addReplace((int32_t)(src - prevSrc), 0);
					}
					prevBoundary = src;
					continue;
				}
			}
			// Other "noNo" type, or need to examine more text around this character:
			// Fall through to the slow path.
		}
		else if(isJamoVT(norm16)) {
			// Jamo L: E1 84 80..92
			// Jamo V: E1 85 A1..B5
			// Jamo T: E1 86 A8..E1 87 82
			U_ASSERT((src - prevSrc) == 3 && *prevSrc == 0xe1);
			UChar32 prev = previousHangulOrJamo(prevBoundary, prevSrc);
			if(prevSrc[1] == 0x85) {
				// The current character is a Jamo Vowel,
				// compose with previous Jamo L and following Jamo T.
				UChar32 l = prev - Hangul::JAMO_L_BASE;
				if((uint32_t)l < Hangul::JAMO_L_COUNT) {
					if(sink == nullptr) {
						return FALSE;
					}
					int32_t t = getJamoTMinusBase(src, limit);
					if(t >= 0) {
						// The next character is a Jamo T.
						src += 3;
					}
					else if(hasCompBoundaryBefore(src, limit)) {
						// No Jamo T follows, not even via decomposition.
						t = 0;
					}
					if(t >= 0) {
						UChar32 syllable = Hangul::HANGUL_BASE +
						    (l*Hangul::JAMO_V_COUNT + (prevSrc[2]-0xa1)) *
						    Hangul::JAMO_T_COUNT + t;
						prevSrc -= 3; // Replace the Jamo L as well.
						if(prevBoundary != prevSrc &&
						    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
						    *sink, options, edits, errorCode)) {
							break;
						}
						ByteSinkUtil::appendCodePoint(prevSrc, src, syllable, *sink, edits);
						prevBoundary = src;
						continue;
					}
					// If we see L+V+x where x!=T then we drop to the slow path,
					// decompose and recompose.
					// This is to deal with NFKC finding normal L and V but a
					// compatibility variant of a T.
					// We need to either fully compose that combination here
					// (which would complicate the code and may not work with strange custom data)
					// or use the slow path.
				}
			}
			else if(Hangul::isHangulLV(prev)) {
				// The current character is a Jamo Trailing consonant,
				// compose with previous Hangul LV that does not contain a Jamo T.
				if(sink == nullptr) {
					return FALSE;
				}
				UChar32 syllable = prev + getJamoTMinusBase(prevSrc, src);
				prevSrc -= 3; // Replace the Hangul LV as well.
				if(prevBoundary != prevSrc &&
				    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
				    *sink, options, edits, errorCode)) {
					break;
				}
				ByteSinkUtil::appendCodePoint(prevSrc, src, syllable, *sink, edits);
				prevBoundary = src;
				continue;
			}
			// No matching context, or may need to decompose surrounding text first:
			// Fall through to the slow path.
		}
		else if(norm16 > JAMO_VT) { // norm16 >= MIN_YES_YES_WITH_CC
			// One or more combining marks that do not combine-back:
			// Check for canonical order, copy unchanged if ok and
			// if followed by a character with a boundary-before.
			uint8 cc = getCCFromNormalYesOrMaybe(norm16); // cc!=0
			if(onlyContiguous /* FCC */ && getPreviousTrailCC(prevBoundary, prevSrc) > cc) {
				// Fails FCD test, need to decompose and contiguously recompose.
				if(sink == nullptr) {
					return FALSE;
				}
			}
			else {
				// If !onlyContiguous (not FCC), then we ignore the tccc of
				// the previous character which passed the quick check "yes && ccc==0" test.
				const uint8 * nextSrc;
				uint16_t n16;
				for(;;) {
					if(src == limit) {
						if(sink != nullptr) {
							ByteSinkUtil::appendUnchanged(prevBoundary, limit,
							    *sink, options, edits, errorCode);
						}
						return TRUE;
					}
					uint8 prevCC = cc;
					nextSrc = src;
					UCPTRIE_FAST_U8_NEXT(normTrie, UCPTRIE_16, nextSrc, limit, n16);
					if(n16 >= MIN_YES_YES_WITH_CC) {
						cc = getCCFromNormalYesOrMaybe(n16);
						if(prevCC > cc) {
							if(sink == nullptr) {
								return FALSE;
							}
							break;
						}
					}
					else {
						break;
					}
					src = nextSrc;
				}
				// src is after the last in-order combining mark.
				// If there is a boundary here, then we continue with no change.
				if(norm16HasCompBoundaryBefore(n16)) {
					if(isCompYesAndZeroCC(n16)) {
						src = nextSrc;
					}
					continue;
				}
				// Use the slow path. There is no boundary in [prevSrc, src[.
			}
		}

		// Slow path: Find the nearest boundaries around the current character,
		// decompose and recompose.
		if(prevBoundary != prevSrc && !norm16HasCompBoundaryBefore(norm16)) {
			const uint8 * p = prevSrc;
			UCPTRIE_FAST_U8_PREV(normTrie, UCPTRIE_16, prevBoundary, p, norm16);
			if(!norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
				prevSrc = p;
			}
		}
		ReorderingBuffer buffer(*this, s16, errorCode);
		if(U_FAILURE(errorCode)) {
			break;
		}
		// We know there is not a boundary here.
		decomposeShort(prevSrc, src, STOP_AT_LIMIT, onlyContiguous,
		    buffer, errorCode);
		// Decompose until the next boundary.
		src = decomposeShort(src, limit, STOP_AT_COMP_BOUNDARY, onlyContiguous,
			buffer, errorCode);
		if(U_FAILURE(errorCode)) {
			break;
		}
		if((src - prevSrc) > INT32_MAX) { // guard before buffer.equals()
			errorCode = U_INDEX_OUTOFBOUNDS_ERROR;
			return TRUE;
		}
		recompose(buffer, 0, onlyContiguous);
		if(!buffer.equals(prevSrc, src)) {
			if(sink == nullptr) {
				return FALSE;
			}
			if(prevBoundary != prevSrc &&
			    !ByteSinkUtil::appendUnchanged(prevBoundary, prevSrc,
			    *sink, options, edits, errorCode)) {
				break;
			}
			if(!ByteSinkUtil::appendChange(prevSrc, src, buffer.getStart(), buffer.length(),
			    *sink, edits, errorCode)) {
				break;
			}
			prevBoundary = src;
		}
	}
	return TRUE;
}

bool Normalizer2Impl::hasCompBoundaryBefore(const UChar * src, const UChar * limit) const {
	if(src == limit || *src < minCompNoMaybeCP) {
		return TRUE;
	}
	UChar32 c;
	uint16_t norm16;
	UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, src, limit, c, norm16);
	return norm16HasCompBoundaryBefore(norm16);
}

bool Normalizer2Impl::hasCompBoundaryBefore(const uint8 * src, const uint8 * limit) const {
	if(src == limit) {
		return TRUE;
	}
	uint16_t norm16;
	UCPTRIE_FAST_U8_NEXT(normTrie, UCPTRIE_16, src, limit, norm16);
	return norm16HasCompBoundaryBefore(norm16);
}

bool Normalizer2Impl::hasCompBoundaryAfter(const UChar * start, const UChar * p,
    bool onlyContiguous) const {
	if(start == p) {
		return TRUE;
	}
	UChar32 c;
	uint16_t norm16;
	UCPTRIE_FAST_U16_PREV(normTrie, UCPTRIE_16, start, p, c, norm16);
	return norm16HasCompBoundaryAfter(norm16, onlyContiguous);
}

bool Normalizer2Impl::hasCompBoundaryAfter(const uint8 * start, const uint8 * p,
    bool onlyContiguous) const {
	if(start == p) {
		return TRUE;
	}
	uint16_t norm16;
	UCPTRIE_FAST_U8_PREV(normTrie, UCPTRIE_16, start, p, norm16);
	return norm16HasCompBoundaryAfter(norm16, onlyContiguous);
}

const UChar * Normalizer2Impl::findPreviousCompBoundary(const UChar * start, const UChar * p,
    bool onlyContiguous) const {
	while(p != start) {
		const UChar * codePointLimit = p;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_PREV(normTrie, UCPTRIE_16, start, p, c, norm16);
		if(norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return codePointLimit;
		}
		if(hasCompBoundaryBefore(c, norm16)) {
			return p;
		}
	}
	return p;
}

const UChar * Normalizer2Impl::findNextCompBoundary(const UChar * p, const UChar * limit,
    bool onlyContiguous) const {
	while(p != limit) {
		const UChar * codePointStart = p;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, p, limit, c, norm16);
		if(hasCompBoundaryBefore(c, norm16)) {
			return codePointStart;
		}
		if(norm16HasCompBoundaryAfter(norm16, onlyContiguous)) {
			return p;
		}
	}
	return p;
}

uint8 Normalizer2Impl::getPreviousTrailCC(const UChar * start, const UChar * p) const {
	if(start == p) {
		return 0;
	}
	int32_t i = (int32_t)(p - start);
	UChar32 c;
	U16_PREV(start, 0, i, c);
	return (uint8)getFCD16(c);
}

uint8 Normalizer2Impl::getPreviousTrailCC(const uint8 * start, const uint8 * p) const {
	if(start == p) {
		return 0;
	}
	int32_t i = (int32_t)(p - start);
	UChar32 c;
	U8_PREV(start, 0, i, c);
	return (uint8)getFCD16(c);
}

// Note: normalizer2impl.cpp r30982 (2011-nov-27)
// still had getFCDTrie() which built and cached an FCD trie.
// That provided faster access to FCD data than getFCD16FromNormData()
// but required synchronization and consumed some 10kB of heap memory
// in any process that uses FCD (e.g., via collation).
// minDecompNoCP etc. and smallFCD[] are intended to help with any loss of performance,
// at least for ASCII & CJK.

// Ticket 20907 - The optimizer in MSVC/Visual Studio versions below 16.4 has trouble with this
// function on Windows ARM64. As a work-around, we disable optimizations for this function.
// This work-around could/should be removed once the following versions of Visual Studio are no
// longer supported: All versions of VS2017, and versions of VS2019 below 16.4.
#if(defined(_MSC_VER) && (defined(_M_ARM64)) && (_MSC_VER < 1924))
#pragma optimize( "", off )
#endif
// Gets the FCD value from the regular normalization data.
uint16_t Normalizer2Impl::getFCD16FromNormData(UChar32 c) const {
	uint16_t norm16 = getNorm16(c);
	if(norm16 >= limitNoNo) {
		if(norm16>=MIN_NORMAL_MAYBE_YES) {
			// combining mark
			norm16 = getCCFromNormalYesOrMaybe(norm16);
			return norm16|(norm16<<8);
		}
		else if(norm16>=minMaybeYes) {
			return 0;
		}
		else { // isDecompNoAlgorithmic(norm16)
			uint16_t deltaTrailCC = norm16 & DELTA_TCCC_MASK;
			if(deltaTrailCC <= DELTA_TCCC_1) {
				return deltaTrailCC >> OFFSET_SHIFT;
			}
			// Maps to an isCompYesAndZeroCC.
			c = mapAlgorithmic(c, norm16);
			norm16 = getRawNorm16(c);
		}
	}
	if(norm16<=minYesNo || isHangulLVT(norm16)) {
		// no decomposition or Hangul syllable, all zeros
		return 0;
	}
	// c decomposes, get everything from the variable-length extra data
	const uint16_t * mapping = getMapping(norm16);
	uint16_t firstUnit = *mapping;
	norm16 = firstUnit>>8; // tccc
	if(firstUnit&MAPPING_HAS_CCC_LCCC_WORD) {
		norm16 |= *(mapping-1)&0xff00; // lccc
	}
	return norm16;
}

#if(defined(_MSC_VER) && (defined(_M_ARM64)) && (_MSC_VER < 1924))
#pragma optimize( "", on )
#endif

// Dual functionality:
// buffer!=NULL: normalize
// buffer==NULL: isNormalized/quickCheck/spanQuickCheckYes
const UChar * Normalizer2Impl::makeFCD(const UChar * src, const UChar * limit, ReorderingBuffer * buffer, UErrorCode & errorCode) const 
{
	// Tracks the last FCD-safe boundary, before lccc=0 or after properly-ordered tccc<=1.
	// Similar to the prevBoundary in the compose() implementation.
	const UChar * prevBoundary = src;
	int32_t prevFCD16 = 0;
	if(!limit) {
		src = copyLowPrefixFromNulTerminated(src, minLcccCP, buffer, errorCode);
		if(U_FAILURE(errorCode)) {
			return src;
		}
		if(prevBoundary<src) {
			prevBoundary = src;
			// We know that the previous character's lccc==0.
			// Fetching the fcd16 value was deferred for this below-U+0300 code point.
			prevFCD16 = getFCD16(*(src-1));
			if(prevFCD16>1) {
				--prevBoundary;
			}
		}
		limit = u_strchr(src, 0);
	}
	// Note: In this function we use buffer->appendZeroCC() because we track
	// the lead and trail combining classes here, rather than leaving it to
	// the ReorderingBuffer.
	// The exception is the call to decomposeShort() which uses the buffer
	// in the normal way.
	const UChar * prevSrc;
	UChar32 c = 0;
	uint16_t fcd16 = 0;
	for(;;) {
		// count code units with lccc==0
		for(prevSrc = src; src!=limit;) {
			if((c = *src)<minLcccCP) {
				prevFCD16 = ~c;
				++src;
			}
			else if(!singleLeadMightHaveNonZeroFCD16(c)) {
				prevFCD16 = 0;
				++src;
			}
			else {
				if(U16_IS_LEAD(c)) {
					UChar c2;
					if((src+1)!=limit && U16_IS_TRAIL(c2 = src[1])) {
						c = U16_GET_SUPPLEMENTARY(c, c2);
					}
				}
				if((fcd16 = getFCD16FromNormData(c))<=0xff) {
					prevFCD16 = fcd16;
					src += U16_LENGTH(c);
				}
				else {
					break;
				}
			}
		}
		// copy these code units all at once
		if(src!=prevSrc) {
			if(buffer!=NULL && !buffer->appendZeroCC(prevSrc, src, errorCode)) {
				break;
			}
			if(src==limit) {
				break;
			}
			prevBoundary = src;
			// We know that the previous character's lccc==0.
			if(prevFCD16<0) {
				// Fetching the fcd16 value was deferred for this below-minLcccCP code point.
				UChar32 prev = ~prevFCD16;
				if(prev<minDecompNoCP) {
					prevFCD16 = 0;
				}
				else {
					prevFCD16 = getFCD16FromNormData(prev);
					if(prevFCD16>1) {
						--prevBoundary;
					}
				}
			}
			else {
				const UChar * p = src-1;
				if(U16_IS_TRAIL(*p) && prevSrc<p && U16_IS_LEAD(*(p-1))) {
					--p;
					// Need to fetch the previous character's FCD value because
					// prevFCD16 was just for the trail surrogate code point.
					prevFCD16 = getFCD16FromNormData(U16_GET_SUPPLEMENTARY(p[0], p[1]));
					// Still known to have lccc==0 because its lead surrogate unit had lccc==0.
				}
				if(prevFCD16>1) {
					prevBoundary = p;
				}
			}
			// The start of the current character (c).
			prevSrc = src;
		}
		else if(src==limit) {
			break;
		}

		src += U16_LENGTH(c);
		// The current character (c) at [prevSrc..src[ has a non-zero lead combining class.
		// Check for proper order, and decompose locally if necessary.
		if((prevFCD16&0xff)<=(fcd16>>8)) {
			// proper order: prev tccc <= current lccc
			if((fcd16&0xff)<=1) {
				prevBoundary = src;
			}
			if(buffer!=NULL && !buffer->appendZeroCC(c, errorCode)) {
				break;
			}
			prevFCD16 = fcd16;
			continue;
		}
		else if(buffer==NULL) {
			return prevBoundary; // quick check "no"
		}
		else {
			/*
			 * Back out the part of the source that we copied or appended
			 * already but is now going to be decomposed.
			 * prevSrc is set to after what was copied/appended.
			 */
			buffer->removeSuffix((int32_t)(prevSrc-prevBoundary));
			/*
			 * Find the part of the source that needs to be decomposed,
			 * up to the next safe boundary.
			 */
			src = findNextFCDBoundary(src, limit);
			/*
			 * The source text does not fulfill the conditions for FCD.
			 * Decompose and reorder a limited piece of the text.
			 */
			decomposeShort(prevBoundary, src, FALSE, FALSE, *buffer, errorCode);
			if(U_FAILURE(errorCode)) {
				break;
			}
			prevBoundary = src;
			prevFCD16 = 0;
		}
	}
	return src;
}

void Normalizer2Impl::makeFCDAndAppend(const UChar * src, const UChar * limit, bool doMakeFCD,
    UnicodeString & safeMiddle, ReorderingBuffer &buffer, UErrorCode & errorCode) const 
{
	if(!buffer.isEmpty()) {
		const UChar * firstBoundaryInSrc = findNextFCDBoundary(src, limit);
		if(src!=firstBoundaryInSrc) {
			const UChar * lastBoundaryInDest = findPreviousFCDBoundary(buffer.getStart(), buffer.getLimit());
			int32_t destSuffixLength = (int32_t)(buffer.getLimit()-lastBoundaryInDest);
			UnicodeString middle(lastBoundaryInDest, destSuffixLength);
			buffer.removeSuffix(destSuffixLength);
			safeMiddle = middle;
			middle.append(src, (int32_t)(firstBoundaryInSrc-src));
			const UChar * middleStart = middle.getBuffer();
			makeFCD(middleStart, middleStart+middle.length(), &buffer, errorCode);
			if(U_FAILURE(errorCode)) {
				return;
			}
			src = firstBoundaryInSrc;
		}
	}
	if(doMakeFCD) {
		makeFCD(src, limit, &buffer, errorCode);
	}
	else {
		SETIFZQ(limit, u_strchr(src, 0)); // appendZeroCC() needs limit!=NULL
		buffer.appendZeroCC(src, limit, errorCode);
	}
}

const UChar * Normalizer2Impl::findPreviousFCDBoundary(const UChar * start, const UChar * p) const 
{
	while(start < p) {
		const UChar * codePointLimit = p;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_PREV(normTrie, UCPTRIE_16, start, p, c, norm16);
		if(c < minDecompNoCP || norm16HasDecompBoundaryAfter(norm16)) {
			return codePointLimit;
		}
		if(norm16HasDecompBoundaryBefore(norm16)) {
			return p;
		}
	}
	return p;
}

const UChar * Normalizer2Impl::findNextFCDBoundary(const UChar * p, const UChar * limit) const 
{
	while(p < limit) {
		const UChar * codePointStart = p;
		UChar32 c;
		uint16_t norm16;
		UCPTRIE_FAST_U16_NEXT(normTrie, UCPTRIE_16, p, limit, c, norm16);
		if(c < minLcccCP || norm16HasDecompBoundaryBefore(norm16)) {
			return codePointStart;
		}
		if(norm16HasDecompBoundaryAfter(norm16)) {
			return p;
		}
	}
	return p;
}
//
// CanonicalIterator data
//
CanonIterData::CanonIterData(UErrorCode & errorCode) : mutableTrie(umutablecptrie_open(0, 0, &errorCode)), trie(nullptr),
	canonStartSets(uprv_deleteUObject, NULL, errorCode) 
{
}

CanonIterData::~CanonIterData() 
{
	umutablecptrie_close(mutableTrie);
	ucptrie_close(trie);
}

void CanonIterData::addToStartSet(UChar32 origin, UChar32 decompLead, UErrorCode & errorCode) 
{
	uint32_t canonValue = umutablecptrie_get(mutableTrie, decompLead);
	if((canonValue&(CANON_HAS_SET|CANON_VALUE_MASK))==0 && origin!=0) {
		// origin is the first character whose decomposition starts with
		// the character for which we are setting the value.
		umutablecptrie_set(mutableTrie, decompLead, canonValue|origin, &errorCode);
	}
	else {
		// origin is not the first character, or it is U+0000.
		UnicodeSet * set;
		if((canonValue&CANON_HAS_SET)==0) {
			set = new UnicodeSet;
			if(set==NULL) {
				errorCode = U_MEMORY_ALLOCATION_ERROR;
				return;
			}
			UChar32 firstOrigin = (UChar32)(canonValue&CANON_VALUE_MASK);
			canonValue = (canonValue&~CANON_VALUE_MASK)|CANON_HAS_SET|(uint32_t)canonStartSets.size();
			umutablecptrie_set(mutableTrie, decompLead, canonValue, &errorCode);
			canonStartSets.addElementX(set, errorCode);
			if(firstOrigin!=0) {
				set->add(firstOrigin);
			}
		}
		else {
			set = (UnicodeSet*)canonStartSets[(int32_t)(canonValue&CANON_VALUE_MASK)];
		}
		set->add(origin);
	}
}

// C++ class for friend access to private Normalizer2Impl members.
class InitCanonIterData {
public:
	static void doInit(Normalizer2Impl * impl, UErrorCode & errorCode);
};

U_CDECL_BEGIN

// UInitOnce instantiation function for CanonIterData
static void U_CALLCONV initCanonIterData(Normalizer2Impl * impl, UErrorCode & errorCode) {
	InitCanonIterData::doInit(impl, errorCode);
}

U_CDECL_END

void InitCanonIterData::doInit(Normalizer2Impl * impl, UErrorCode & errorCode) {
	U_ASSERT(impl->fCanonIterData == NULL);
	impl->fCanonIterData = new CanonIterData(errorCode);
	if(impl->fCanonIterData == NULL) {
		errorCode = U_MEMORY_ALLOCATION_ERROR;
	}
	if(U_SUCCESS(errorCode)) {
		UChar32 start = 0, end;
		uint32_t value;
		while((end = ucptrie_getRange(impl->normTrie, start,
		    UCPMAP_RANGE_FIXED_LEAD_SURROGATES, Normalizer2Impl::INERT,
		    nullptr, nullptr, &value)) >= 0) {
			// Call Normalizer2Impl::makeCanonIterDataFromNorm16() for a range of same-norm16 characters.
			if(value != Normalizer2Impl::INERT) {
				impl->makeCanonIterDataFromNorm16(start, end, value, *impl->fCanonIterData, errorCode);
			}
			start = end + 1;
		}
#ifdef UCPTRIE_DEBUG
		umutablecptrie_setName(impl->fCanonIterData->mutableTrie, "CanonIterData");
#endif
		impl->fCanonIterData->trie = umutablecptrie_buildImmutable(
			impl->fCanonIterData->mutableTrie, UCPTRIE_TYPE_SMALL, UCPTRIE_VALUE_BITS_32, &errorCode);
		umutablecptrie_close(impl->fCanonIterData->mutableTrie);
		impl->fCanonIterData->mutableTrie = nullptr;
	}
	if(U_FAILURE(errorCode)) {
		delete impl->fCanonIterData;
		impl->fCanonIterData = NULL;
	}
}

void Normalizer2Impl::makeCanonIterDataFromNorm16(UChar32 start, UChar32 end, const uint16_t norm16,
    CanonIterData &newData,
    UErrorCode & errorCode) const {
	if(isInert(norm16) || (minYesNo<=norm16 && norm16<minNoNo)) {
		// Inert, or 2-way mapping (including Hangul syllable).
		// We do not write a canonStartSet for any yesNo character.
		// Composites from 2-way mappings are added at runtime from the
		// starter's compositions list, and the other characters in
		// 2-way mappings get CANON_NOT_SEGMENT_STARTER set because they are
		// "maybe" characters.
		return;
	}
	for(UChar32 c = start; c<=end; ++c) {
		uint32_t oldValue = umutablecptrie_get(newData.mutableTrie, c);
		uint32_t newValue = oldValue;
		if(isMaybeOrNonZeroCC(norm16)) {
			// not a segment starter if it occurs in a decomposition or has cc!=0
			newValue |= CANON_NOT_SEGMENT_STARTER;
			if(norm16<MIN_NORMAL_MAYBE_YES) {
				newValue |= CANON_HAS_COMPOSITIONS;
			}
		}
		else if(norm16<minYesNo) {
			newValue |= CANON_HAS_COMPOSITIONS;
		}
		else {
			// c has a one-way decomposition
			UChar32 c2 = c;
			// Do not modify the whole-range norm16 value.
			uint16_t norm16_2 = norm16;
			if(isDecompNoAlgorithmic(norm16_2)) {
				// Maps to an isCompYesAndZeroCC.
				c2 = mapAlgorithmic(c2, norm16_2);
				norm16_2 = getRawNorm16(c2);
				// No compatibility mappings for the CanonicalIterator.
				U_ASSERT(!(isHangulLV(norm16_2) || isHangulLVT(norm16_2)));
			}
			if(norm16_2 > minYesNo) {
				// c decomposes, get everything from the variable-length extra data
				const uint16_t * mapping = getMapping(norm16_2);
				uint16_t firstUnit = *mapping;
				int32_t length = firstUnit&MAPPING_LENGTH_MASK;
				if((firstUnit&MAPPING_HAS_CCC_LCCC_WORD)!=0) {
					if(c==c2 && (*(mapping-1)&0xff)!=0) {
						newValue |= CANON_NOT_SEGMENT_STARTER; // original c has cc!=0
					}
				}
				// Skip empty mappings (no characters in the decomposition).
				if(length!=0) {
					++mapping; // skip over the firstUnit
					// add c to first code point's start set
					int32_t i = 0;
					U16_NEXT_UNSAFE(mapping, i, c2);
					newData.addToStartSet(c, c2, errorCode);
					// Set CANON_NOT_SEGMENT_STARTER for each remaining code point of a
					// one-way mapping. A 2-way mapping is possible here after
					// intermediate algorithmic mapping.
					if(norm16_2>=minNoNo) {
						while(i<length) {
							U16_NEXT_UNSAFE(mapping, i, c2);
							uint32_t c2Value = umutablecptrie_get(newData.mutableTrie, c2);
							if((c2Value&CANON_NOT_SEGMENT_STARTER)==0) {
								umutablecptrie_set(newData.mutableTrie, c2,
								    c2Value|CANON_NOT_SEGMENT_STARTER, &errorCode);
							}
						}
					}
				}
			}
			else {
				// c decomposed to c2 algorithmically; c has cc==0
				newData.addToStartSet(c, c2, errorCode);
			}
		}
		if(newValue!=oldValue) {
			umutablecptrie_set(newData.mutableTrie, c, newValue, &errorCode);
		}
	}
}

bool Normalizer2Impl::ensureCanonIterData(UErrorCode & errorCode) const {
	// Logically const: Synchronized instantiation.
	Normalizer2Impl * me = const_cast<Normalizer2Impl *>(this);
	umtx_initOnce(me->fCanonIterDataInitOnce, &initCanonIterData, me, errorCode);
	return U_SUCCESS(errorCode);
}

int32_t Normalizer2Impl::getCanonValue(UChar32 c) const {
	return (int32_t)ucptrie_get(fCanonIterData->trie, c);
}

const UnicodeSet &Normalizer2Impl::getCanonStartSet(int32_t n) const {
	return *(const UnicodeSet*)fCanonIterData->canonStartSets[n];
}

bool Normalizer2Impl::isCanonSegmentStarter(UChar32 c) const {
	return getCanonValue(c)>=0;
}

bool Normalizer2Impl::getCanonStartSet(UChar32 c, UnicodeSet & set) const {
	int32_t canonValue = getCanonValue(c)&~CANON_NOT_SEGMENT_STARTER;
	if(canonValue==0) {
		return FALSE;
	}
	set.clear();
	int32_t value = canonValue&CANON_VALUE_MASK;
	if((canonValue&CANON_HAS_SET)!=0) {
		set.addAll(getCanonStartSet(value));
	}
	else if(value!=0) {
		set.add(value);
	}
	if((canonValue&CANON_HAS_COMPOSITIONS)!=0) {
		uint16_t norm16 = getRawNorm16(c);
		if(norm16==JAMO_L) {
			UChar32 syllable =
			    (UChar32)(Hangul::HANGUL_BASE+(c-Hangul::JAMO_L_BASE)*Hangul::JAMO_VT_COUNT);
			set.add(syllable, syllable+Hangul::JAMO_VT_COUNT-1);
		}
		else {
			addComposites(getCompositionsList(norm16), set);
		}
	}
	return TRUE;
}

U_NAMESPACE_END

// Normalizer2 data swapping ----------------------------------------------- ***

U_NAMESPACE_USE

U_CAPI int32_t U_EXPORT2 unorm2_swap(const UDataSwapper * ds,
    const void * inData, int32_t length, void * outData,
    UErrorCode * pErrorCode) {
	const UDataInfo * pInfo;
	int32_t headerSize;

	const uint8 * inBytes;
	uint8 * outBytes;

	const int32_t * inIndexes;
	int32_t indexes[Normalizer2Impl::IX_TOTAL_SIZE+1];

	int32_t i, offset, nextOffset, size;

	/* udata_swapDataHeader checks the arguments */
	headerSize = udata_swapDataHeader(ds, inData, length, outData, pErrorCode);
	if(!pErrorCode || U_FAILURE(*pErrorCode)) {
		return 0;
	}

	/* check data format and format version */
	pInfo = (const UDataInfo*)((const char *)inData+4);
	uint8 formatVersion0 = pInfo->formatVersion[0];
	if(!(
		    pInfo->dataFormat[0]==0x4e && /* dataFormat="Nrm2" */
		    pInfo->dataFormat[1]==0x72 &&
		    pInfo->dataFormat[2]==0x6d &&
		    pInfo->dataFormat[3]==0x32 &&
		    (1<=formatVersion0 && formatVersion0<=4)
		    )) {
		udata_printError(ds,
		    "unorm2_swap(): data format %02x.%02x.%02x.%02x (format version %02x) is not recognized as Normalizer2 data\n",
		    pInfo->dataFormat[0],
		    pInfo->dataFormat[1],
		    pInfo->dataFormat[2],
		    pInfo->dataFormat[3],
		    pInfo->formatVersion[0]);
		*pErrorCode = U_UNSUPPORTED_ERROR;
		return 0;
	}

	inBytes = (const uint8 *)inData+headerSize;
	outBytes = (uint8 *)outData+headerSize;

	inIndexes = (const int32_t*)inBytes;
	int32_t minIndexesLength;
	if(formatVersion0==1) {
		minIndexesLength = Normalizer2Impl::IX_MIN_MAYBE_YES+1;
	}
	else if(formatVersion0==2) {
		minIndexesLength = Normalizer2Impl::IX_MIN_YES_NO_MAPPINGS_ONLY+1;
	}
	else {
		minIndexesLength = Normalizer2Impl::IX_MIN_LCCC_CP+1;
	}

	if(length>=0) {
		length -= headerSize;
		if(length<minIndexesLength*4) {
			udata_printError(ds, "unorm2_swap(): too few bytes (%d after header) for Normalizer2 data\n",
			    length);
			*pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
			return 0;
		}
	}

	/* read the first few indexes */
	for(i = 0; i<UPRV_LENGTHOF(indexes); ++i) {
		indexes[i] = udata_readInt32(ds, inIndexes[i]);
	}

	/* get the total length of the data */
	size = indexes[Normalizer2Impl::IX_TOTAL_SIZE];

	if(length>=0) {
		if(length<size) {
			udata_printError(ds, "unorm2_swap(): too few bytes (%d after header) for all of Normalizer2 data\n",
			    length);
			*pErrorCode = U_INDEX_OUTOFBOUNDS_ERROR;
			return 0;
		}
		/* copy the data for inaccessible bytes */
		if(inBytes!=outBytes) {
			uprv_memcpy(outBytes, inBytes, size);
		}
		offset = 0;
		/* swap the int32_t indexes[] */
		nextOffset = indexes[Normalizer2Impl::IX_NORM_TRIE_OFFSET];
		ds->swapArray32(ds, inBytes, nextOffset-offset, outBytes, pErrorCode);
		offset = nextOffset;
		/* swap the trie */
		nextOffset = indexes[Normalizer2Impl::IX_EXTRA_DATA_OFFSET];
		utrie_swapAnyVersion(ds, inBytes+offset, nextOffset-offset, outBytes+offset, pErrorCode);
		offset = nextOffset;
		/* swap the uint16_t extraData[] */
		nextOffset = indexes[Normalizer2Impl::IX_SMALL_FCD_OFFSET];
		ds->swapArray16(ds, inBytes+offset, nextOffset-offset, outBytes+offset, pErrorCode);
		offset = nextOffset;
		/* no need to swap the uint8 smallFCD[] (new in formatVersion 2) */
		nextOffset = indexes[Normalizer2Impl::IX_SMALL_FCD_OFFSET+1];
		offset = nextOffset;
		U_ASSERT(offset==size);
	}
	return headerSize+size;
}

#endif  // !UCONFIG_NO_NORMALIZATION
