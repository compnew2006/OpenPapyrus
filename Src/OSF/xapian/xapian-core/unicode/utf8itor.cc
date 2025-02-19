/** @file
 * @brief iterate over a utf8 string.
 */
// Copyright (C) 2006,2007,2010,2013,2015,2019 Olly Betts
// @license GNU GPL
//
#include <xapian-internal.h>
#pragma hdrstop

using namespace std;

static inline bool bad_cont(uchar ch) {
	return static_cast<signed char>(ch) >= static_cast<signed char>(0xc0);
}

namespace Xapian {
namespace Unicode {
// buf should be at least 4 bytes.
unsigned nonascii_to_utf8(uint ch, char* buf)
{
	if(ch < 0x800) {
		buf[0] = char(0xc0 | (ch >> 6));
		buf[1] = char(0x80 | (ch & 0x3f));
		return 2;
	}
	if(ch < 0x10000) {
		buf[0] = char(0xe0 | (ch >> 12));
		buf[1] = char(0x80 | ((ch >> 6) & 0x3f));
		buf[2] = char(0x80 | (ch & 0x3f));
		return 3;
	}
	if(ch < 0x200000) {
		buf[0] = char(0xf0 | (ch >> 18));
		buf[1] = char(0x80 | ((ch >> 12) & 0x3f));
		buf[2] = char(0x80 | ((ch >> 6) & 0x3f));
		buf[3] = char(0x80 | (ch & 0x3f));
		return 4;
	}
	// Unicode doesn't specify any characters above 0x10ffff.
	// Should we be presented with such a numeric character
	// entity or similar, we just replace it with nothing.
	return 0;
}
}

Utf8Iterator::Utf8Iterator(const char* p_)
{
	assign(p_, strlen(p_));
}

bool Utf8Iterator::calculate_sequence_length() const noexcept
{
	// Handle invalid UTF-8, overlong sequences, and truncated sequences as
	// if the text was actually in ISO-8859-1 since we need to do something
	// with it, and this seems the most likely reason why we'd have invalid
	// UTF-8.
	uchar ch = *p;
	seqlen = 1;
	// Single byte encoding (0x00-0x7f) or invalid (0x80-0xbf) or overlong
	// sequence (0xc0-0xc1).
	//
	// (0xc0 and 0xc1 would start 2 byte sequences for characters which are
	// representable in a single byte, and we should not decode these.)
	if(ch < 0xc2) 
		return (ch < 0x80);
	if(ch < 0xe0) {
		if(p + 1 == end || // Not enough bytes
		    bad_cont(p[1])) // Invalid
			return false;
		seqlen = 2;
		return true;
	}
	if(ch < 0xf0) {
		if(end - p < 3 || // Not enough bytes
		    bad_cont(p[1]) || bad_cont(p[2]) || // Invalid
		    (p[0] == 0xe0 && p[1] < 0xa0)) // Overlong encoding
			return false;
		seqlen = 3;
		return true;
	}
	if(ch >= 0xf5 || // Code value above Unicode
	    end - p < 4 || // Not enough bytes
	    bad_cont(p[1]) || bad_cont(p[2]) || bad_cont(p[3]) || // Invalid
	    (p[0] == 0xf0 && p[1] < 0x90) || // Overlong encoding
	    (p[0] == 0xf4 && p[1] >= 0x90)) // Code value above Unicode
		return false;
	seqlen = 4;
	return true;
}

unsigned Utf8Iterator::operator*() const noexcept 
{
	if(p == NULL) return unsigned(-1);
	if(seqlen == 0) calculate_sequence_length();
	uchar ch = *p;
	if(seqlen == 1) return ch;
	if(seqlen == 2) return ((ch & 0x1f) << 6) | (p[1] & 0x3f);
	if(seqlen == 3)
		return ((ch & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
	return ((ch & 0x07) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}

unsigned Utf8Iterator::strict_deref() const noexcept
{
	if(p == NULL) 
		return unsigned(-1);
	if(seqlen == 0) {
		if(!calculate_sequence_length())
			return unsigned(*p) | 0x80000000;
	}
	uchar ch = *p;
	if(seqlen == 1) return ch;
	if(seqlen == 2) return ((ch & 0x1f) << 6) | (p[1] & 0x3f);
	if(seqlen == 3)
		return ((ch & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
	return ((ch & 0x07) << 18) | ((p[1] & 0x3f) << 12) |
	       ((p[2] & 0x3f) << 6) | (p[3] & 0x3f);
}
}
