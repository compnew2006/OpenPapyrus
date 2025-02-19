// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 * Copyright (C) 2004-2012, International Business Machines Corporation and
 * others. All Rights Reserved.
 *******************************************************************************
 */
#include <icu-internal.h>
#pragma hdrstop

#if !UCONFIG_NO_FORMATTING

#include "unicode/utmscale.h"

#define ticks        INT64_C(1)
#define microseconds (ticks * 10)
#define milliseconds (microseconds * 1000)
#define seconds      (milliseconds * 1000)
#define minutes      (seconds * 60)
#define hours        (minutes * 60)
#define days         (hours * 24)

/* Constants generated by ICU4J com.ibm.icu.dev.tool.timescale.GenerateCTimeScaleData. */
static const int64_t timeScaleTable[UDTS_MAX_SCALE][UTSV_MAX_SCALE_VALUE] = {
	/* units             epochOffset                     fromMin                        fromMax
	                              toMin                        toMax                    epochOffsetP1
	                  epochOffsetM1              unitsRound                    minRound                     maxRound
	   */
	{milliseconds, INT64_C(62135596800000),     INT64_C(-984472800485477),     INT64_C(860201606885477),     INT64_C(
		 -9223372036854774999), INT64_C(9223372036854774999), INT64_C(62135596800001),     INT64_C(62135596799999),     INT64_C(
		 5000),         INT64_C(-9223372036854770808), INT64_C(9223372036854770807)},
	{seconds,      INT64_C(62135596800),        INT64_C(-984472800485),        INT64_C(860201606885),        U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(62135596801),        INT64_C(62135596799),        INT64_C(5000000),      INT64_C(
		 -9223372036849775808), INT64_C(9223372036849775807)},
	{milliseconds, INT64_C(62135596800000),     INT64_C(-984472800485477),     INT64_C(860201606885477),     INT64_C(
		 -9223372036854774999), INT64_C(9223372036854774999), INT64_C(62135596800001),     INT64_C(62135596799999),     INT64_C(
		 5000),         INT64_C(-9223372036854770808), INT64_C(9223372036854770807)},
	{ticks,        INT64_C(504911232000000000), U_INT64_MIN,                   INT64_C(8718460804854775807), INT64_C(
		 -8718460804854775808), U_INT64_MAX,                  INT64_C(504911232000000000), INT64_C(504911232000000000),
	 INT64_C(0),            U_INT64_MIN,                   U_INT64_MAX},
	{ticks,        INT64_C(0),                  U_INT64_MIN,                   U_INT64_MAX,                  U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(0),                  INT64_C(0),                  INT64_C(0),
	 U_INT64_MIN,                   U_INT64_MAX},
	{seconds,      INT64_C(60052752000),        INT64_C(-982389955685),        INT64_C(862284451685),        U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(60052752001),        INT64_C(60052751999),        INT64_C(5000000),      INT64_C(
		 -9223372036849775808), INT64_C(9223372036849775807)},
	{seconds,      INT64_C(63113904000),        INT64_C(-985451107685),        INT64_C(859223299685),        U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(63113904001),        INT64_C(63113903999),        INT64_C(5000000),      INT64_C(
		 -9223372036849775808), INT64_C(9223372036849775807)},
	{days,         INT64_C(693594),             INT64_C(-11368793),            INT64_C(9981605),             U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(693595),             INT64_C(693593),             INT64_C(432000000000), INT64_C(
		 -9223371604854775808), INT64_C(9223371604854775807)},
	{days,         INT64_C(693594),             INT64_C(-11368793),            INT64_C(9981605),             U_INT64_MIN,
	 U_INT64_MAX,                  INT64_C(693595),             INT64_C(693593),             INT64_C(432000000000), INT64_C(
		 -9223371604854775808), INT64_C(9223371604854775807)},
	{microseconds, INT64_C(62135596800000000),  INT64_C(-984472800485477580),  INT64_C(860201606885477580),  INT64_C(
		 -9223372036854775804), INT64_C(9223372036854775804), INT64_C(62135596800000001),  INT64_C(62135596799999999),
	 INT64_C(5),            INT64_C(-9223372036854775803), INT64_C(9223372036854775802)},
};

U_CAPI int64_t U_EXPORT2 utmscale_getTimeScaleValue(UDateTimeScale timeScale, UTimeScaleValue value, UErrorCode * status)
{
	if(status == NULL || U_FAILURE(*status)) {
		return 0;
	}

	if(timeScale < UDTS_JAVA_TIME || UDTS_MAX_SCALE <= timeScale
	 || value < UTSV_UNITS_VALUE || UTSV_MAX_SCALE_VALUE <= value) {
		*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	return timeScaleTable[timeScale][value];
}

U_CAPI int64_t U_EXPORT2 utmscale_fromInt64(int64_t otherTime, UDateTimeScale timeScale, UErrorCode * status)
{
	const int64_t * data;

	if(status == NULL || U_FAILURE(*status)) {
		return 0;
	}

	if((int32_t)timeScale < 0 || timeScale >= UDTS_MAX_SCALE) {
		*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	data = (const int64_t*)(&timeScaleTable[timeScale]);

	if(otherTime < data[UTSV_FROM_MIN_VALUE] || otherTime > data[UTSV_FROM_MAX_VALUE]) {
		*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	return (otherTime + data[UTSV_EPOCH_OFFSET_VALUE]) * data[UTSV_UNITS_VALUE];
}

U_CAPI int64_t U_EXPORT2 utmscale_toInt64(int64_t universalTime, UDateTimeScale timeScale, UErrorCode * status)
{
	const int64_t * data;

	if(status == NULL || U_FAILURE(*status)) {
		return 0;
	}

	if((int32_t)timeScale < 0 || timeScale >= UDTS_MAX_SCALE) {
		*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	data = (const int64_t*)(&timeScaleTable[timeScale]);

	if(universalTime < data[UTSV_TO_MIN_VALUE] || universalTime > data[UTSV_TO_MAX_VALUE]) {
		*status = U_ILLEGAL_ARGUMENT_ERROR;
		return 0;
	}

	if(universalTime < 0) {
		if(universalTime < data[UTSV_MIN_ROUND_VALUE]) {
			return (universalTime + data[UTSV_UNITS_ROUND_VALUE]) / data[UTSV_UNITS_VALUE] -
			       data[UTSV_EPOCH_OFFSET_PLUS_1_VALUE];
		}

		return (universalTime - data[UTSV_UNITS_ROUND_VALUE]) / data[UTSV_UNITS_VALUE] - data[UTSV_EPOCH_OFFSET_VALUE];
	}

	if(universalTime > data[UTSV_MAX_ROUND_VALUE]) {
		return (universalTime - data[UTSV_UNITS_ROUND_VALUE]) / data[UTSV_UNITS_VALUE] - data[UTSV_EPOCH_OFFSET_MINUS_1_VALUE];
	}

	return (universalTime + data[UTSV_UNITS_ROUND_VALUE]) / data[UTSV_UNITS_VALUE] - data[UTSV_EPOCH_OFFSET_VALUE];
}

#endif /* #if !UCONFIG_NO_FORMATTING */
