// DAYPERIODRULES.CPP
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2016, International Business Machines Corporation and others.  All Rights Reserved.
// created on: 2016-01-20
// created by: kazede
//
#include <icu-internal.h>
#pragma hdrstop
#include "dayperiodrules.h"
#include "ucln_in.h"
#include "uresimp.h"

U_NAMESPACE_BEGIN

namespace {
struct DayPeriodRulesData : public UMemory {
	DayPeriodRulesData() : localeToRuleSetNumMap(NULL), rules(NULL), maxRuleSetNum(0) 
	{
	}
	UHashtable * localeToRuleSetNumMap;
	DayPeriodRules * rules;
	int32_t maxRuleSetNum;
} *data = NULL;

enum CutoffType {
	CUTOFF_TYPE_UNKNOWN = -1,
	CUTOFF_TYPE_BEFORE,
	CUTOFF_TYPE_AFTER, // TODO: AFTER is deprecated in CLDR 29. Remove.
	CUTOFF_TYPE_FROM,
	CUTOFF_TYPE_AT
};
} // namespace

struct DayPeriodRulesDataSink : public ResourceSink {
	DayPeriodRulesDataSink() {
		for(int32_t i = 0; i < UPRV_LENGTHOF(cutoffs); ++i) {
			cutoffs[i] = 0;
		}
	}

	virtual ~DayPeriodRulesDataSink();

	virtual void put(const char * key, ResourceValue &value, bool, UErrorCode & errorCode) override {
		ResourceTable dayPeriodData = value.getTable(errorCode);
		if(U_FAILURE(errorCode)) {
			return;
		}

		for(int32_t i = 0; dayPeriodData.getKeyAndValue(i, key, value); ++i) {
			if(uprv_strcmp(key, "locales") == 0) {
				ResourceTable locales = value.getTable(errorCode);
				if(U_FAILURE(errorCode)) {
					return;
				}

				for(int32_t j = 0; locales.getKeyAndValue(j, key, value); ++j) {
					UnicodeString setNum_str = value.getUnicodeString(errorCode);
					int32_t setNum = parseSetNum(setNum_str, errorCode);
					uhash_puti(data->localeToRuleSetNumMap, const_cast<char *>(key), setNum, &errorCode);
				}
			}
			else if(uprv_strcmp(key, "rules") == 0) {
				// Allocate one more than needed to skip [0]. See comment in parseSetNum().
				data->rules = new DayPeriodRules[data->maxRuleSetNum + 1];
				if(data->rules == NULL) {
					errorCode = U_MEMORY_ALLOCATION_ERROR;
					return;
				}
				ResourceTable rules = value.getTable(errorCode);
				processRules(rules, key, value, errorCode);
				if(U_FAILURE(errorCode)) {
					return;
				}
			}
		}
	}

	void processRules(const ResourceTable &rules, const char * key,
	    ResourceValue &value, UErrorCode & errorCode) {
		if(U_FAILURE(errorCode)) {
			return;
		}

		for(int32_t i = 0; rules.getKeyAndValue(i, key, value); ++i) {
			ruleSetNum = parseSetNum(key, errorCode);
			ResourceTable ruleSet = value.getTable(errorCode);
			if(U_FAILURE(errorCode)) {
				return;
			}

			for(int32_t j = 0; ruleSet.getKeyAndValue(j, key, value); ++j) {
				period = DayPeriodRules::getDayPeriodFromString(key);
				if(period == DayPeriodRules::DAYPERIOD_UNKNOWN) {
					errorCode = U_INVALID_FORMAT_ERROR;
					return;
				}
				ResourceTable periodDefinition = value.getTable(errorCode);
				if(U_FAILURE(errorCode)) {
					return;
				}

				for(int32_t k = 0; periodDefinition.getKeyAndValue(k, key, value); ++k) {
					if(value.getType() == URES_STRING) {
						// Key-value pairs (e.g. before{6:00}).
						CutoffType type = getCutoffTypeFromString(key);
						addCutoff(type, value.getUnicodeString(errorCode), errorCode);
						if(U_FAILURE(errorCode)) {
							return;
						}
					}
					else {
						// Arrays (e.g. before{6:00, 24:00}).
						cutoffType = getCutoffTypeFromString(key);
						ResourceArray cutoffArray = value.getArray(errorCode);
						if(U_FAILURE(errorCode)) {
							return;
						}

						int32_t length = cutoffArray.getSize();
						for(int32_t l = 0; l < length; ++l) {
							cutoffArray.getValue(l, value);
							addCutoff(cutoffType, value.getUnicodeString(errorCode), errorCode);
							if(U_FAILURE(errorCode)) {
								return;
							}
						}
					}
				}
				setDayPeriodForHoursFromCutoffs(errorCode);
				for(int32_t k = 0; k < UPRV_LENGTHOF(cutoffs); ++k) {
					cutoffs[k] = 0;
				}
			}

			if(!data->rules[ruleSetNum].allHoursAreSet()) {
				errorCode = U_INVALID_FORMAT_ERROR;
				return;
			}
		}
	}

	// Members.
	int32_t cutoffs[25]; // [0] thru [24]: 24 is allowed in "before 24".

	// "Path" to data.
	int32_t ruleSetNum;
	DayPeriodRules::DayPeriod period;
	CutoffType cutoffType;

	// Helpers.
	static int32_t parseSetNum(const UnicodeString & setNumStr, UErrorCode & errorCode) {
		CharString cs;
		cs.appendInvariantChars(setNumStr, errorCode);
		return parseSetNum(cs.data(), errorCode);
	}

	static int32_t parseSetNum(const char * setNumStr, UErrorCode & errorCode) {
		if(U_FAILURE(errorCode)) {
			return -1;
		}

		if(uprv_strncmp(setNumStr, "set", 3) != 0) {
			errorCode = U_INVALID_FORMAT_ERROR;
			return -1;
		}

		int32_t i = 3;
		int32_t setNum = 0;
		while(setNumStr[i] != 0) {
			int32_t digit = setNumStr[i] - '0';
			if(digit < 0 || 9 < digit) {
				errorCode = U_INVALID_FORMAT_ERROR;
				return -1;
			}
			setNum = 10 * setNum + digit;
			++i;
		}

		// Rule set number must not be zero. (0 is used to indicate "not found" by hashmap.)
		// Currently ICU data conveniently starts numbering rule sets from 1.
		if(setNum == 0) {
			errorCode = U_INVALID_FORMAT_ERROR;
			return -1;
		}
		else {
			return setNum;
		}
	}

	void addCutoff(CutoffType type, const UnicodeString & hour_str, UErrorCode & errorCode) {
		if(U_FAILURE(errorCode)) {
			return;
		}

		if(type == CUTOFF_TYPE_UNKNOWN) {
			errorCode = U_INVALID_FORMAT_ERROR;
			return;
		}

		int32_t hour = parseHour(hour_str, errorCode);
		if(U_FAILURE(errorCode)) {
			return;
		}

		cutoffs[hour] |= 1 << type;
	}

	// Translate the cutoffs[] array to day period rules.
	void setDayPeriodForHoursFromCutoffs(UErrorCode & errorCode) {
		DayPeriodRules &rule = data->rules[ruleSetNum];

		for(int32_t startHour = 0; startHour <= 24; ++startHour) {
			// AT cutoffs must be either midnight or noon.
			if(cutoffs[startHour] & (1 << CUTOFF_TYPE_AT)) {
				if(startHour == 0 && period == DayPeriodRules::DAYPERIOD_MIDNIGHT) {
					rule.fHasMidnight = TRUE;
				}
				else if(startHour == 12 && period == DayPeriodRules::DAYPERIOD_NOON) {
					rule.fHasNoon = TRUE;
				}
				else {
					errorCode = U_INVALID_FORMAT_ERROR; // Bad data.
					return;
				}
			}

			// FROM/AFTER and BEFORE must come in a pair.
			if(cutoffs[startHour] & (1 << CUTOFF_TYPE_FROM) ||
			    cutoffs[startHour] & (1 << CUTOFF_TYPE_AFTER)) {
				for(int32_t hour = startHour + 1;; ++hour) {
					if(hour == startHour) {
						// We've gone around the array once and can't find a BEFORE.
						errorCode = U_INVALID_FORMAT_ERROR;
						return;
					}
					if(hour == 25) {
						hour = 0;
					}
					if(cutoffs[hour] & (1 << CUTOFF_TYPE_BEFORE)) {
						rule.add(startHour, hour, period);
						break;
					}
				}
			}
		}
	}

	// Translate "before" to CUTOFF_TYPE_BEFORE, for example.
	static CutoffType getCutoffTypeFromString(const char * type_str) {
		if(uprv_strcmp(type_str, "from") == 0) {
			return CUTOFF_TYPE_FROM;
		}
		else if(uprv_strcmp(type_str, "before") == 0) {
			return CUTOFF_TYPE_BEFORE;
		}
		else if(uprv_strcmp(type_str, "after") == 0) {
			return CUTOFF_TYPE_AFTER;
		}
		else if(uprv_strcmp(type_str, "at") == 0) {
			return CUTOFF_TYPE_AT;
		}
		else {
			return CUTOFF_TYPE_UNKNOWN;
		}
	}

	// Gets the numerical value of the hour from the Unicode string.
	static int32_t parseHour(const UnicodeString & time, UErrorCode & errorCode) {
		if(U_FAILURE(errorCode)) {
			return 0;
		}

		int32_t hourLimit = time.length() - 3;
		// `time` must look like "x:00" or "xx:00".
		// If length is wrong or `time` doesn't end with ":00", error out.
		if((hourLimit != 1 && hourLimit != 2) ||
		    time[hourLimit] != 0x3A || time[hourLimit + 1] != 0x30 ||
		    time[hourLimit + 2] != 0x30) {
			errorCode = U_INVALID_FORMAT_ERROR;
			return 0;
		}

		// If `time` doesn't begin with a number in [0, 24], error out.
		// Note: "24:00" is possible in "before 24:00".
		int32_t hour = time[0] - 0x30;
		if(hour < 0 || 9 < hour) {
			errorCode = U_INVALID_FORMAT_ERROR;
			return 0;
		}
		if(hourLimit == 2) {
			int32_t hourDigit2 = time[1] - 0x30;
			if(hourDigit2 < 0 || 9 < hourDigit2) {
				errorCode = U_INVALID_FORMAT_ERROR;
				return 0;
			}
			hour = hour * 10 + hourDigit2;
			if(hour > 24) {
				errorCode = U_INVALID_FORMAT_ERROR;
				return 0;
			}
		}

		return hour;
	}
};  // struct DayPeriodRulesDataSink

struct DayPeriodRulesCountSink : public ResourceSink {
	virtual ~DayPeriodRulesCountSink();

	virtual void put(const char * key, ResourceValue &value, bool, UErrorCode & errorCode) override {
		ResourceTable rules = value.getTable(errorCode);
		if(U_FAILURE(errorCode)) {
			return;
		}

		for(int32_t i = 0; rules.getKeyAndValue(i, key, value); ++i) {
			int32_t setNum = DayPeriodRulesDataSink::parseSetNum(key, errorCode);
			if(setNum > data->maxRuleSetNum) {
				data->maxRuleSetNum = setNum;
			}
		}
	}
};

// Out-of-line virtual destructors.
DayPeriodRulesDataSink::~DayPeriodRulesDataSink() {
}

DayPeriodRulesCountSink::~DayPeriodRulesCountSink() {
}

namespace {
UInitOnce initOnce = U_INITONCE_INITIALIZER;

U_CFUNC bool U_CALLCONV dayPeriodRulesCleanup() {
	delete[] data->rules;
	uhash_close(data->localeToRuleSetNumMap);
	delete data;
	data = NULL;
	return TRUE;
}
}  // namespace

void U_CALLCONV DayPeriodRules::load(UErrorCode & errorCode) {
	if(U_FAILURE(errorCode)) {
		return;
	}

	data = new DayPeriodRulesData();
	data->localeToRuleSetNumMap = uhash_open(uhash_hashChars, uhash_compareChars, NULL, &errorCode);
	LocalUResourceBundlePointer rb_dayPeriods(ures_openDirect(NULL, "dayPeriods", &errorCode));

	// Get the largest rule set number (so we allocate enough objects).
	DayPeriodRulesCountSink countSink;
	ures_getAllItemsWithFallback(rb_dayPeriods.getAlias(), "rules", countSink, errorCode);

	// Populate rules.
	DayPeriodRulesDataSink sink;
	ures_getAllItemsWithFallback(rb_dayPeriods.getAlias(), "", sink, errorCode);

	ucln_i18n_registerCleanup(UCLN_I18N_DAYPERIODRULES, dayPeriodRulesCleanup);
}

const DayPeriodRules * DayPeriodRules::getInstance(const Locale &locale, UErrorCode & errorCode) {
	umtx_initOnce(initOnce, DayPeriodRules::load, errorCode);

	// If the entire day period rules data doesn't conform to spec (even if the part we want
	// does), return NULL.
	if(U_FAILURE(errorCode)) {
		return NULL;
	}

	const char * localeCode = locale.getBaseName();
	char name[ULOC_FULLNAME_CAPACITY];
	char parentName[ULOC_FULLNAME_CAPACITY];

	if(uprv_strlen(localeCode) < ULOC_FULLNAME_CAPACITY) {
		uprv_strcpy(name, localeCode);

		// Treat empty string as root.
		if(*name == '\0') {
			uprv_strcpy(name, "root");
		}
	}
	else {
		errorCode = U_BUFFER_OVERFLOW_ERROR;
		return NULL;
	}

	int32_t ruleSetNum = 0; // NB there is no rule set 0 and 0 is returned upon lookup failure.
	while(*name != '\0') {
		ruleSetNum = uhash_geti(data->localeToRuleSetNumMap, name);
		if(ruleSetNum == 0) {
			// name and parentName can't be the same pointer, so fill in parent then copy to child.
			uloc_getParent(name, parentName, ULOC_FULLNAME_CAPACITY, &errorCode);
			if(*parentName == '\0') {
				// Saves a lookup in the hash table.
				break;
			}
			uprv_strcpy(name, parentName);
		}
		else {
			break;
		}
	}

	if(ruleSetNum <= 0 || data->rules[ruleSetNum].getDayPeriodForHour(0) == DAYPERIOD_UNKNOWN) {
		// If day period for hour 0 is UNKNOWN then day period for all hours are UNKNOWN.
		// Data doesn't exist even with fallback.
		return NULL;
	}
	else {
		return &data->rules[ruleSetNum];
	}
}

DayPeriodRules::DayPeriodRules() : fHasMidnight(FALSE), fHasNoon(FALSE) 
{
	for(int32_t i = 0; i < 24; ++i) {
		fDayPeriodForHour[i] = DayPeriodRules::DAYPERIOD_UNKNOWN;
	}
}

double DayPeriodRules::getMidPointForDayPeriod(DayPeriodRules::DayPeriod dayPeriod, UErrorCode & errorCode) const 
{
	if(U_FAILURE(errorCode)) {
		return -1;
	}
	int32_t startHour = getStartHourForDayPeriod(dayPeriod, errorCode);
	int32_t endHour = getEndHourForDayPeriod(dayPeriod, errorCode);
	// Can't obtain startHour or endHour; bail out.
	if(U_FAILURE(errorCode)) {
		return -1;
	}
	double midPoint = (startHour + endHour) / 2.0;
	if(startHour > endHour) {
		// dayPeriod wraps around midnight. Shift midPoint by 12 hours, in the direction that
		// lands it in [0, 24).
		midPoint += 12;
		if(midPoint >= 24) {
			midPoint -= 24;
		}
	}

	return midPoint;
}

int32_t DayPeriodRules::getStartHourForDayPeriod(DayPeriodRules::DayPeriod dayPeriod, UErrorCode & errorCode) const 
{
	if(U_FAILURE(errorCode)) {
		return -1;
	}
	if(dayPeriod == DAYPERIOD_MIDNIGHT) {
		return 0;
	}
	if(dayPeriod == DAYPERIOD_NOON) {
		return 12;
	}
	if(fDayPeriodForHour[0] == dayPeriod && fDayPeriodForHour[23] == dayPeriod) {
		// dayPeriod wraps around midnight. Start hour is later than end hour.
		for(int32_t i = 22; i >= 1; --i) {
			if(fDayPeriodForHour[i] != dayPeriod) {
				return (i + 1);
			}
		}
	}
	else {
		for(int32_t i = 0; i <= 23; ++i) {
			if(fDayPeriodForHour[i] == dayPeriod) {
				return i;
			}
		}
	}
	// dayPeriod doesn't exist in rule set; set error and exit.
	errorCode = U_ILLEGAL_ARGUMENT_ERROR;
	return -1;
}

int32_t DayPeriodRules::getEndHourForDayPeriod(DayPeriodRules::DayPeriod dayPeriod, UErrorCode & errorCode) const 
{
	if(U_FAILURE(errorCode)) {
		return -1;
	}
	if(dayPeriod == DAYPERIOD_MIDNIGHT) {
		return 0;
	}
	if(dayPeriod == DAYPERIOD_NOON) {
		return 12;
	}
	if(fDayPeriodForHour[0] == dayPeriod && fDayPeriodForHour[23] == dayPeriod) {
		// dayPeriod wraps around midnight. End hour is before start hour.
		for(int32_t i = 1; i <= 22; ++i) {
			if(fDayPeriodForHour[i] != dayPeriod) {
				// i o'clock is when a new period starts, therefore when the old period ends.
				return i;
			}
		}
	}
	else {
		for(int32_t i = 23; i >= 0; --i) {
			if(fDayPeriodForHour[i] == dayPeriod) {
				return (i + 1);
			}
		}
	}
	// dayPeriod doesn't exist in rule set; set error and exit.
	errorCode = U_ILLEGAL_ARGUMENT_ERROR;
	return -1;
}

DayPeriodRules::DayPeriod DayPeriodRules::getDayPeriodFromString(const char * type_str) 
{
	if(sstreq(type_str, "midnight"))
		return DAYPERIOD_MIDNIGHT;
	else if(sstreq(type_str, "noon"))
		return DAYPERIOD_NOON;
	else if(sstreq(type_str, "morning1"))
		return DAYPERIOD_MORNING1;
	else if(sstreq(type_str, "afternoon1"))
		return DAYPERIOD_AFTERNOON1;
	else if(sstreq(type_str, "evening1"))
		return DAYPERIOD_EVENING1;
	else if(sstreq(type_str, "night1"))
		return DAYPERIOD_NIGHT1;
	else if(sstreq(type_str, "morning2"))
		return DAYPERIOD_MORNING2;
	else if(sstreq(type_str, "afternoon2"))
		return DAYPERIOD_AFTERNOON2;
	else if(sstreq(type_str, "evening2"))
		return DAYPERIOD_EVENING2;
	else if(sstreq(type_str, "night2"))
		return DAYPERIOD_NIGHT2;
	else if(sstreq(type_str, "am"))
		return DAYPERIOD_AM;
	else if(sstreq(type_str, "pm"))
		return DAYPERIOD_PM;
	else
		return DAYPERIOD_UNKNOWN;
}

void DayPeriodRules::add(int32_t startHour, int32_t limitHour, DayPeriod period) 
{
	for(int32_t i = startHour; i != limitHour; ++i) {
		if(i == 24) {
			i = 0;
		}
		fDayPeriodForHour[i] = period;
	}
}

bool DayPeriodRules::allHoursAreSet() 
{
	for(int32_t i = 0; i < 24; ++i) {
		if(fDayPeriodForHour[i] == DAYPERIOD_UNKNOWN)
			return false;
	}
	return true;
}

U_NAMESPACE_END
