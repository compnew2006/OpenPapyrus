// sjis.c -  Oniguruma (regular expression library)
// Copyright (c) 2002-2020  K.Kosako All rights reserved.
//
#include "regint.h"
#pragma hdrstop

static const int EncLen_SJIS[] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1
};

static const char SJIS_CAN_BE_TRAIL_TABLE[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0
};

#define SJIS_ISMB_FIRST(byte)  (EncLen_SJIS[byte] > 1)
#define SJIS_ISMB_TRAIL(byte)  SJIS_CAN_BE_TRAIL_TABLE[(byte)]

static int mbc_enc_len(const uchar * p)
{
	return EncLen_SJIS[*p];
}

static int is_valid_mbc_string(const uchar * p, const uchar * end)
{
	while(p < end) {
		if(*p < 0x80) {
			p++;
		}
		else if(*p < 0xa1) {
			if(*p == 0xa0 || *p == 0x80)
				return FALSE;
			p++;
			if(p >= end) return FALSE;
			if(*p < 0x40 || *p > 0xfc || *p == 0x7f)
				return FALSE;
			p++;
		}
		else if(*p < 0xe0) {
			p++;
		}
		else if(*p < 0xfd) {
			p++;
			if(p >= end) return FALSE;
			if(*p < 0x40 || *p > 0xfc || *p == 0x7f)
				return FALSE;
			p++;
		}
		else
			return FALSE;
	}

	return TRUE;
}

static int code_to_mbclen(OnigCodePoint code)
{
	if(code < 256) {
		if(EncLen_SJIS[(int)code] == 1)
			return 1;
	}
	else if(code < 0x10000) {
		if(EncLen_SJIS[(int)(code >>  8) & 0xff] == 2)
			return 2;
	}
	return ONIGERR_INVALID_CODE_POINT_VALUE;
}

static OnigCodePoint mbc_to_code(const uchar * p, const uchar * end)
{
	int i;
	int len = enclen(ONIG_ENCODING_SJIS, p);
	int c = *p++;
	OnigCodePoint n = c;
	if(len == 1) 
		return n;
	for(i = 1; i < len; i++) {
		if(p >= end) 
			break;
		c = *p++;
		n <<= 8;  n += c;
	}
	return n;
}

static int code_to_mbc(OnigCodePoint code, uchar * buf)
{
	uchar * p = buf;
	if((code & 0xff00) != 0) 
		*p++ = (uchar)(((code >>  8) & 0xff));
	*p++ = (uchar)(code & 0xff);
	return (int)(p - buf);
}

static int mbc_case_fold(OnigCaseFoldType flag ARG_UNUSED, const uchar ** pp, const uchar * end ARG_UNUSED, uchar * lower)
{
	const uchar * p = *pp;
	if(ONIGENC_IS_MBC_ASCII(p)) {
		*lower = ONIGENC_ASCII_CODE_TO_LOWER_CASE(*p);
		(*pp)++;
		return 1;
	}
	else {
		int i;
		int len = enclen(ONIG_ENCODING_SJIS, p);
		for(i = 0; i < len; i++) {
			*lower++ = *p++;
		}
		(*pp) += len;
		return len; /* return byte length of converted char to lower */
	}
}

static uchar * left_adjust_char_head(const uchar * start, const uchar * s)
{
	const uchar * p;
	int len;
	if(s <= start) 
		return (uchar *)s;
	p = s;
	if(SJIS_ISMB_TRAIL(*p)) {
		while(p > start) {
			if(!SJIS_ISMB_FIRST(*--p)) {
				p++;
				break;
			}
		}
	}
	len = enclen(ONIG_ENCODING_SJIS, p);
	if(p + len > s) 
		return (uchar *)p;
	p += len;
	return (uchar *)(p + ((s - p) & ~1));
}

static int is_allowed_reverse_match(const uchar * s, const uchar * end ARG_UNUSED)
{
	const uchar c = *s;
	return (SJIS_ISMB_TRAIL(c) ? FALSE : TRUE);
}

static const OnigCodePoint CR_Hiragana[] = {
	1,
	0x829f, 0x82f1
}; /* CR_Hiragana */

static const OnigCodePoint CR_Katakana[] = {
	4,
	0x00a6, 0x00af,
	0x00b1, 0x00dd,
	0x8340, 0x837e,
	0x8380, 0x8396,
}; /* CR_Katakana */

static const OnigCodePoint* PropertyList[] = {
	CR_Hiragana,
	CR_Katakana
};

static int property_name_to_ctype(OnigEncoding enc, uchar * p, uchar * end)
{
	struct PropertyNameCtype* pc;
	int len = (int)(end - p);
	char q[32];
	if(len < sizeof(q) - 1) {
		memcpy(q, p, (size_t)len);
		q[len] = '\0';
		pc = onigenc_sjis_lookup_property_name(q, len);
		if(pc != 0)
			return pc->ctype;
	}
	return ONIGERR_INVALID_CHAR_PROPERTY_NAME;
}

static int is_code_ctype(OnigCodePoint code, uint ctype)
{
	if(ctype <= ONIGENC_MAX_STD_CTYPE) {
		if(code < 128)
			return ONIGENC_IS_ASCII_CODE_CTYPE(code, ctype);
		else {
			if(CTYPE_IS_WORD_GRAPH_PRINT(ctype)) {
				return (code_to_mbclen(code) > 1 ? TRUE : FALSE);
			}
		}
	}
	else {
		ctype -= (ONIGENC_MAX_STD_CTYPE + 1);
		if(ctype >= (uint)(sizeof(PropertyList)/sizeof(PropertyList[0])))
			return ONIGERR_TYPE_BUG;
		return onig_is_in_code_range((uchar *)PropertyList[ctype], code);
	}
	return FALSE;
}

static int get_ctype_code_range(OnigCtype ctype, OnigCodePoint* sb_out, const OnigCodePoint* ranges[])
{
	if(ctype <= ONIGENC_MAX_STD_CTYPE) {
		return ONIG_NO_SUPPORT_CONFIG;
	}
	else {
		*sb_out = 0x80;
		ctype -= (ONIGENC_MAX_STD_CTYPE + 1);
		if(ctype >= (OnigCtype)(sizeof(PropertyList)/sizeof(PropertyList[0])))
			return ONIGERR_TYPE_BUG;
		*ranges = PropertyList[ctype];
		return 0;
	}
}

OnigEncodingType OnigEncodingSJIS = {
	mbc_enc_len,
	"Shift_JIS", /* name */
	2,       /* max enc length */
	1,       /* min enc length */
	onigenc_is_mbc_newline_0x0a,
	mbc_to_code,
	code_to_mbclen,
	code_to_mbc,
	mbc_case_fold,
	onigenc_ascii_apply_all_case_fold,
	onigenc_ascii_get_case_fold_codes_by_str,
	property_name_to_ctype,
	is_code_ctype,
	get_ctype_code_range,
	left_adjust_char_head,
	is_allowed_reverse_match,
	NULL, /* init */
	NULL, /* is_initialized */
	is_valid_mbc_string,
	ENC_FLAG_ASCII_COMPATIBLE|ENC_FLAG_SKIP_OFFSET_1_OR_0,
	0, 0
};
