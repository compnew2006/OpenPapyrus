// CANITER.CPP
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1996-2015, International Business Machines Corporation and others. All Rights Reserved.
//
#include <icu-internal.h>
#pragma hdrstop

#if !UCONFIG_NO_NORMALIZATION

#include "unicode/caniter.h"

/**
 * This class allows one to iterate through all the strings that are canonically equivalent to a given
 * string. For example, here are some sample results:
   Results for: {LATIN CAPITAL LETTER A WITH RING ABOVE}{LATIN SMALL LETTER D}{COMBINING DOT ABOVE}{COMBINING CEDILLA}
   1: \u0041\u030A\u0064\u0307\u0327
   = {LATIN CAPITAL LETTER A}{COMBINING RING ABOVE}{LATIN SMALL LETTER D}{COMBINING DOT ABOVE}{COMBINING CEDILLA}
   2: \u0041\u030A\u0064\u0327\u0307
   = {LATIN CAPITAL LETTER A}{COMBINING RING ABOVE}{LATIN SMALL LETTER D}{COMBINING CEDILLA}{COMBINING DOT ABOVE}
   3: \u0041\u030A\u1E0B\u0327
   = {LATIN CAPITAL LETTER A}{COMBINING RING ABOVE}{LATIN SMALL LETTER D WITH DOT ABOVE}{COMBINING CEDILLA}
   4: \u0041\u030A\u1E11\u0307
   = {LATIN CAPITAL LETTER A}{COMBINING RING ABOVE}{LATIN SMALL LETTER D WITH CEDILLA}{COMBINING DOT ABOVE}
   5: \u00C5\u0064\u0307\u0327
   = {LATIN CAPITAL LETTER A WITH RING ABOVE}{LATIN SMALL LETTER D}{COMBINING DOT ABOVE}{COMBINING CEDILLA}
   6: \u00C5\u0064\u0327\u0307
   = {LATIN CAPITAL LETTER A WITH RING ABOVE}{LATIN SMALL LETTER D}{COMBINING CEDILLA}{COMBINING DOT ABOVE}
   7: \u00C5\u1E0B\u0327
   = {LATIN CAPITAL LETTER A WITH RING ABOVE}{LATIN SMALL LETTER D WITH DOT ABOVE}{COMBINING CEDILLA}
   8: \u00C5\u1E11\u0307
   = {LATIN CAPITAL LETTER A WITH RING ABOVE}{LATIN SMALL LETTER D WITH CEDILLA}{COMBINING DOT ABOVE}
   9: \u212B\u0064\u0307\u0327
   = {ANGSTROM SIGN}{LATIN SMALL LETTER D}{COMBINING DOT ABOVE}{COMBINING CEDILLA}
   10: \u212B\u0064\u0327\u0307
   = {ANGSTROM SIGN}{LATIN SMALL LETTER D}{COMBINING CEDILLA}{COMBINING DOT ABOVE}
   11: \u212B\u1E0B\u0327
   = {ANGSTROM SIGN}{LATIN SMALL LETTER D WITH DOT ABOVE}{COMBINING CEDILLA}
   12: \u212B\u1E11\u0307
   = {ANGSTROM SIGN}{LATIN SMALL LETTER D WITH CEDILLA}{COMBINING DOT ABOVE}
 *<br>Note: the code is intended for use with small strings, and is not suitable for larger ones,
 * since it has not been optimized for that situation.
 *@author M. Davis
 *@draft
 */

// public

U_NAMESPACE_BEGIN
// TODO: add boilerplate methods.

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(CanonicalIterator)

/**
 *@param source string to get results for
 */
CanonicalIterator::CanonicalIterator(const UnicodeString & sourceStr, UErrorCode & status) :
	pieces(NULL),
	pieces_length(0),
	pieces_lengths(NULL),
	current(NULL),
	current_length(0),
	nfd(*Normalizer2::getNFDInstance(status)),
	nfcImpl(*Normalizer2Factory::getNFCImpl(status))
{
	if(U_SUCCESS(status) && nfcImpl.ensureCanonIterData(status)) {
		setSource(sourceStr, status);
	}
}

CanonicalIterator::~CanonicalIterator() 
{
	cleanPieces();
}

void CanonicalIterator::cleanPieces() 
{
	if(pieces) {
		for(int32_t i = 0; i < pieces_length; i++) {
			if(pieces[i] != NULL) {
				delete[] pieces[i];
			}
		}
		uprv_free(pieces);
		pieces = NULL;
		pieces_length = 0;
	}
	if(pieces_lengths != NULL) {
		uprv_free(pieces_lengths);
		pieces_lengths = NULL;
	}
	if(current != NULL) {
		uprv_free(current);
		current = NULL;
		current_length = 0;
	}
}
/**
 *@return gets the source: NOTE: it is the NFD form of source
 */
UnicodeString CanonicalIterator::getSource() { return source; }

/**
 * Resets the iterator so that one can start again from the beginning.
 */
void CanonicalIterator::reset() 
{
	done = FALSE;
	for(int i = 0; i < current_length; ++i)
		current[i] = 0;
}

/**
 *@return the next string that is canonically equivalent. The value null is returned when
 * the iteration is done.
 */
UnicodeString CanonicalIterator::next() 
{
	int32_t i = 0;
	if(done) {
		buffer.setToBogus();
		return buffer;
	}
	// delete old contents
	buffer.remove();
	// construct return value
	for(i = 0; i < pieces_length; ++i) {
		buffer.append(pieces[i][current[i]]);
	}
	//String result = buffer.toString(); // not needed
	// find next value for next time
	for(i = current_length - 1;; --i) {
		if(i < 0) {
			done = TRUE;
			break;
		}
		current[i]++;
		if(current[i] < pieces_lengths[i]) break; // got sequence
		current[i] = 0;
	}
	return buffer;
}

/**
 *@param set the source string to iterate against. This allows the same iterator to be used
 * while changing the source string, saving object creation.
 */
void CanonicalIterator::setSource(const UnicodeString & newSource, UErrorCode & status) {
	int32_t list_length = 0;
	UChar32 cp = 0;
	int32_t start = 0;
	int32_t i = 0;
	UnicodeString * list = NULL;

	nfd.normalize(newSource, source, status);
	if(U_FAILURE(status)) {
		return;
	}
	done = FALSE;

	cleanPieces();

	// catch degenerate case
	if(newSource.length() == 0) {
		pieces = (UnicodeString **)uprv_malloc(sizeof(UnicodeString *));
		pieces_lengths = (int32_t*)uprv_malloc(1 * sizeof(int32_t));
		pieces_length = 1;
		current = (int32_t*)uprv_malloc(1 * sizeof(int32_t));
		current_length = 1;
		if(pieces == NULL || pieces_lengths == NULL || current == NULL) {
			status = U_MEMORY_ALLOCATION_ERROR;
			goto CleanPartialInitialization;
		}
		current[0] = 0;
		pieces[0] = new UnicodeString[1];
		pieces_lengths[0] = 1;
		if(pieces[0] == 0) {
			status = U_MEMORY_ALLOCATION_ERROR;
			goto CleanPartialInitialization;
		}
		return;
	}

	list = new UnicodeString[source.length()];
	if(list == 0) {
		status = U_MEMORY_ALLOCATION_ERROR;
		goto CleanPartialInitialization;
	}

	// i should initially be the number of code units at the
	// start of the string
	i = U16_LENGTH(source.char32At(0));
	// int32_t i = 1;
	// find the segments
	// This code iterates through the source string and
	// extracts segments that end up on a codepoint that
	// doesn't start any decompositions. (Analysis is done
	// on the NFD form - see above).
	for(; i < source.length(); i += U16_LENGTH(cp)) {
		cp = source.char32At(i);
		if(nfcImpl.isCanonSegmentStarter(cp)) {
			source.extract(start, i-start, list[list_length++]); // add up to i
			start = i;
		}
	}
	source.extract(start, i-start, list[list_length++]); // add last one

	// allocate the arrays, and find the strings that are CE to each segment
	pieces = (UnicodeString **)uprv_malloc(list_length * sizeof(UnicodeString *));
	pieces_length = list_length;
	pieces_lengths = (int32_t*)uprv_malloc(list_length * sizeof(int32_t));
	current = (int32_t*)uprv_malloc(list_length * sizeof(int32_t));
	current_length = list_length;
	if(pieces == NULL || pieces_lengths == NULL || current == NULL) {
		status = U_MEMORY_ALLOCATION_ERROR;
		goto CleanPartialInitialization;
	}

	for(i = 0; i < current_length; i++) {
		current[i] = 0;
	}
	// for each segment, get all the combinations that can produce
	// it after NFD normalization
	for(i = 0; i < pieces_length; ++i) {
		//if(PROGRESS) printf("SEGMENT\n");
		pieces[i] = getEquivalents(list[i], pieces_lengths[i], status);
	}

	delete[] list;
	return;
// Common section to cleanup all local variables and reset object variables.
CleanPartialInitialization:
	if(list) {
		delete[] list;
	}
	cleanPieces();
}

/**
 * Dumb recursive implementation of permutation.
 * TODO: optimize
 * @param source the string to find permutations for
 * @return the results in a set.
 */
void U_EXPORT2 CanonicalIterator::permute(UnicodeString & source, bool skipZeros, Hashtable * result, UErrorCode & status) {
	if(U_FAILURE(status)) {
		return;
	}
	//if(PROGRESS) printf("Permute: %s\n", UToS(Tr(source)));
	int32_t i = 0;

	// optimization:
	// if zero or one character, just return a set with it
	// we check for length < 2 to keep from counting code points all the time
	if(source.length() <= 2 && source.countChar32() <= 1) {
		UnicodeString * toPut = new UnicodeString(source);
		/* test for NULL */
		if(toPut == 0) {
			status = U_MEMORY_ALLOCATION_ERROR;
			return;
		}
		result->put(source, toPut, status);
		return;
	}

	// otherwise iterate through the string, and recursively permute all the other characters
	UChar32 cp;
	Hashtable subpermute(status);
	if(U_FAILURE(status)) {
		return;
	}
	subpermute.setValueDeleter(uprv_deleteUObject);

	for(i = 0; i < source.length(); i += U16_LENGTH(cp)) {
		cp = source.char32At(i);
		const UHashElement * ne = NULL;
		int32_t el = UHASH_FIRST;
		UnicodeString subPermuteString = source;

		// optimization:
		// if the character is canonical combining class zero,
		// don't permute it
		if(skipZeros && i != 0 && u_getCombiningClass(cp) == 0) {
			//System.out.println("Skipping " + Utility.hex(UTF16.valueOf(source, i)));
			continue;
		}

		subpermute.removeAll();

		// see what the permutations of the characters before and after this one are
		//Hashtable *subpermute = permute(source.substring(0,i) + source.substring(i + UTF16.getCharCount(cp)));
		permute(subPermuteString.remove(i, U16_LENGTH(cp)), skipZeros, &subpermute, status);
		/* Test for buffer overflows */
		if(U_FAILURE(status)) {
			return;
		}
		// The upper remove is destructive. The question is do we have to make a copy, or we don't care about
		// the contents
		// of source at this point.

		// prefix this character to all of them
		ne = subpermute.nextElement(el);
		while(ne != NULL) {
			UnicodeString * permRes = (UnicodeString *)(ne->value.pointer);
			UnicodeString * chStr = new UnicodeString(cp);
			//test for  NULL
			if(chStr == NULL) {
				status = U_MEMORY_ALLOCATION_ERROR;
				return;
			}
			chStr->append(*permRes); //*((UnicodeString *)(ne->value.pointer));
			//if(PROGRESS) printf("  Piece: %s\n", UToS(*chStr));
			result->put(*chStr, chStr, status);
			ne = subpermute.nextElement(el);
		}
	}
	//return result;
}

// privates

// we have a segment, in NFD. Find all the strings that are canonically equivalent to it.
UnicodeString * CanonicalIterator::getEquivalents(const UnicodeString & segment, int32_t &result_len, UErrorCode & status) {
	Hashtable result(status);
	Hashtable permutations(status);
	Hashtable basic(status);
	if(U_FAILURE(status)) {
		return 0;
	}
	result.setValueDeleter(uprv_deleteUObject);
	permutations.setValueDeleter(uprv_deleteUObject);
	basic.setValueDeleter(uprv_deleteUObject);

	UChar USeg[256];
	int32_t segLen = segment.extract(USeg, 256, status);
	getEquivalents2(&basic, USeg, segLen, status);

	// now get all the permutations
	// add only the ones that are canonically equivalent
	// TODO: optimize by not permuting any class zero.

	const UHashElement * ne = NULL;
	int32_t el = UHASH_FIRST;
	//Iterator it = basic.iterator();
	ne = basic.nextElement(el);
	//while(it.hasNext())
	while(ne != NULL) {
		//String item = (String) it.next();
		UnicodeString item = *((UnicodeString *)(ne->value.pointer));

		permutations.removeAll();
		permute(item, CANITER_SKIP_ZEROES, &permutations, status);
		const UHashElement * ne2 = NULL;
		int32_t el2 = UHASH_FIRST;
		//Iterator it2 = permutations.iterator();
		ne2 = permutations.nextElement(el2);
		//while(it2.hasNext())
		while(ne2 != NULL) {
			//String possible = (String) it2.next();
			//UnicodeString *possible = new UnicodeString(*((UnicodeString *)(ne2->value.pointer)));
			UnicodeString possible(*((UnicodeString *)(ne2->value.pointer)));
			UnicodeString attempt;
			nfd.normalize(possible, attempt, status);

			// TODO: check if operator == is semanticaly the same as attempt.equals(segment)
			if(attempt==segment) {
				//if(PROGRESS) printf("Adding Permutation: %s\n", UToS(Tr(*possible)));
				// TODO: use the hashtable just to catch duplicates - store strings directly (somehow).
				result.put(possible, new UnicodeString(possible), status); //add(possible);
			}
			else {
				//if(PROGRESS) printf("-Skipping Permutation: %s\n", UToS(Tr(*possible)));
			}

			ne2 = permutations.nextElement(el2);
		}
		ne = basic.nextElement(el);
	}

	/* Test for buffer overflows */
	if(U_FAILURE(status)) {
		return 0;
	}
	// convert into a String[] to clean up storage
	//String[] finalResult = new String[result.size()];
	UnicodeString * finalResult = NULL;
	int32_t resultCount;
	if((resultCount = result.count()) != 0) {
		finalResult = new UnicodeString[resultCount];
		if(finalResult == 0) {
			status = U_MEMORY_ALLOCATION_ERROR;
			return NULL;
		}
	}
	else {
		status = U_ILLEGAL_ARGUMENT_ERROR;
		return NULL;
	}
	//result.toArray(finalResult);
	result_len = 0;
	el = UHASH_FIRST;
	ne = result.nextElement(el);
	while(ne != NULL) {
		finalResult[result_len++] = *((UnicodeString *)(ne->value.pointer));
		ne = result.nextElement(el);
	}

	return finalResult;
}

Hashtable * CanonicalIterator::getEquivalents2(Hashtable * fillinResult, const UChar * segment, int32_t segLen, UErrorCode & status) {
	if(U_FAILURE(status)) {
		return NULL;
	}

	//if(PROGRESS) printf("Adding: %s\n", UToS(Tr(segment)));

	UnicodeString toPut(segment, segLen);

	fillinResult->put(toPut, new UnicodeString(toPut), status);

	UnicodeSet starts;

	// cycle through all the characters
	UChar32 cp;
	for(int32_t i = 0; i < segLen; i += U16_LENGTH(cp)) {
		// see if any character is at the start of some decomposition
		U16_GET(segment, 0, i, segLen, cp);
		if(!nfcImpl.getCanonStartSet(cp, starts)) {
			continue;
		}
		// if so, see which decompositions match
		UnicodeSetIterator iter(starts);
		while(iter.next()) {
			UChar32 cp2 = iter.getCodepoint();
			Hashtable remainder(status);
			remainder.setValueDeleter(uprv_deleteUObject);
			if(extract(&remainder, cp2, segment, segLen, i, status) == NULL) {
				continue;
			}

			// there were some matches, so add all the possibilities to the set.
			UnicodeString prefix(segment, i);
			prefix += cp2;

			int32_t el = UHASH_FIRST;
			const UHashElement * ne = remainder.nextElement(el);
			while(ne != NULL) {
				UnicodeString item = *((UnicodeString *)(ne->value.pointer));
				UnicodeString * toAdd = new UnicodeString(prefix);
				/* test for NULL */
				if(toAdd == 0) {
					status = U_MEMORY_ALLOCATION_ERROR;
					return NULL;
				}
				*toAdd += item;
				fillinResult->put(*toAdd, toAdd, status);

				//if(PROGRESS) printf("Adding: %s\n", UToS(Tr(*toAdd)));

				ne = remainder.nextElement(el);
			}
		}
	}

	/* Test for buffer overflows */
	if(U_FAILURE(status)) {
		return NULL;
	}
	return fillinResult;
}

/**
 * See if the decomposition of cp2 is at segment starting at segmentPos
 * (with canonical rearrangement!)
 * If so, take the remainder, and return the equivalents
 */
Hashtable * CanonicalIterator::extract(Hashtable * fillinResult,
    UChar32 comp,
    const UChar * segment,
    int32_t segLen,
    int32_t segmentPos,
    UErrorCode & status) {
//Hashtable *CanonicalIterator::extract(UChar32 comp, const UnicodeString & segment, int32_t segLen, int32_t segmentPos,
// UErrorCode & status) {
	//if(PROGRESS) printf(" extract: %s, ", UToS(Tr(UnicodeString(comp))));
	//if(PROGRESS) printf("%s, %i\n", UToS(Tr(segment)), segmentPos);

	if(U_FAILURE(status)) {
		return NULL;
	}

	UnicodeString temp(comp);
	int32_t inputLen = temp.length();
	UnicodeString decompString;
	nfd.normalize(temp, decompString, status);
	if(U_FAILURE(status)) {
		return NULL;
	}
	if(decompString.isBogus()) {
		status = U_MEMORY_ALLOCATION_ERROR;
		return NULL;
	}
	const UChar * decomp = decompString.getBuffer();
	int32_t decompLen = decompString.length();

	// See if it matches the start of segment (at segmentPos)
	bool ok = FALSE;
	UChar32 cp;
	int32_t decompPos = 0;
	UChar32 decompCp;
	U16_NEXT(decomp, decompPos, decompLen, decompCp);

	int32_t i = segmentPos;
	while(i < segLen) {
		U16_NEXT(segment, i, segLen, cp);

		if(cp == decompCp) { // if equal, eat another cp from decomp
			//if(PROGRESS) printf("  matches: %s\n", UToS(Tr(UnicodeString(cp))));

			if(decompPos == decompLen) { // done, have all decomp characters!
				temp.append(segment+i, segLen-i);
				ok = TRUE;
				break;
			}
			U16_NEXT(decomp, decompPos, decompLen, decompCp);
		}
		else {
			//if(PROGRESS) printf("  buffer: %s\n", UToS(Tr(UnicodeString(cp))));

			// brute force approach
			temp.append(cp);

			/* TODO: optimize
			   // since we know that the classes are monotonically increasing, after zero
			   // e.g. 0 5 7 9 0 3
			   // we can do an optimization
			   // there are only a few cases that work: zero, less, same, greater
			   // if both classes are the same, we fail
			   // if the decomp class < the segment class, we fail

			   segClass = getClass(cp);
			   if(decompClass <= segClass) return null;
			 */
		}
	}
	if(!ok)
		return NULL; // we failed, characters left over
	//if(PROGRESS) printf("Matches\n");
	if(inputLen == temp.length()) {
		fillinResult->put(UnicodeString(), new UnicodeString(), status);
		return fillinResult; // succeed, but no remainder
	}

	// brute force approach
	// check to make sure result is canonically equivalent
	UnicodeString trial;
	nfd.normalize(temp, trial, status);
	if(U_FAILURE(status) || trial.compare(segment+segmentPos, segLen - segmentPos) != 0) {
		return NULL;
	}
	return getEquivalents2(fillinResult, temp.getBuffer()+inputLen, temp.length()-inputLen, status);
}

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_NORMALIZATION */
