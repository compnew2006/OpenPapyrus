// Copyright 2020 The Abseil Authors.
// Licensed under the Apache License, Version 2.0 (the "License");
//
#include "absl/absl-internal.h"
#pragma hdrstop
#include "absl/strings/internal/escaping.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace strings_internal {
const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

size_t CalculateBase64EscapedLenInternal(size_t input_len, bool do_padding) {
	// Base64 encodes three bytes of input at a time. If the input is not
	// divisible by three, we pad as appropriate.
	//
	// (from https://tools.ietf.org/html/rfc3548)
	// Special processing is performed if fewer than 24 bits are available
	// at the end of the data being encoded.  A full encoding quantum is
	// always completed at the end of a quantity.  When fewer than 24 input
	// bits are available in an input group, zero bits are added (on the
	// right) to form an integral number of 6-bit groups.  Padding at the
	// end of the data is performed using the '=' character.  Since all base
	// 64 input is an integral number of octets, only the following cases
	// can arise:

	// Base64 encodes each three bytes of input into four bytes of output.
	size_t len = (input_len / 3) * 4;

	if(input_len % 3 == 0) {
		// (from https://tools.ietf.org/html/rfc3548)
		// (1) the final quantum of encoding input is an integral multiple of 24
		// bits; here, the final unit of encoded output will be an integral
		// multiple of 4 characters with no "=" padding,
	}
	else if(input_len % 3 == 1) {
		// (from https://tools.ietf.org/html/rfc3548)
		// (2) the final quantum of encoding input is exactly 8 bits; here, the
		// final unit of encoded output will be two characters followed by two
		// "=" padding characters, or
		len += 2;
		if(do_padding) {
			len += 2;
		}
	}
	else { // (input_len % 3 == 2)
		// (from https://tools.ietf.org/html/rfc3548)
		// (3) the final quantum of encoding input is exactly 16 bits; here, the
		// final unit of encoded output will be three characters followed by one
		// "=" padding character.
		len += 3;
		if(do_padding) {
			len += 1;
		}
	}

	assert(len >= input_len); // make sure we didn't overflow
	return len;
}

size_t Base64EscapeInternal(const unsigned char* src, size_t szsrc, char* dest,
    size_t szdest, const char* base64,
    bool do_padding) {
	static const char kPad64 = '=';

	if(szsrc * 4 > szdest * 3) return 0;

	char* cur_dest = dest;
	const unsigned char* cur_src = src;

	char* const limit_dest = dest + szdest;
	const unsigned char* const limit_src = src + szsrc;

	// Three bytes of data encodes to four characters of cyphertext.
	// So we can pump through three-byte chunks atomically.
	if(szsrc >= 3) {               // "limit_src - 3" is UB if szsrc < 3.
		while(cur_src < limit_src - 3) { // While we have >= 32 bits.
			uint32_t in = absl::big_endian::Load32(cur_src) >> 8;

			cur_dest[0] = base64[in >> 18];
			in &= 0x3FFFF;
			cur_dest[1] = base64[in >> 12];
			in &= 0xFFF;
			cur_dest[2] = base64[in >> 6];
			in &= 0x3F;
			cur_dest[3] = base64[in];

			cur_dest += 4;
			cur_src += 3;
		}
	}
	// To save time, we didn't update szdest or szsrc in the loop.  So do it now.
	szdest = limit_dest - cur_dest;
	szsrc = limit_src - cur_src;

	/* now deal with the tail (<=3 bytes) */
	switch(szsrc) {
		case 0:
		    // Nothing left; nothing more to do.
		    break;
		case 1: {
		    // One byte left: this encodes to two characters, and (optionally)
		    // two pad characters to round out the four-character cypherblock.
		    if(szdest < 2) return 0;
		    uint32_t in = cur_src[0];
		    cur_dest[0] = base64[in >> 2];
		    in &= 0x3;
		    cur_dest[1] = base64[in << 4];
		    cur_dest += 2;
		    szdest -= 2;
		    if(do_padding) {
			    if(szdest < 2) return 0;
			    cur_dest[0] = kPad64;
			    cur_dest[1] = kPad64;
			    cur_dest += 2;
			    szdest -= 2;
		    }
		    break;
	    }
		case 2: {
		    // Two bytes left: this encodes to three characters, and (optionally)
		    // one pad character to round out the four-character cypherblock.
		    if(szdest < 3) return 0;
		    uint32_t in = absl::big_endian::Load16(cur_src);
		    cur_dest[0] = base64[in >> 10];
		    in &= 0x3FF;
		    cur_dest[1] = base64[in >> 4];
		    in &= 0x00F;
		    cur_dest[2] = base64[in << 2];
		    cur_dest += 3;
		    szdest -= 3;
		    if(do_padding) {
			    if(szdest < 1) return 0;
			    cur_dest[0] = kPad64;
			    cur_dest += 1;
			    szdest -= 1;
		    }
		    break;
	    }
		case 3: {
		    // Three bytes left: same as in the big loop above.  We can't do this in
		    // the loop because the loop above always reads 4 bytes, and the fourth
		    // byte is past the end of the input.
		    if(szdest < 4) return 0;
		    uint32_t in = (cur_src[0] << 16) + absl::big_endian::Load16(cur_src + 1);
		    cur_dest[0] = base64[in >> 18];
		    in &= 0x3FFFF;
		    cur_dest[1] = base64[in >> 12];
		    in &= 0xFFF;
		    cur_dest[2] = base64[in >> 6];
		    in &= 0x3F;
		    cur_dest[3] = base64[in];
		    cur_dest += 4;
		    szdest -= 4;
		    break;
	    }
		default:
		    // Should not be reached: blocks of 4 bytes are handled
		    // in the while loop before this switch statement.
		    ABSL_RAW_LOG(FATAL, "Logic problem? szsrc = %zu", szsrc);
		    break;
	}
	return (cur_dest - dest);
}
}  // namespace strings_internal
ABSL_NAMESPACE_END
}  // namespace absl
