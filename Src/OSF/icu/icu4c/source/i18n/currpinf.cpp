// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2009-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */
#include <icu-internal.h>
#pragma hdrstop
#include "unicode/currpinf.h"

#if !UCONFIG_NO_FORMATTING

//#define CURRENCY_PLURAL_INFO_DEBUG 1

#include "unicode/numsys.h"
#include "uresimp.h"
#include "ureslocs.h"

U_NAMESPACE_BEGIN

static const UChar gNumberPatternSeparator = 0x3B; // ;

U_CDECL_BEGIN

/**
 * @internal ICU 4.2
 */
static bool U_CALLCONV ValueComparator(UHashTok val1, UHashTok val2);

bool U_CALLCONV ValueComparator(UHashTok val1, UHashTok val2) 
{
	const UnicodeString * affix_1 = static_cast<const UnicodeString *>(val1.pointer);
	const UnicodeString * affix_2 = static_cast<const UnicodeString *>(val2.pointer);
	return *affix_1 == *affix_2;
}

U_CDECL_END UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CurrencyPluralInfo)

static const UChar gDefaultCurrencyPluralPattern[] = { '0', '.', '#', '#', ' ', 0xA4, 0xA4, 0xA4, 0 };
static const UChar gTripleCurrencySign[] = {0xA4, 0xA4, 0xA4, 0};
static const UChar gPluralCountOther[] = {0x6F, 0x74, 0x68, 0x65, 0x72, 0};
static const UChar gPart0[] = {0x7B, 0x30, 0x7D, 0};
static const UChar gPart1[] = {0x7B, 0x31, 0x7D, 0};
static const char gNumberElementsTag[] = "NumberElements";
static const char gLatnTag[] = "latn";
static const char gPatternsTag[] = "patterns";
static const char gDecimalFormatTag[] = "decimalFormat";
static const char gCurrUnitPtnTag[] = "CurrencyUnitPatterns";

CurrencyPluralInfo::CurrencyPluralInfo(UErrorCode & status) :   fPluralCountToCurrencyUnitPattern(nullptr),
	fPluralRules(nullptr), fLocale(nullptr), fInternalStatus(U_ZERO_ERROR) 
{
	initialize(Locale::getDefault(), status);
}

CurrencyPluralInfo::CurrencyPluralInfo(const Locale & locale, UErrorCode & status) :   fPluralCountToCurrencyUnitPattern(nullptr),
	fPluralRules(nullptr), fLocale(nullptr), fInternalStatus(U_ZERO_ERROR) 
{
	initialize(locale, status);
}

CurrencyPluralInfo::CurrencyPluralInfo(const CurrencyPluralInfo& info) :   UObject(info), fPluralCountToCurrencyUnitPattern(nullptr),
	fPluralRules(nullptr), fLocale(nullptr), fInternalStatus(U_ZERO_ERROR) 
{
	*this = info;
}

CurrencyPluralInfo&CurrencyPluralInfo::operator = (const CurrencyPluralInfo& info) 
{
	if(this == &info) {
		return *this;
	}
	fInternalStatus = info.fInternalStatus;
	if(U_FAILURE(fInternalStatus)) {
		// bail out early if the object we were copying from was already 'invalid'.
		return *this;
	}
	deleteHash(fPluralCountToCurrencyUnitPattern);
	fPluralCountToCurrencyUnitPattern = initHash(fInternalStatus);
	copyHash(info.fPluralCountToCurrencyUnitPattern, fPluralCountToCurrencyUnitPattern, fInternalStatus);
	if(U_FAILURE(fInternalStatus)) {
		return *this;
	}
	ZDELETE(fPluralRules);
	ZDELETE(fLocale);
	if(info.fPluralRules != nullptr) {
		fPluralRules = info.fPluralRules->clone();
		if(fPluralRules == nullptr) {
			fInternalStatus = U_MEMORY_ALLOCATION_ERROR;
			return *this;
		}
	}
	if(info.fLocale != nullptr) {
		fLocale = info.fLocale->clone();
		if(fLocale == nullptr) {
			// Note: If clone had an error parameter, then we could check/set that instead.
			fInternalStatus = U_MEMORY_ALLOCATION_ERROR;
			return *this;
		}
		// If the other locale wasn't bogus, but our clone'd locale is bogus, then OOM happened
		// during the call to clone().
		if(!info.fLocale->isBogus() && fLocale->isBogus()) {
			fInternalStatus = U_MEMORY_ALLOCATION_ERROR;
			return *this;
		}
	}
	return *this;
}

CurrencyPluralInfo::~CurrencyPluralInfo() 
{
	deleteHash(fPluralCountToCurrencyUnitPattern);
	fPluralCountToCurrencyUnitPattern = nullptr;
	ZDELETE(fPluralRules);
	ZDELETE(fLocale);
}

bool CurrencyPluralInfo::operator == (const CurrencyPluralInfo& info) const 
{
#ifdef CURRENCY_PLURAL_INFO_DEBUG
	if(*fPluralRules == *info.fPluralRules) {
		std::cout << "same plural rules\n";
	}
	if(*fLocale == *info.fLocale) {
		std::cout << "same locale\n";
	}
	if(fPluralCountToCurrencyUnitPattern->equals(*info.fPluralCountToCurrencyUnitPattern)) {
		std::cout << "same pattern\n";
	}
#endif
	return *fPluralRules == *info.fPluralRules && *fLocale == *info.fLocale && fPluralCountToCurrencyUnitPattern->equals(*info.fPluralCountToCurrencyUnitPattern);
}

CurrencyPluralInfo* CurrencyPluralInfo::clone() const 
{
	CurrencyPluralInfo * newObj = new CurrencyPluralInfo(*this);
	// Since clone doesn't have a 'status' parameter, the best we can do is return nullptr
	// if the new object was not full constructed properly (an error occurred).
	if(newObj && U_FAILURE(newObj->fInternalStatus)) {
		ZDELETE(newObj);
	}
	return newObj;
}

const PluralRules* CurrencyPluralInfo::getPluralRules() const { return fPluralRules; }

UnicodeString &CurrencyPluralInfo::getCurrencyPluralPattern(const UnicodeString & pluralCount, UnicodeString & result) const 
{
	const UnicodeString * currencyPluralPattern = (UnicodeString *)fPluralCountToCurrencyUnitPattern->get(pluralCount);
	if(currencyPluralPattern == nullptr) {
		// fall back to "other"
		if(pluralCount.compare(gPluralCountOther, 5)) {
			currencyPluralPattern = (UnicodeString *)fPluralCountToCurrencyUnitPattern->get(UnicodeString(TRUE, gPluralCountOther, 5));
		}
		if(currencyPluralPattern == nullptr) {
			// no currencyUnitPatterns defined,
			// fallback to predefined default.
			// This should never happen when ICU resource files are
			// available, since currencyUnitPattern of "other" is always
			// defined in root.
			result = UnicodeString(gDefaultCurrencyPluralPattern);
			return result;
		}
	}
	result = *currencyPluralPattern;
	return result;
}

const Locale&CurrencyPluralInfo::getLocale() const { return *fLocale; }

void CurrencyPluralInfo::setPluralRules(const UnicodeString & ruleDescription, UErrorCode & status) 
{
	if(U_SUCCESS(status)) {
		delete fPluralRules;
		fPluralRules = PluralRules::createRules(ruleDescription, status);
	}
}

void CurrencyPluralInfo::setCurrencyPluralPattern(const UnicodeString & pluralCount, const UnicodeString & pattern, UErrorCode & status) 
{
	if(U_SUCCESS(status)) {
		UnicodeString * oldValue = static_cast<UnicodeString *>(fPluralCountToCurrencyUnitPattern->get(pluralCount));
		delete oldValue;
		LocalPointer<UnicodeString> p(new UnicodeString(pattern), status);
		if(U_SUCCESS(status)) {
			// the p object allocated above will be owned by fPluralCountToCurrencyUnitPattern
			// after the call to put(), even if the method returns failure.
			fPluralCountToCurrencyUnitPattern->put(pluralCount, p.orphan(), status);
		}
	}
}

void CurrencyPluralInfo::setLocale(const Locale & loc, UErrorCode & status) 
{
	initialize(loc, status);
}

void CurrencyPluralInfo::initialize(const Locale & loc, UErrorCode & status) 
{
	if(U_FAILURE(status)) {
		return;
	}
	ZDELETE(fLocale);
	ZDELETE(fPluralRules);
	fLocale = loc.clone();
	if(fLocale == nullptr) {
		status = U_MEMORY_ALLOCATION_ERROR;
		return;
	}
	// If the locale passed in wasn't bogus, but our clone'd locale is bogus, then OOM happened
	// during the call to loc.clone().
	if(!loc.isBogus() && fLocale->isBogus()) {
		status = U_MEMORY_ALLOCATION_ERROR;
		return;
	}
	fPluralRules = PluralRules::forLocale(loc, status);
	setupCurrencyPluralPattern(loc, status);
}

void CurrencyPluralInfo::setupCurrencyPluralPattern(const Locale & loc, UErrorCode & status) 
{
	if(U_FAILURE(status)) {
		return;
	}
	deleteHash(fPluralCountToCurrencyUnitPattern);
	fPluralCountToCurrencyUnitPattern = initHash(status);
	if(U_FAILURE(status)) {
		return;
	}
	LocalPointer<NumberingSystem> ns(NumberingSystem::createInstance(loc, status), status);
	if(U_FAILURE(status)) {
		return;
	}
	UErrorCode ec = U_ZERO_ERROR;
	LocalUResourceBundlePointer rb(ures_open(nullptr, loc.getName(), &ec));
	LocalUResourceBundlePointer numElements(ures_getByKeyWithFallback(rb.getAlias(), gNumberElementsTag, nullptr, &ec));
	ures_getByKeyWithFallback(numElements.getAlias(), ns->getName(), rb.getAlias(), &ec);
	ures_getByKeyWithFallback(rb.getAlias(), gPatternsTag, rb.getAlias(), &ec);
	int32_t ptnLen;
	const UChar * numberStylePattern = ures_getStringByKeyWithFallback(rb.getAlias(), gDecimalFormatTag, &ptnLen, &ec);
	// Fall back to "latn" if num sys specific pattern isn't there.
	if(ec == U_MISSING_RESOURCE_ERROR && (uprv_strcmp(ns->getName(), gLatnTag) != 0)) {
		ec = U_ZERO_ERROR;
		ures_getByKeyWithFallback(numElements.getAlias(), gLatnTag, rb.getAlias(), &ec);
		ures_getByKeyWithFallback(rb.getAlias(), gPatternsTag, rb.getAlias(), &ec);
		numberStylePattern = ures_getStringByKeyWithFallback(rb.getAlias(), gDecimalFormatTag, &ptnLen, &ec);
	}
	int32_t numberStylePatternLen = ptnLen;
	const UChar * negNumberStylePattern = nullptr;
	int32_t negNumberStylePatternLen = 0;
	// TODO: Java
	// parse to check whether there is ";" separator in the numberStylePattern
	bool hasSeparator = false;
	if(U_SUCCESS(ec)) {
		for(int32_t styleCharIndex = 0; styleCharIndex < ptnLen; ++styleCharIndex) {
			if(numberStylePattern[styleCharIndex] == gNumberPatternSeparator) {
				hasSeparator = true;
				// split the number style pattern into positive and negative
				negNumberStylePattern = numberStylePattern + styleCharIndex + 1;
				negNumberStylePatternLen = ptnLen - styleCharIndex - 1;
				numberStylePatternLen = styleCharIndex;
			}
		}
	}
	if(U_FAILURE(ec)) {
		// If OOM occurred during the above code, then we want to report that back to the caller.
		if(ec == U_MEMORY_ALLOCATION_ERROR) {
			status = ec;
		}
		return;
	}
	LocalUResourceBundlePointer currRb(ures_open(U_ICUDATA_CURR, loc.getName(), &ec));
	LocalUResourceBundlePointer currencyRes(ures_getByKeyWithFallback(currRb.getAlias(), gCurrUnitPtnTag, nullptr, &ec));
#ifdef CURRENCY_PLURAL_INFO_DEBUG
	std::cout << "in set up\n";
#endif
	LocalPointer<StringEnumeration> keywords(fPluralRules->getKeywords(ec), ec);
	if(U_SUCCESS(ec)) {
		const char * pluralCount;
		while(((pluralCount = keywords->next(nullptr, ec)) != nullptr) && U_SUCCESS(ec)) {
			int32_t ptnLength;
			UErrorCode err = U_ZERO_ERROR;
			const UChar * patternChars = ures_getStringByKeyWithFallback(currencyRes.getAlias(), pluralCount, &ptnLength, &err);
			if(err == U_MEMORY_ALLOCATION_ERROR || patternChars == nullptr) {
				ec = err;
				break;
			}
			if(U_SUCCESS(err) && ptnLength > 0) {
				UnicodeString * pattern = new UnicodeString(patternChars, ptnLength);
				if(pattern == nullptr) {
					ec = U_MEMORY_ALLOCATION_ERROR;
					break;
				}
#ifdef CURRENCY_PLURAL_INFO_DEBUG
				char result_1[1000];
				pattern->extract(0, pattern->length(), result_1, "UTF-8");
				std::cout << "pluralCount: " << pluralCount << "; pattern: " << result_1 << "\n";
#endif
				pattern->findAndReplace(UnicodeString(TRUE, gPart0, 3), UnicodeString(numberStylePattern, numberStylePatternLen));
				pattern->findAndReplace(UnicodeString(TRUE, gPart1, 3), UnicodeString(TRUE, gTripleCurrencySign, 3));
				if(hasSeparator) {
					UnicodeString negPattern(patternChars, ptnLength);
					negPattern.findAndReplace(UnicodeString(TRUE, gPart0, 3), UnicodeString(negNumberStylePattern, negNumberStylePatternLen));
					negPattern.findAndReplace(UnicodeString(TRUE, gPart1, 3), UnicodeString(TRUE, gTripleCurrencySign, 3));
					pattern->append(gNumberPatternSeparator);
					pattern->append(negPattern);
				}
#ifdef CURRENCY_PLURAL_INFO_DEBUG
				pattern->extract(0, pattern->length(), result_1, "UTF-8");
				std::cout << "pluralCount: " << pluralCount << "; pattern: " << result_1 << "\n";
#endif
				// the 'pattern' object allocated above will be owned by the
				// fPluralCountToCurrencyUnitPattern after the call to
				// put(), even if the method returns failure.
				fPluralCountToCurrencyUnitPattern->put(UnicodeString(pluralCount, -1, US_INV), pattern, status);
			}
		}
	}
	// If OOM occurred during the above code, then we want to report that back to the caller.
	if(ec == U_MEMORY_ALLOCATION_ERROR) {
		status = ec;
	}
}

void CurrencyPluralInfo::deleteHash(Hashtable* hTable) 
{
	if(hTable) {
		int32_t pos = UHASH_FIRST;
		const UHashElement* element = nullptr;
		while((element = hTable->nextElement(pos)) != nullptr) {
			const UHashTok valueTok = element->value;
			const UnicodeString * value = (UnicodeString *)valueTok.pointer;
			delete value;
		}
		ZDELETE(hTable);
	}
}

Hashtable* CurrencyPluralInfo::initHash(UErrorCode & status) 
{
	if(U_FAILURE(status)) {
		return nullptr;
	}
	LocalPointer<Hashtable> hTable(new Hashtable(TRUE, status), status);
	if(U_FAILURE(status)) {
		return nullptr;
	}
	hTable->setValueComparator(ValueComparator);
	return hTable.orphan();
}

void CurrencyPluralInfo::copyHash(const Hashtable* source, Hashtable* target, UErrorCode & status) 
{
	if(U_FAILURE(status)) {
		return;
	}
	int32_t pos = UHASH_FIRST;
	const UHashElement* element = nullptr;
	if(source) {
		while((element = source->nextElement(pos)) != nullptr) {
			const UHashTok keyTok = element->key;
			const UnicodeString * key = (UnicodeString *)keyTok.pointer;
			const UHashTok valueTok = element->value;
			const UnicodeString * value = (UnicodeString *)valueTok.pointer;
			LocalPointer<UnicodeString> copy(new UnicodeString(*value), status);
			if(U_FAILURE(status)) {
				return;
			}
			// The HashTable owns the 'copy' object after the call to put().
			target->put(UnicodeString(*key), copy.orphan(), status);
			if(U_FAILURE(status)) {
				return;
			}
		}
	}
}

U_NAMESPACE_END

#endif
