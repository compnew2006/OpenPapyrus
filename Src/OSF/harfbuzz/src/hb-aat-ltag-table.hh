/*
 * Copyright © 2018  Ebrahim Byagowi
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 */
#ifndef HB_AAT_LTAG_TABLE_HH
#define HB_AAT_LTAG_TABLE_HH

#include "hb-open-type.hh"

/*
 * ltag -- Language Tag
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6ltag.html
 */
#define HB_AAT_TAG_ltag HB_TAG('l', 't', 'a', 'g')

namespace AAT {
	using namespace OT;

	struct FTStringRange {
		friend struct ltag;

		bool sanitize(hb_sanitize_context_t * c, const void * base) const
		{
			TRACE_SANITIZE(this);
			return_trace(c->check_struct(this) && (base+tag).sanitize(c, length));
		}

protected:
		NNOffsetTo<UnsizedArrayOf<HBUINT8>>
		tag; /* Offset from the start of the table to
		 * the beginning of the string */
		HBUINT16 length; /* String length (in bytes) */
public:
		DEFINE_SIZE_STATIC(4);
	};

	struct ltag {
		static constexpr hb_tag_t tableTag = HB_AAT_TAG_ltag;
		hb_language_t get_language(uint i) const
		{
			const FTStringRange &range = tagRanges[i];
			return hb_language_from_string((const char*)(this+range.tag).arrayZ, range.length);
		}
		bool sanitize(hb_sanitize_context_t * c) const
		{
			TRACE_SANITIZE(this);
			return_trace(LIKELY(c->check_struct(this) && version >= 1 && tagRanges.sanitize(c, this)));
		}
protected:
		HBUINT32 version; /* Table version; currently 1 */
		HBUINT32 flags; /* Table flags; currently none defined */
		LArrayOf<FTStringRange>
		tagRanges; /* Range for each tag's string */
public:
		DEFINE_SIZE_ARRAY(12, tagRanges);
	};
} /* namespace AAT */

#endif /* HB_AAT_LTAG_TABLE_HH */
