// normalizer2impl.h
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *   Copyright (C) 2009-2014, International Business Machines Corporation and others.  All Rights Reserved.
 *   encoding:   UTF-8
 *   created on: 2009nov22
 *   created by: Markus W. Scherer
 */
#ifndef __NORMALIZER2IMPL_H__
#define __NORMALIZER2IMPL_H__

#include "unicode/utypes.h"

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/normalizer2.h"
#include "unicode/ucptrie.h"
#include "unicode/unistr.h"
#include "unicode/unorm.h"
#include "unicode/utf.h"
#include "unicode/utf16.h"
#include "mutex.h"
#include "udataswp.h"
#include "uset_imp.h"

// When the nfc.nrm data is *not* hardcoded into the common library
// (with this constant set to 0),
// then it needs to be built into the data package:
// Add nfc.nrm to icu4c/source/data/Makefile.in DAT_FILES_SHORT
#define NORM2_HARDCODE_NFC_DATA 1

U_NAMESPACE_BEGIN

struct CanonIterData;

class ByteSink;
class Edits;
class InitCanonIterData;
class LcccContext;

class U_COMMON_API Hangul {
public:
	/* Korean Hangul and Jamo constants */
	enum {
		JAMO_L_BASE = 0x1100, /* "lead" jamo */
		JAMO_L_END = 0x1112,
		JAMO_V_BASE = 0x1161, /* "vowel" jamo */
		JAMO_V_END = 0x1175,
		JAMO_T_BASE = 0x11a7, /* "trail" jamo */
		JAMO_T_END = 0x11c2,

		HANGUL_BASE = 0xac00,
		HANGUL_END = 0xd7a3,

		JAMO_L_COUNT = 19,
		JAMO_V_COUNT = 21,
		JAMO_T_COUNT = 28,

		JAMO_VT_COUNT = JAMO_V_COUNT*JAMO_T_COUNT,

		HANGUL_COUNT = JAMO_L_COUNT*JAMO_V_COUNT*JAMO_T_COUNT,
		HANGUL_LIMIT = HANGUL_BASE+HANGUL_COUNT
	};

	static inline bool isHangul(UChar32 c) {
		return HANGUL_BASE<=c && c<HANGUL_LIMIT;
	}

	static inline bool isHangulLV(UChar32 c) {
		c -= HANGUL_BASE;
		return 0<=c && c<HANGUL_COUNT && c%JAMO_T_COUNT==0;
	}

	static inline bool isJamoL(UChar32 c) {
		return (uint32_t)(c-JAMO_L_BASE)<JAMO_L_COUNT;
	}

	static inline bool isJamoV(UChar32 c) {
		return (uint32_t)(c-JAMO_V_BASE)<JAMO_V_COUNT;
	}

	static inline bool isJamoT(UChar32 c) {
		int32_t t = c-JAMO_T_BASE;
		return 0<t && t<JAMO_T_COUNT; // not JAMO_T_BASE itself
	}

	static bool isJamo(UChar32 c) {
		return JAMO_L_BASE<=c && c<=JAMO_T_END &&
		       (c<=JAMO_L_END || (JAMO_V_BASE<=c && c<=JAMO_V_END) || JAMO_T_BASE<c);
	}

	/**
	 * Decomposes c, which must be a Hangul syllable, into buffer
	 * and returns the length of the decomposition (2 or 3).
	 */
	static inline int32_t decompose(UChar32 c, UChar buffer[3]) {
		c -= HANGUL_BASE;
		UChar32 c2 = c%JAMO_T_COUNT;
		c /= JAMO_T_COUNT;
		buffer[0] = (UChar)(JAMO_L_BASE+c/JAMO_V_COUNT);
		buffer[1] = (UChar)(JAMO_V_BASE+c%JAMO_V_COUNT);
		if(c2==0) {
			return 2;
		}
		else {
			buffer[2] = (UChar)(JAMO_T_BASE+c2);
			return 3;
		}
	}

	/**
	 * Decomposes c, which must be a Hangul syllable, into buffer.
	 * This is the raw, not recursive, decomposition. Its length is always 2.
	 */
	static inline void getRawDecomposition(UChar32 c, UChar buffer[2]) {
		UChar32 orig = c;
		c -= HANGUL_BASE;
		UChar32 c2 = c%JAMO_T_COUNT;
		if(c2==0) {
			c /= JAMO_T_COUNT;
			buffer[0] = (UChar)(JAMO_L_BASE+c/JAMO_V_COUNT);
			buffer[1] = (UChar)(JAMO_V_BASE+c%JAMO_V_COUNT);
		}
		else {
			buffer[0] = (UChar)(orig-c2); // LV syllable
			buffer[1] = (UChar)(JAMO_T_BASE+c2);
		}
	}

private:
	Hangul(); // no instantiation
};

class Normalizer2Impl;

class U_COMMON_API ReorderingBuffer : public UMemory {
public:
	/** Constructs only; init() should be called. */
	ReorderingBuffer(const Normalizer2Impl &ni, UnicodeString & dest) : impl(ni), str(dest),
		start(NULL), reorderStart(NULL), limit(NULL), remainingCapacity(0), lastCC(0) 
	{
	}
	/** Constructs, removes the string contents, and initializes for a small initial capacity. */
	ReorderingBuffer(const Normalizer2Impl &ni, UnicodeString & dest, UErrorCode & errorCode);
	~ReorderingBuffer() 
	{
		if(start!=NULL) {
			str.releaseBuffer((int32_t)(limit-start));
		}
	}
	bool init(int32_t destCapacity, UErrorCode & errorCode);
	bool isEmpty() const { return start==limit; }
	int32_t length() const { return (int32_t)(limit-start); }
	UChar * getStart() { return start; }
	UChar * getLimit() { return limit; }
	uint8 getLastCC() const { return lastCC; }
	bool equals(const UChar * start, const UChar * limit) const;
	bool equals(const uint8 * otherStart, const uint8 * otherLimit) const;
	bool append(UChar32 c, uint8 cc, UErrorCode & errorCode) 
	{
		return (c<=0xffff) ? appendBMP((UChar)c, cc, errorCode) : appendSupplementary(c, cc, errorCode);
	}
	bool append(const UChar * s, int32_t length, bool isNFD, uint8 leadCC, uint8 trailCC, UErrorCode & errorCode);
	bool appendBMP(UChar c, uint8 cc, UErrorCode & errorCode) 
	{
		if(remainingCapacity==0 && !resize(1, errorCode)) {
			return false;
		}
		if(lastCC<=cc || cc==0) {
			*limit++ = c;
			lastCC = cc;
			if(cc<=1) {
				reorderStart = limit;
			}
		}
		else {
			insert(c, cc);
		}
		--remainingCapacity;
		return true;
	}

	bool appendZeroCC(UChar32 c, UErrorCode & errorCode);
	bool appendZeroCC(const UChar * s, const UChar * sLimit, UErrorCode & errorCode);
	void remove();
	void removeSuffix(int32_t suffixLength);
	void setReorderingLimit(UChar * newLimit) 
	{
		remainingCapacity += (int32_t)(limit-newLimit);
		reorderStart = limit = newLimit;
		lastCC = 0;
	}
	void copyReorderableSuffixTo(UnicodeString & s) const 
	{
		s.setTo(ConstChar16Ptr(reorderStart), (int32_t)(limit-reorderStart));
	}

private:
	/*
	 * TODO: Revisit whether it makes sense to track reorderStart.
	 * It is set to after the last known character with cc<=1,
	 * which stops previousCC() before it reads that character and looks up its cc.
	 * previousCC() is normally only called from insert().
	 * In other words, reorderStart speeds up the insertion of a combining mark
	 * into a multi-combining mark sequence where it does not belong at the end.
	 * This might not be worth the trouble.
	 * On the other hand, it's not a huge amount of trouble.
	 *
	 * We probably need it for UNORM_SIMPLE_APPEND.
	 */

	bool appendSupplementary(UChar32 c, uint8 cc, UErrorCode & errorCode);
	void insert(UChar32 c, uint8 cc);
	static void writeCodePoint(UChar * p, UChar32 c) {
		if(c<=0xffff) {
			*p = (UChar)c;
		}
		else {
			p[0] = U16_LEAD(c);
			p[1] = U16_TRAIL(c);
		}
	}

	bool resize(int32_t appendLength, UErrorCode & errorCode);

	const Normalizer2Impl &impl;
	UnicodeString & str;
	UChar * start, * reorderStart, * limit;
	int32_t remainingCapacity;
	uint8 lastCC;

	// private backward iterator
	void setIterator() {
		codePointStart = limit;
	}

	void skipPrevious(); // Requires start<codePointStart.
	uint8 previousCC(); // Returns 0 if there is no previous character.

	UChar * codePointStart, * codePointLimit;
};

/**
 * Low-level implementation of the Unicode Normalization Algorithm.
 * For the data structure and details see the documentation at the end of
 * this normalizer2impl.h and in the design doc at
 * http://site.icu-project.org/design/normalization/custom
 */
class U_COMMON_API Normalizer2Impl : public UObject {
public:
	Normalizer2Impl() : normTrie(NULL), fCanonIterData(NULL) {
	}

	virtual ~Normalizer2Impl();

	void init(const int32_t * inIndexes, const UCPTrie * inTrie,
	    const uint16_t * inExtraData, const uint8 * inSmallFCD);

	void addLcccChars(UnicodeSet & set) const;
	void addPropertyStarts(const USetAdder * sa, UErrorCode & errorCode) const;
	void addCanonIterPropertyStarts(const USetAdder * sa, UErrorCode & errorCode) const;

	// low-level properties ------------------------------------------------ ***

	bool ensureCanonIterData(UErrorCode & errorCode) const;

	// The trie stores values for lead surrogate code *units*.
	// Surrogate code *points* are inert.
	uint16_t getNorm16(UChar32 c) const {
		return U_IS_LEAD(c) ?
		       static_cast<uint16_t>(INERT) :
		       UCPTRIE_FAST_GET(normTrie, UCPTRIE_16, c);
	}

	uint16_t getRawNorm16(UChar32 c) const {
		return UCPTRIE_FAST_GET(normTrie, UCPTRIE_16, c);
	}

	UNormalizationCheckResult getCompQuickCheck(uint16_t norm16) const {
		if(norm16<minNoNo || MIN_YES_YES_WITH_CC<=norm16) {
			return UNORM_YES;
		}
		else if(minMaybeYes<=norm16) {
			return UNORM_MAYBE;
		}
		else {
			return UNORM_NO;
		}
	}

	bool isAlgorithmicNoNo(uint16_t norm16) const {
		return limitNoNo<=norm16 && norm16<minMaybeYes;
	}

	bool isCompNo(uint16_t norm16) const {
		return minNoNo<=norm16 && norm16<minMaybeYes;
	}

	bool isDecompYes(uint16_t norm16) const {
		return norm16<minYesNo || minMaybeYes<=norm16;
	}

	uint8 getCC(uint16_t norm16) const {
		if(norm16>=MIN_NORMAL_MAYBE_YES) {
			return getCCFromNormalYesOrMaybe(norm16);
		}
		if(norm16<minNoNo || limitNoNo<=norm16) {
			return 0;
		}
		return getCCFromNoNo(norm16);
	}

	static uint8 getCCFromNormalYesOrMaybe(uint16_t norm16) {
		return (uint8)(norm16 >> OFFSET_SHIFT);
	}

	static uint8 getCCFromYesOrMaybe(uint16_t norm16) {
		return norm16>=MIN_NORMAL_MAYBE_YES ? getCCFromNormalYesOrMaybe(norm16) : 0;
	}

	uint8 getCCFromYesOrMaybeCP(UChar32 c) const {
		if(c < minCompNoMaybeCP) {
			return 0;
		}
		return getCCFromYesOrMaybe(getNorm16(c));
	}

	/**
	 * Returns the FCD data for code point c.
	 * @param c A Unicode code point.
	 * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
	 */
	uint16_t getFCD16(UChar32 c) const {
		if(c<minDecompNoCP) {
			return 0;
		}
		else if(c<=0xffff) {
			if(!singleLeadMightHaveNonZeroFCD16(c)) {
				return 0;
			}
		}
		return getFCD16FromNormData(c);
	}

	/**
	 * Returns the FCD data for the next code point (post-increment).
	 * Might skip only a lead surrogate rather than the whole surrogate pair if none of
	 * the supplementary code points associated with the lead surrogate have non-zero FCD data.
	 * @param s A valid pointer into a string. Requires s!=limit.
	 * @param limit The end of the string, or NULL.
	 * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
	 */
	uint16_t nextFCD16(const UChar *&s, const UChar * limit) const {
		UChar32 c = *s++;
		if(c<minDecompNoCP || !singleLeadMightHaveNonZeroFCD16(c)) {
			return 0;
		}
		UChar c2;
		if(U16_IS_LEAD(c) && s!=limit && U16_IS_TRAIL(c2 = *s)) {
			c = U16_GET_SUPPLEMENTARY(c, c2);
			++s;
		}
		return getFCD16FromNormData(c);
	}

	/**
	 * Returns the FCD data for the previous code point (pre-decrement).
	 * @param start The start of the string.
	 * @param s A valid pointer into a string. Requires start<s.
	 * @return The lccc(c) in bits 15..8 and tccc(c) in bits 7..0.
	 */
	uint16_t previousFCD16(const UChar * start, const UChar *&s) const {
		UChar32 c = *--s;
		if(c<minDecompNoCP) {
			return 0;
		}
		if(!U16_IS_TRAIL(c)) {
			if(!singleLeadMightHaveNonZeroFCD16(c)) {
				return 0;
			}
		}
		else {
			UChar c2;
			if(start<s && U16_IS_LEAD(c2 = *(s-1))) {
				c = U16_GET_SUPPLEMENTARY(c2, c);
				--s;
			}
		}
		return getFCD16FromNormData(c);
	}

	/** Returns true if the single-or-lead code unit c might have non-zero FCD data. */
	bool singleLeadMightHaveNonZeroFCD16(UChar32 lead) const {
		// 0<=lead<=0xffff
		uint8 bits = smallFCD[lead>>8];
		if(bits==0) {
			return false;
		}
		return (bool)((bits>>((lead>>5)&7))&1);
	}

	/** Returns the FCD value from the regular normalization data. */
	uint16_t getFCD16FromNormData(UChar32 c) const;

	/**
	 * Gets the decomposition for one code point.
	 * @param c code point
	 * @param buffer out-only buffer for algorithmic decompositions
	 * @param length out-only, takes the length of the decomposition, if any
	 * @return pointer to the decomposition, or NULL if none
	 */
	const UChar * getDecomposition(UChar32 c, UChar buffer[4], int32_t &length) const;

	/**
	 * Gets the raw decomposition for one code point.
	 * @param c code point
	 * @param buffer out-only buffer for algorithmic decompositions
	 * @param length out-only, takes the length of the decomposition, if any
	 * @return pointer to the decomposition, or NULL if none
	 */
	const UChar * getRawDecomposition(UChar32 c, UChar buffer[30], int32_t &length) const;

	UChar32 composePair(UChar32 a, UChar32 b) const;

	bool isCanonSegmentStarter(UChar32 c) const;
	bool getCanonStartSet(UChar32 c, UnicodeSet & set) const;

	enum {
		// Fixed norm16 values.
		MIN_YES_YES_WITH_CC = 0xfe02,
		JAMO_VT = 0xfe00,
		MIN_NORMAL_MAYBE_YES = 0xfc00,
		JAMO_L = 2, // offset=1 hasCompBoundaryAfter=false
		INERT = 1, // offset=0 hasCompBoundaryAfter=true

		// norm16 bit 0 is comp-boundary-after.
		HAS_COMP_BOUNDARY_AFTER = 1,
		OFFSET_SHIFT = 1,

		// For algorithmic one-way mappings, norm16 bits 2..1 indicate the
		// tccc (0, 1, >1) for quick FCC boundary-after tests.
		DELTA_TCCC_0 = 0,
		DELTA_TCCC_1 = 2,
		DELTA_TCCC_GT_1 = 4,
		DELTA_TCCC_MASK = 6,
		DELTA_SHIFT = 3,

		MAX_DELTA = 0x40
	};

	enum {
		// Byte offsets from the start of the data, after the generic header.
		IX_NORM_TRIE_OFFSET,
		IX_EXTRA_DATA_OFFSET,
		IX_SMALL_FCD_OFFSET,
		IX_RESERVED3_OFFSET,
		IX_RESERVED4_OFFSET,
		IX_RESERVED5_OFFSET,
		IX_RESERVED6_OFFSET,
		IX_TOTAL_SIZE,

		// Code point thresholds for quick check codes.
		IX_MIN_DECOMP_NO_CP,
		IX_MIN_COMP_NO_MAYBE_CP,

		// Norm16 value thresholds for quick check combinations and types of extra data.

		/** Mappings & compositions in [minYesNo..minYesNoMappingsOnly[. */
		IX_MIN_YES_NO,
		/** Mappings are comp-normalized. */
		IX_MIN_NO_NO,
		IX_LIMIT_NO_NO,
		IX_MIN_MAYBE_YES,

		/** Mappings only in [minYesNoMappingsOnly..minNoNo[. */
		IX_MIN_YES_NO_MAPPINGS_ONLY,
		/** Mappings are not comp-normalized but have a comp boundary before. */
		IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE,
		/** Mappings do not have a comp boundary before. */
		IX_MIN_NO_NO_COMP_NO_MAYBE_CC,
		/** Mappings to the empty string. */
		IX_MIN_NO_NO_EMPTY,

		IX_MIN_LCCC_CP,
		IX_RESERVED19,
		IX_COUNT
	};

	enum {
		MAPPING_HAS_CCC_LCCC_WORD = 0x80,
		MAPPING_HAS_RAW_MAPPING = 0x40,
		// unused bit 0x20,
		MAPPING_LENGTH_MASK = 0x1f
	};

	enum {
		COMP_1_LAST_TUPLE = 0x8000,
		COMP_1_TRIPLE = 1,
		COMP_1_TRAIL_LIMIT = 0x3400,
		COMP_1_TRAIL_MASK = 0x7ffe,
		COMP_1_TRAIL_SHIFT = 9, // 10-1 for the "triple" bit
		COMP_2_TRAIL_SHIFT = 6,
		COMP_2_TRAIL_MASK = 0xffc0
	};

	// higher-level functionality ------------------------------------------ ***

	// NFD without an NFD Normalizer2 instance.
	UnicodeString & decompose(const UnicodeString & src, UnicodeString & dest,
	    UErrorCode & errorCode) const;
	/**
	 * Decomposes [src, limit[ and writes the result to dest.
	 * limit can be NULL if src is NUL-terminated.
	 * destLengthEstimate is the initial dest buffer capacity and can be -1.
	 */
	void decompose(const UChar * src, const UChar * limit,
	    UnicodeString & dest, int32_t destLengthEstimate,
	    UErrorCode & errorCode) const;

	const UChar * decompose(const UChar * src, const UChar * limit,
	    ReorderingBuffer * buffer, UErrorCode & errorCode) const;
	void decomposeAndAppend(const UChar * src, const UChar * limit,
	    bool doDecompose,
	    UnicodeString & safeMiddle,
	    ReorderingBuffer &buffer,
	    UErrorCode & errorCode) const;

	/** sink==nullptr: isNormalized()/spanQuickCheckYes() */
	const uint8 * decomposeUTF8(uint32_t options,
	    const uint8 * src, const uint8 * limit,
	    ByteSink * sink, Edits * edits, UErrorCode & errorCode) const;

	bool compose(const UChar * src, const UChar * limit, bool onlyContiguous, bool doCompose,
	    ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	const UChar * composeQuickCheck(const UChar * src, const UChar * limit, bool onlyContiguous, UNormalizationCheckResult * pQCResult) const;
	void composeAndAppend(const UChar * src, const UChar * limit, bool doCompose, bool onlyContiguous,
	    UnicodeString & safeMiddle, ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	/** sink==nullptr: isNormalized() */
	bool composeUTF8(uint32_t options, bool onlyContiguous, const uint8 * src, const uint8 * limit, ByteSink * sink, icu::Edits * edits, UErrorCode & errorCode) const;
	const UChar * makeFCD(const UChar * src, const UChar * limit, ReorderingBuffer * buffer, UErrorCode & errorCode) const;
	void makeFCDAndAppend(const UChar * src, const UChar * limit, bool doMakeFCD, UnicodeString & safeMiddle, ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	bool hasDecompBoundaryBefore(UChar32 c) const;
	bool norm16HasDecompBoundaryBefore(uint16_t norm16) const;
	bool hasDecompBoundaryAfter(UChar32 c) const;
	bool norm16HasDecompBoundaryAfter(uint16_t norm16) const;
	bool isDecompInert(UChar32 c) const { return isDecompYesAndZeroCC(getNorm16(c)); }
	bool hasCompBoundaryBefore(UChar32 c) const { return c<minCompNoMaybeCP || norm16HasCompBoundaryBefore(getNorm16(c)); }
	bool hasCompBoundaryAfter(UChar32 c, bool onlyContiguous) const { return norm16HasCompBoundaryAfter(getNorm16(c), onlyContiguous); }
	bool isCompInert(UChar32 c, bool onlyContiguous) const 
	{
		uint16_t norm16 = getNorm16(c);
		return isCompYesAndZeroCC(norm16) && (norm16 & HAS_COMP_BOUNDARY_AFTER) != 0 &&
		       (!onlyContiguous || isInert(norm16) || *getMapping(norm16) <= 0x1ff);
	}
	bool hasFCDBoundaryBefore(UChar32 c) const { return hasDecompBoundaryBefore(c); }
	bool hasFCDBoundaryAfter(UChar32 c) const { return hasDecompBoundaryAfter(c); }
	bool isFCDInert(UChar32 c) const { return getFCD16(c)<=1; }
private:
	friend class InitCanonIterData;
	friend class LcccContext;
	bool isMaybe(uint16_t norm16) const { return minMaybeYes<=norm16 && norm16<=JAMO_VT; }
	bool isMaybeOrNonZeroCC(uint16_t norm16) const { return norm16>=minMaybeYes; }
	static bool isInert(uint16_t norm16) { return norm16==INERT; }
	static bool isJamoL(uint16_t norm16) { return norm16==JAMO_L; }
	static bool isJamoVT(uint16_t norm16) { return norm16==JAMO_VT; }
	uint16_t hangulLVT() const { return minYesNoMappingsOnly|HAS_COMP_BOUNDARY_AFTER; }
	bool isHangulLV(uint16_t norm16) const { return norm16==minYesNo; }
	bool isHangulLVT(uint16_t norm16) const { return norm16==hangulLVT(); }
	bool isCompYesAndZeroCC(uint16_t norm16) const { return norm16<minNoNo; }

	// bool isCompYes(uint16_t norm16) const {
	//     return norm16>=MIN_YES_YES_WITH_CC || norm16<minNoNo;
	// }
	// bool isCompYesOrMaybe(uint16_t norm16) const {
	//     return norm16<minNoNo || minMaybeYes<=norm16;
	// }
	// bool hasZeroCCFromDecompYes(uint16_t norm16) const {
	//     return norm16<=MIN_NORMAL_MAYBE_YES || norm16==JAMO_VT;
	// }
	bool isDecompYesAndZeroCC(uint16_t norm16) const 
	{
		return norm16<minYesNo || norm16==JAMO_VT || (minMaybeYes<=norm16 && norm16<=MIN_NORMAL_MAYBE_YES);
	}
	/**
	 * A little faster and simpler than isDecompYesAndZeroCC() but does not include
	 * the MaybeYes which combine-forward and have ccc=0.
	 * (Standard Unicode 10 normalization does not have such characters.)
	 */
	bool isMostDecompYesAndZeroCC(uint16_t norm16) const { return norm16<minYesNo || norm16==MIN_NORMAL_MAYBE_YES || norm16==JAMO_VT; }
	bool isDecompNoAlgorithmic(uint16_t norm16) const { return norm16>=limitNoNo; }
	// For use with isCompYes().
	// Perhaps the compiler can combine the two tests for MIN_YES_YES_WITH_CC.
	// static uint8 getCCFromYes(uint16_t norm16) {
	//     return norm16>=MIN_YES_YES_WITH_CC ? getCCFromNormalYesOrMaybe(norm16) : 0;
	// }
	uint8 getCCFromNoNo(uint16_t norm16) const {
		const uint16_t * mapping = getMapping(norm16);
		if(*mapping&MAPPING_HAS_CCC_LCCC_WORD) {
			return (uint8)*(mapping-1);
		}
		else {
			return 0;
		}
	}
	// requires that the [cpStart..cpLimit[ character passes isCompYesAndZeroCC()
	uint8 getTrailCCFromCompYesAndZeroCC(uint16_t norm16) const 
	{
		if(norm16<=minYesNo) {
			return 0; // yesYes and Hangul LV have ccc=tccc=0
		}
		else {
			// For Hangul LVT we harmlessly fetch a firstUnit with tccc=0 here.
			return (uint8)(*getMapping(norm16)>>8); // tccc from yesNo
		}
	}

	uint8 getPreviousTrailCC(const UChar * start, const UChar * p) const;
	uint8 getPreviousTrailCC(const uint8 * start, const uint8 * p) const;

	// Requires algorithmic-NoNo.
	UChar32 mapAlgorithmic(UChar32 c, uint16_t norm16) const { return c+(norm16>>DELTA_SHIFT)-centerNoNoDelta; }
	UChar32 getAlgorithmicDelta(uint16_t norm16) const { return (norm16>>DELTA_SHIFT)-centerNoNoDelta; }
	// Requires minYesNo<norm16<limitNoNo.
	const uint16_t * getMapping(uint16_t norm16) const { return extraData+(norm16>>OFFSET_SHIFT); }
	const uint16_t * getCompositionsListForDecompYes(uint16_t norm16) const 
	{
		if(norm16<JAMO_L || MIN_NORMAL_MAYBE_YES<=norm16) {
			return NULL;
		}
		else if(norm16<minMaybeYes) {
			return getMapping(norm16); // for yesYes; if Jamo L: harmless empty list
		}
		else {
			return maybeYesCompositions+norm16-minMaybeYes;
		}
	}
	const uint16_t * getCompositionsListForComposite(uint16_t norm16) const 
	{
		// A composite has both mapping & compositions list.
		const uint16_t * list = getMapping(norm16);
		return list+ // mapping pointer
		       1+ // +1 to skip the first unit with the mapping length
		       (*list&MAPPING_LENGTH_MASK); // + mapping length
	}
	const uint16_t * getCompositionsListForMaybe(uint16_t norm16) const 
	{
		// minMaybeYes<=norm16<MIN_NORMAL_MAYBE_YES
		return maybeYesCompositions+((norm16-minMaybeYes)>>OFFSET_SHIFT);
	}
	/**
	 * @param c code point must have compositions
	 * @return compositions list pointer
	 */
	const uint16_t * getCompositionsList(uint16_t norm16) const 
	{
		return isDecompYes(norm16) ? getCompositionsListForDecompYes(norm16) : getCompositionsListForComposite(norm16);
	}
	const UChar * copyLowPrefixFromNulTerminated(const UChar * src, UChar32 minNeedDataCP, ReorderingBuffer * buffer, UErrorCode & errorCode) const;

	enum StopAt { STOP_AT_LIMIT, STOP_AT_DECOMP_BOUNDARY, STOP_AT_COMP_BOUNDARY };

	const UChar * decomposeShort(const UChar * src, const UChar * limit,
	    bool stopAtCompBoundary, bool onlyContiguous, ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	bool decompose(UChar32 c, uint16_t norm16, ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	const uint8 * decomposeShort(const uint8 * src, const uint8 * limit, StopAt stopAt, bool onlyContiguous, ReorderingBuffer &buffer, UErrorCode & errorCode) const;
	static int32_t combine(const uint16_t * list, UChar32 trail);
	void addComposites(const uint16_t * list, UnicodeSet & set) const;
	void recompose(ReorderingBuffer &buffer, int32_t recomposeStartIndex, bool onlyContiguous) const;
	bool hasCompBoundaryBefore(UChar32 c, uint16_t norm16) const { return c<minCompNoMaybeCP || norm16HasCompBoundaryBefore(norm16); }
	bool norm16HasCompBoundaryBefore(uint16_t norm16) const { return norm16 < minNoNoCompNoMaybeCC || isAlgorithmicNoNo(norm16); }
	bool hasCompBoundaryBefore(const UChar * src, const UChar * limit) const;
	bool hasCompBoundaryBefore(const uint8 * src, const uint8 * limit) const;
	bool hasCompBoundaryAfter(const UChar * start, const UChar * p, bool onlyContiguous) const;
	bool hasCompBoundaryAfter(const uint8 * start, const uint8 * p, bool onlyContiguous) const;
	bool norm16HasCompBoundaryAfter(uint16_t norm16, bool onlyContiguous) const 
	{
		return (norm16 & HAS_COMP_BOUNDARY_AFTER) != 0 && (!onlyContiguous || isTrailCC01ForCompBoundaryAfter(norm16));
	}
	/** For FCC: Given norm16 HAS_COMP_BOUNDARY_AFTER, does it have tccc<=1? */
	bool isTrailCC01ForCompBoundaryAfter(uint16_t norm16) const 
	{
		return isInert(norm16) || (isDecompNoAlgorithmic(norm16) ? (norm16 & DELTA_TCCC_MASK) <= DELTA_TCCC_1 : *getMapping(norm16) <= 0x1ff);
	}
	const UChar * findPreviousCompBoundary(const UChar * start, const UChar * p, bool onlyContiguous) const;
	const UChar * findNextCompBoundary(const UChar * p, const UChar * limit, bool onlyContiguous) const;
	const UChar * findPreviousFCDBoundary(const UChar * start, const UChar * p) const;
	const UChar * findNextFCDBoundary(const UChar * p, const UChar * limit) const;
	void makeCanonIterDataFromNorm16(UChar32 start, UChar32 end, const uint16_t norm16, CanonIterData &newData, UErrorCode & errorCode) const;
	int32_t getCanonValue(UChar32 c) const;
	const UnicodeSet &getCanonStartSet(int32_t n) const;

	// UVersionInfo dataVersion;

	// BMP code point thresholds for quick check loops looking at single UTF-16 code units.
	UChar minDecompNoCP;
	UChar minCompNoMaybeCP;
	UChar minLcccCP;

	// Norm16 value thresholds for quick check combinations and types of extra data.
	uint16_t minYesNo;
	uint16_t minYesNoMappingsOnly;
	uint16_t minNoNo;
	uint16_t minNoNoCompBoundaryBefore;
	uint16_t minNoNoCompNoMaybeCC;
	uint16_t minNoNoEmpty;
	uint16_t limitNoNo;
	uint16_t centerNoNoDelta;
	uint16_t minMaybeYes;

	const UCPTrie * normTrie;
	const uint16_t * maybeYesCompositions;
	const uint16_t * extraData; // mappings and/or compositions for yesYes, yesNo & noNo characters
	const uint8 * smallFCD; // [0x100] one bit per 32 BMP code points, set if any FCD!=0

	UInitOnce fCanonIterDataInitOnce = U_INITONCE_INITIALIZER;
	CanonIterData  * fCanonIterData;
};

// bits in canonIterData
#define CANON_NOT_SEGMENT_STARTER 0x80000000
#define CANON_HAS_COMPOSITIONS 0x40000000
#define CANON_HAS_SET 0x200000
#define CANON_VALUE_MASK 0x1fffff

/**
 * ICU-internal shortcut for quick access to standard Unicode normalization.
 */
class U_COMMON_API Normalizer2Factory {
public:
	static const Normalizer2 * getFCDInstance(UErrorCode & errorCode);
	static const Normalizer2 * getFCCInstance(UErrorCode & errorCode);
	static const Normalizer2 * getNoopInstance(UErrorCode & errorCode);
	static const Normalizer2 * getInstance(UNormalizationMode mode, UErrorCode & errorCode);
	static const Normalizer2Impl * getNFCImpl(UErrorCode & errorCode);
	static const Normalizer2Impl * getNFKCImpl(UErrorCode & errorCode);
	static const Normalizer2Impl * getNFKC_CFImpl(UErrorCode & errorCode);
	// Get the Impl instance of the Normalizer2.
	// Must be used only when it is known that norm2 is a Normalizer2WithImpl instance.
	static const Normalizer2Impl * getImpl(const Normalizer2 * norm2);
private:
	Normalizer2Factory(); // No instantiation.
};

U_NAMESPACE_END

U_CAPI int32_t U_EXPORT2 unorm2_swap(const UDataSwapper * ds, const void * inData, int32_t length, void * outData, UErrorCode * pErrorCode);

/**
 * Get the NF*_QC property for a code point, for u_getIntPropertyValue().
 * @internal
 */
U_CFUNC UNormalizationCheckResult unorm_getQuickCheck(UChar32 c, UNormalizationMode mode);

/**
 * Gets the 16-bit FCD value (lead & trail CCs) for a code point, for u_getIntPropertyValue().
 * @internal
 */
U_CFUNC uint16_t unorm_getFCD16(UChar32 c);

/**
 * Format of Normalizer2 .nrm data files.
 * Format version 4.0.
 *
 * Normalizer2 .nrm data files provide data for the Unicode Normalization algorithms.
 * ICU ships with data files for standard Unicode Normalization Forms
 * NFC and NFD (nfc.nrm), NFKC and NFKD (nfkc.nrm) and NFKC_Casefold (nfkc_cf.nrm).
 * Custom (application-specific) data can be built into additional .nrm files
 * with the gennorm2 build tool.
 * ICU ships with one such file, uts46.nrm, for the implementation of UTS #46.
 *
 * Normalizer2.getInstance() causes a .nrm file to be loaded, unless it has been
 * cached already. Internally, Normalizer2Impl.load() reads the .nrm file.
 *
 * A .nrm file begins with a standard ICU data file header
 * (DataHeader, see ucmndata.h and unicode/udata.h).
 * The UDataInfo.dataVersion field usually contains the Unicode version
 * for which the data was generated.
 *
 * After the header, the file contains the following parts.
 * Constants are defined as enum values of the Normalizer2Impl class.
 *
 * Many details of the data structures are described in the design doc
 * which is at http://site.icu-project.org/design/normalization/custom
 *
 * int32_t indexes[indexesLength]; -- indexesLength=indexes[IX_NORM_TRIE_OFFSET]/4;
 *
 *      The first eight indexes are byte offsets in ascending order.
 *      Each byte offset marks the start of the next part in the data file,
 *      and the end of the previous one.
 *      When two consecutive byte offsets are the same, then the corresponding part is empty.
 *      Byte offsets are offsets from after the header,
 *      that is, from the beginning of the indexes[].
 *      Each part starts at an offset with proper alignment for its data.
 *      If necessary, the previous part may include padding bytes to achieve this alignment.
 *
 *      minDecompNoCP=indexes[IX_MIN_DECOMP_NO_CP] is the lowest code point
 *      with a decomposition mapping, that is, with NF*D_QC=No.
 *      minCompNoMaybeCP=indexes[IX_MIN_COMP_NO_MAYBE_CP] is the lowest code point
 *      with NF*C_QC=No (has a one-way mapping) or Maybe (combines backward).
 *      minLcccCP=indexes[IX_MIN_LCCC_CP] (index 18, new in formatVersion 3)
 *      is the lowest code point with lccc!=0.
 *
 *      The next eight indexes are thresholds of 16-bit trie values for ranges of
 *      values indicating multiple normalization properties.
 *      They are listed here in threshold order, not in the order they are stored in the indexes.
 *          minYesNo=indexes[IX_MIN_YES_NO];
 *          minYesNoMappingsOnly=indexes[IX_MIN_YES_NO_MAPPINGS_ONLY];
 *          minNoNo=indexes[IX_MIN_NO_NO];
 *          minNoNoCompBoundaryBefore=indexes[IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE];
 *          minNoNoCompNoMaybeCC=indexes[IX_MIN_NO_NO_COMP_NO_MAYBE_CC];
 *          minNoNoEmpty=indexes[IX_MIN_NO_NO_EMPTY];
 *          limitNoNo=indexes[IX_LIMIT_NO_NO];
 *          minMaybeYes=indexes[IX_MIN_MAYBE_YES];
 *      See the normTrie description below and the design doc for details.
 *
 * UCPTrie normTrie; -- see ucptrie_impl.h and ucptrie.h, same as Java CodePointTrie
 *
 *      The trie holds the main normalization data. Each code point is mapped to a 16-bit value.
 *      Rather than using independent bits in the value (which would require more than 16 bits),
 *      information is extracted primarily via range checks.
 *      Except, format version 3 uses bit 0 for hasCompBoundaryAfter().
 *      For example, a 16-bit value norm16 in the range minYesNo<=norm16<minNoNo
 *      means that the character has NF*C_QC=Yes and NF*D_QC=No properties,
 *      which means it has a two-way (round-trip) decomposition mapping.
 *      Values in the range 2<=norm16<limitNoNo are also directly indexes into the extraData
 *      pointing to mappings, compositions lists, or both.
 *      Value norm16==INERT (0 in versions 1 & 2, 1 in version 3)
 *      means that the character is normalization-inert, that is,
 *      it does not have a mapping, does not participate in composition, has a zero
 *      canonical combining class, and forms a boundary where text before it and after it
 *      can be normalized independently.
 *      For details about how multiple properties are encoded in 16-bit values
 *      see the design doc.
 *      Note that the encoding cannot express all combinations of the properties involved;
 *      it only supports those combinations that are allowed by
 *      the Unicode Normalization algorithms. Details are in the design doc as well.
 *      The gennorm2 tool only builds .nrm files for data that conforms to the limitations.
 *
 *      The trie has a value for each lead surrogate code unit representing the "worst case"
 *      properties of the 1024 supplementary characters whose UTF-16 form starts with
 *      the lead surrogate. If all of the 1024 supplementary characters are normalization-inert,
 *      then their lead surrogate code unit has the trie value INERT.
 *      When the lead surrogate unit's value exceeds the quick check minimum during processing,
 *      the properties for the full supplementary code point need to be looked up.
 *
 * uint16_t maybeYesCompositions[MIN_NORMAL_MAYBE_YES-minMaybeYes];
 * uint16_t extraData[];
 *
 *      There is only one byte offset for the end of these two arrays.
 *      The split between them is given by the constant and variable mentioned above.
 *      In version 3, the difference must be shifted right by OFFSET_SHIFT.
 *
 *      The maybeYesCompositions array contains compositions lists for characters that
 *      combine both forward (as starters in composition pairs)
 *      and backward (as trailing characters in composition pairs).
 *      Such characters do not occur in Unicode 5.2 but are allowed by
 *      the Unicode Normalization algorithms.
 *      If there are no such characters, then minMaybeYes==MIN_NORMAL_MAYBE_YES
 *      and the maybeYesCompositions array is empty.
 *      If there are such characters, then minMaybeYes is subtracted from their norm16 values
 *      to get the index into this array.
 *
 *      The extraData array contains compositions lists for "YesYes" characters,
 *      followed by mappings and optional compositions lists for "YesNo" characters,
 *      followed by only mappings for "NoNo" characters.
 *      (Referring to pairs of NFC/NFD quick check values.)
 *      The norm16 values of those characters are directly indexes into the extraData array.
 *      In version 3, the norm16 values must be shifted right by OFFSET_SHIFT
 *      for accessing extraData.
 *
 *      The data structures for compositions lists and mappings are described in the design doc.
 *
 * uint8 smallFCD[0x100]; -- new in format version 2
 *
 *      This is a bit set to help speed up FCD value lookups in the absence of a full
 *      UTrie2 or other large data structure with the full FCD value mapping.
 *
 *      Each smallFCD bit is set if any of the corresponding 32 BMP code points
 *      has a non-zero FCD value (lccc!=0 or tccc!=0).
 *      Bit 0 of smallFCD[0] is for U+0000..U+001F. Bit 7 of smallFCD[0xff] is for U+FFE0..U+FFFF.
 *      A bit for 32 lead surrogates is set if any of the 32k corresponding
 *      _supplementary_ code points has a non-zero FCD value.
 *
 *      This bit set is most useful for the large blocks of CJK characters with FCD=0.
 *
 * Changes from format version 1 to format version 2 ---------------------------
 *
 * - Addition of data for raw (not recursively decomposed) mappings.
 *   + The MAPPING_NO_COMP_BOUNDARY_AFTER bit in the extraData is now also set when
 *     the mapping is to an empty string or when the character combines-forward.
 *     This subsumes the one actual use of the MAPPING_PLUS_COMPOSITION_LIST bit which
 *     is then repurposed for the MAPPING_HAS_RAW_MAPPING bit.
 *   + For details see the design doc.
 * - Addition of indexes[IX_MIN_YES_NO_MAPPINGS_ONLY] and separation of the yesNo extraData into
 *   distinct ranges (combines-forward vs. not)
 *   so that a range check can be used to find out if there is a compositions list.
 *   This is fully equivalent with formatVersion 1's MAPPING_PLUS_COMPOSITION_LIST flag.
 *   It is needed for the new (in ICU 49) composePair(), not for other normalization.
 * - Addition of the smallFCD[] bit set.
 *
 * Changes from format version 2 to format version 3 (ICU 60) ------------------
 *
 * - norm16 bit 0 indicates hasCompBoundaryAfter(),
 *   except that for contiguous composition (FCC) the tccc must be checked as well.
 *   Data indexes and ccc values are shifted left by one (OFFSET_SHIFT).
 *   Thresholds like minNoNo are tested before shifting.
 *
 * - Algorithmic mapping deltas are shifted left by two more bits (total DELTA_SHIFT),
 *   to make room for two bits (three values) indicating whether the tccc is 0, 1, or greater.
 *   See DELTA_TCCC_MASK etc.
 *   This helps with fetching tccc/FCD values and FCC hasCompBoundaryAfter().
 *   minMaybeYes is 8-aligned so that the DELTA_TCCC_MASK bits can be tested directly.
 *
 * - Algorithmic mappings are only used for mapping to "comp yes and ccc=0" characters,
 *   and ASCII characters are mapped algorithmically only to other ASCII characters.
 *   This helps with hasCompBoundaryBefore() and compose() fast paths.
 *   It is never necessary any more to loop for algorithmic mappings.
 *
 * - Addition of indexes[IX_MIN_NO_NO_COMP_BOUNDARY_BEFORE],
 *   indexes[IX_MIN_NO_NO_COMP_NO_MAYBE_CC], and indexes[IX_MIN_NO_NO_EMPTY],
 *   and separation of the noNo extraData into distinct ranges.
 *   With this, the noNo norm16 value indicates whether the mapping is
 *   compose-normalized, not normalized but hasCompBoundaryBefore(),
 *   not even that, or maps to an empty string.
 *   hasCompBoundaryBefore() can be determined solely from the norm16 value.
 *
 * - The norm16 value for Hangul LVT is now different from that for Hangul LV,
 *   so that hasCompBoundaryAfter() need not check for the syllable type.
 *   For Hangul LV, minYesNo continues to be used (no comp-boundary-after).
 *   For Hangul LVT, minYesNoMappingsOnly|HAS_COMP_BOUNDARY_AFTER is used.
 *   The extraData units at these indexes are set to firstUnit=2 and firstUnit=3, respectively,
 *   to simplify some code.
 *
 * - The extraData firstUnit bit 5 is no longer necessary
 *   (norm16 bit 0 used instead of firstUnit MAPPING_NO_COMP_BOUNDARY_AFTER),
 *   is reserved again, and always set to 0.
 *
 * - Addition of indexes[IX_MIN_LCCC_CP], the first code point where lccc!=0.
 *   This used to be hardcoded to U+0300, but in data like NFKC_Casefold it is lower:
 *   U+00AD Soft Hyphen maps to an empty string,
 *   which is artificially assigned "worst case" values lccc=1 and tccc=255.
 *
 * - A mapping to an empty string has explicit lccc=1 and tccc=255 values.
 *
 * Changes from format version 3 to format version 4 (ICU 63) ------------------
 *
 * Switched from UTrie2 to UCPTrie/CodePointTrie.
 *
 * The new trie no longer stores different values for surrogate code *units* vs.
 * surrogate code *points*.
 * Lead surrogates still have values for optimized UTF-16 string processing.
 * When looking up code point properties, the code now checks for lead surrogates and
 * treats them as inert.
 *
 * gennorm2 now has to reject mappings for surrogate code points.
 * UTS #46 maps unpaired surrogates to U+FFFD in code rather than via its
 * custom normalization data file.
 */

#endif  /* !UCONFIG_NO_NORMALIZATION */
#endif  /* __NORMALIZER2IMPL_H__ */
