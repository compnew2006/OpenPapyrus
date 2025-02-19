/*************************************************
*      Perl-Compatible Regular Expressions       *
*************************************************/

/* PCRE is a library of functions to support regular expressions whose syntax
and semantics are as close as possible to those of the Perl 5 language.

                       Written by Philip Hazel
           Copyright (c) 1997-2017 University of Cambridge

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/
#include "pcre_internal.h"
#pragma hdrstop
/* @sobolev
#ifndef PCRE_INCLUDED
	// This module contains some fixed tables that are used by more than one of the
	// PCRE code modules. The tables are also #included by the pcretest program, which
	// uses macros to change their names from _pcre_xxx to xxxx, thereby avoiding name clashes with the library. 
	#ifdef HAVE_CONFIG_H
		#include "config.h"
	#endif
	#include "pcre_internal.h"
#endif 
*/

/* Table of sizes for the fixed-length opcodes. It's defined in a macro so that
the definition is next to the definition of the opcodes in pcre_internal.h. */

const pcre_uint8 PRIV(OP_lengths)[] = { OP_LENGTHS };

/* Tables of horizontal and vertical whitespace characters, suitable for
adding to classes. */

const uint32 PRIV(hspace_list)[] = { HSPACE_LIST };
const uint32 PRIV(vspace_list)[] = { VSPACE_LIST };

/*************************************************
*           Tables for UTF-8 support             *
*************************************************/

/* These are the breakpoints for different numbers of bytes in a UTF-8
character. */

#if(defined SUPPORT_UTF && defined COMPILE_PCRE8) || (defined PCRE_INCLUDED && (defined SUPPORT_PCRE16 || defined SUPPORT_PCRE32))

/* These tables are also required by pcretest in 16- or 32-bit mode. */

const int PRIV(utf8_table1)[] =
  { 0x7f, 0x7ff, 0xffff, 0x1fffff, 0x3ffffff, 0x7fffffff};

const int PRIV(utf8_table1_size) = sizeof(PRIV(utf8_table1)) / sizeof(int);

/* These are the indicator bits and the mask for the data bits to set in the
first byte of a character, indexed by the number of additional bytes. */

const int PRIV(utf8_table2)[] = { 0,    0xc0, 0xe0, 0xf0, 0xf8, 0xfc};
const int PRIV(utf8_table3)[] = { 0xff, 0x1f, 0x0f, 0x07, 0x03, 0x01};

/* Table of the number of extra bytes, indexed by the first byte masked with
0x3f. The highest number for a valid UTF-8 first byte is in fact 0x3d. */

const pcre_uint8 PRIV(utf8_table4)[] = {
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  3,3,3,3,3,3,3,3,4,4,4,4,5,5,5,5 };

#endif /* (SUPPORT_UTF && COMPILE_PCRE8) || (PCRE_INCLUDED && SUPPORT_PCRE[16|32])*/

#ifdef SUPPORT_UTF

/* Table to translate from particular type value to the general value. */

const uint32 PRIV(ucp_gentype)[] = {
  ucp_C, ucp_C, ucp_C, ucp_C, ucp_C,  /* Cc, Cf, Cn, Co, Cs */
  ucp_L, ucp_L, ucp_L, ucp_L, ucp_L,  /* Ll, Lu, Lm, Lo, Lt */
  ucp_M, ucp_M, ucp_M,                /* Mc, Me, Mn */
  ucp_N, ucp_N, ucp_N,                /* Nd, Nl, No */
  ucp_P, ucp_P, ucp_P, ucp_P, ucp_P,  /* Pc, Pd, Pe, Pf, Pi */
  ucp_P, ucp_P,                       /* Ps, Po */
  ucp_S, ucp_S, ucp_S, ucp_S,         /* Sc, Sk, Sm, So */
  ucp_Z, ucp_Z, ucp_Z                 /* Zl, Zp, Zs */
};

/* This table encodes the rules for finding the end of an extended grapheme
cluster. Every code point has a grapheme break property which is one of the
ucp_gbXX values defined in ucp.h. The 2-dimensional table is indexed by the
properties of two adjacent code points. The left property selects a word from
the table, and the right property selects a bit from that word like this:

  ucp_gbtable[left-property] & (1 << right-property)

The value is non-zero if a grapheme break is NOT permitted between the relevant
two code points. The breaking rules are as follows:

1. Break at the start and end of text (pretty obviously).

2. Do not break between a CR and LF; otherwise, break before and   after
   controls.

3. Do not break Hangul syllable sequences, the rules for which are:

    L may be followed by L, V, LV or LVT
    LV or V may be followed by V or T
    LVT or T may be followed by T

4. Do not break before extending characters.

The next two rules are only for extended grapheme clusters (but that's what we
are implementing).

5. Do not break before SpacingMarks.

6. Do not break after Prepend characters.

7. Otherwise, break everywhere.
*/

const uint32 PRIV(ucp_gbtable[]) = {
   (1<<ucp_gbLF),                                           /* 0 CR */
   0,                                                       /* 1 LF */
   0,                                                       /* 2 Control */
   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark),                /* 3 Extend */
   (1<<ucp_gbExtend)|(1<<ucp_gbPrepend)|                    /* 4 Prepend */
     (1<<ucp_gbSpacingMark)|(1<<ucp_gbL)|
     (1<<ucp_gbV)|(1<<ucp_gbT)|(1<<ucp_gbLV)|
     (1<<ucp_gbLVT)|(1<<ucp_gbOther),

   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark),                /* 5 SpacingMark */
   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)|(1<<ucp_gbL)|   /* 6 L */
     (1<<ucp_gbV)|(1<<ucp_gbLV)|(1<<ucp_gbLVT),

   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)|(1<<ucp_gbV)|   /* 7 V */
     (1<<ucp_gbT),

   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)|(1<<ucp_gbT),   /* 8 T */
   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)|(1<<ucp_gbV)|   /* 9 LV */
     (1<<ucp_gbT),

   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)|(1<<ucp_gbT),   /* 10 LVT */
   (1<<ucp_gbRegionalIndicator),                            /* 11 RegionalIndicator */
   (1<<ucp_gbExtend)|(1<<ucp_gbSpacingMark)                 /* 12 Other */
};

#ifdef SUPPORT_JIT
/* This table reverses PRIV(ucp_gentype). We can save the cost
of a memory load. */

const int PRIV(ucp_typerange)[] = {
  ucp_Cc, ucp_Cs,
  ucp_Ll, ucp_Lu,
  ucp_Mc, ucp_Mn,
  ucp_Nd, ucp_No,
  ucp_Pc, ucp_Ps,
  ucp_Sc, ucp_So,
  ucp_Zl, ucp_Zs,
};
#endif /* SUPPORT_JIT */

/* The pcre_utt[] table below translates Unicode property names into type and
code values. It is searched by binary chop, so must be in collating sequence of
name. Originally, the table contained pointers to the name strings in the first
field of each entry. However, that leads to a large number of relocations when
a shared library is dynamically loaded. A significant reduction is made by
putting all the names into a single, large string and then using offsets in the
table itself. Maintenance is more error-prone, but frequent changes to this
data are unlikely.

July 2008: There is now a script called maint/GenerateUtt.py that can be used
to generate this data automatically instead of maintaining it by hand.

The script was updated in March 2009 to generate a new EBCDIC-compliant
version. Like all other character and string literals that are compared against
the regular expression pattern, we must use STR_ macros instead of literal
strings to make sure that UTF-8 support works on EBCDIC platforms. */

#define STRING_Any0 STR_A STR_n STR_y "\0"
#define STRING_Arabic0 STR_A STR_r STR_a STR_b STR_i STR_c "\0"
#define STRING_Armenian0 STR_A STR_r STR_m STR_e STR_n STR_i STR_a STR_n "\0"
#define STRING_Avestan0 STR_A STR_v STR_e STR_s STR_t STR_a STR_n "\0"
#define STRING_Balinese0 STR_B STR_a STR_l STR_i STR_n STR_e STR_s STR_e "\0"
#define STRING_Bamum0 STR_B STR_a STR_m STR_u STR_m "\0"
#define STRING_Bassa_Vah0 STR_B STR_a STR_s STR_s STR_a STR_UNDERSCORE STR_V STR_a STR_h "\0"
#define STRING_Batak0 STR_B STR_a STR_t STR_a STR_k "\0"
#define STRING_Bengali0 STR_B STR_e STR_n STR_g STR_a STR_l STR_i "\0"
#define STRING_Bopomofo0 STR_B STR_o STR_p STR_o STR_m STR_o STR_f STR_o "\0"
#define STRING_Brahmi0 STR_B STR_r STR_a STR_h STR_m STR_i "\0"
#define STRING_Braille0 STR_B STR_r STR_a STR_i STR_l STR_l STR_e "\0"
#define STRING_Buginese0 STR_B STR_u STR_g STR_i STR_n STR_e STR_s STR_e "\0"
#define STRING_Buhid0 STR_B STR_u STR_h STR_i STR_d "\0"
#define STRING_C0 STR_C "\0"
#define STRING_Canadian_Aboriginal0 STR_C STR_a STR_n STR_a STR_d STR_i STR_a STR_n STR_UNDERSCORE STR_A STR_b STR_o STR_r STR_i STR_g STR_i STR_n STR_a STR_l "\0"
#define STRING_Carian0 STR_C STR_a STR_r STR_i STR_a STR_n "\0"
#define STRING_Caucasian_Albanian0 STR_C STR_a STR_u STR_c STR_a STR_s STR_i STR_a STR_n STR_UNDERSCORE STR_A STR_l STR_b STR_a STR_n STR_i STR_a STR_n "\0"
#define STRING_Cc0 STR_C STR_c "\0"
#define STRING_Cf0 STR_C STR_f "\0"
#define STRING_Chakma0 STR_C STR_h STR_a STR_k STR_m STR_a "\0"
#define STRING_Cham0 STR_C STR_h STR_a STR_m "\0"
#define STRING_Cherokee0 STR_C STR_h STR_e STR_r STR_o STR_k STR_e STR_e "\0"
#define STRING_Cn0 STR_C STR_n "\0"
#define STRING_Co0 STR_C STR_o "\0"
#define STRING_Common0 STR_C STR_o STR_m STR_m STR_o STR_n "\0"
#define STRING_Coptic0 STR_C STR_o STR_p STR_t STR_i STR_c "\0"
#define STRING_Cs0 STR_C STR_s "\0"
#define STRING_Cuneiform0 STR_C STR_u STR_n STR_e STR_i STR_f STR_o STR_r STR_m "\0"
#define STRING_Cypriot0 STR_C STR_y STR_p STR_r STR_i STR_o STR_t "\0"
#define STRING_Cyrillic0 STR_C STR_y STR_r STR_i STR_l STR_l STR_i STR_c "\0"
#define STRING_Deseret0 STR_D STR_e STR_s STR_e STR_r STR_e STR_t "\0"
#define STRING_Devanagari0 STR_D STR_e STR_v STR_a STR_n STR_a STR_g STR_a STR_r STR_i "\0"
#define STRING_Duployan0 STR_D STR_u STR_p STR_l STR_o STR_y STR_a STR_n "\0"
#define STRING_Egyptian_Hieroglyphs0 STR_E STR_g STR_y STR_p STR_t STR_i STR_a STR_n STR_UNDERSCORE STR_H STR_i STR_e STR_r STR_o STR_g STR_l STR_y STR_p STR_h STR_s "\0"
#define STRING_Elbasan0 STR_E STR_l STR_b STR_a STR_s STR_a STR_n "\0"
#define STRING_Ethiopic0 STR_E STR_t STR_h STR_i STR_o STR_p STR_i STR_c "\0"
#define STRING_Georgian0 STR_G STR_e STR_o STR_r STR_g STR_i STR_a STR_n "\0"
#define STRING_Glagolitic0 STR_G STR_l STR_a STR_g STR_o STR_l STR_i STR_t STR_i STR_c "\0"
#define STRING_Gothic0 STR_G STR_o STR_t STR_h STR_i STR_c "\0"
#define STRING_Grantha0 STR_G STR_r STR_a STR_n STR_t STR_h STR_a "\0"
#define STRING_Greek0 STR_G STR_r STR_e STR_e STR_k "\0"
#define STRING_Gujarati0 STR_G STR_u STR_j STR_a STR_r STR_a STR_t STR_i "\0"
#define STRING_Gurmukhi0 STR_G STR_u STR_r STR_m STR_u STR_k STR_h STR_i "\0"
#define STRING_Han0 STR_H STR_a STR_n "\0"
#define STRING_Hangul0 STR_H STR_a STR_n STR_g STR_u STR_l "\0"
#define STRING_Hanunoo0 STR_H STR_a STR_n STR_u STR_n STR_o STR_o "\0"
#define STRING_Hebrew0 STR_H STR_e STR_b STR_r STR_e STR_w "\0"
#define STRING_Hiragana0 STR_H STR_i STR_r STR_a STR_g STR_a STR_n STR_a "\0"
#define STRING_Imperial_Aramaic0 STR_I STR_m STR_p STR_e STR_r STR_i STR_a STR_l STR_UNDERSCORE STR_A STR_r STR_a STR_m STR_a STR_i STR_c "\0"
#define STRING_Inherited0 STR_I STR_n STR_h STR_e STR_r STR_i STR_t STR_e STR_d "\0"
#define STRING_Inscriptional_Pahlavi0 STR_I STR_n STR_s STR_c STR_r STR_i STR_p STR_t STR_i STR_o STR_n STR_a STR_l STR_UNDERSCORE STR_P STR_a STR_h STR_l STR_a STR_v STR_i "\0"
#define STRING_Inscriptional_Parthian0 STR_I STR_n STR_s STR_c STR_r STR_i STR_p STR_t STR_i STR_o STR_n STR_a STR_l STR_UNDERSCORE STR_P STR_a STR_r STR_t STR_h STR_i STR_a STR_n "\0"
#define STRING_Javanese0 STR_J STR_a STR_v STR_a STR_n STR_e STR_s STR_e "\0"
#define STRING_Kaithi0 STR_K STR_a STR_i STR_t STR_h STR_i "\0"
#define STRING_Kannada0 STR_K STR_a STR_n STR_n STR_a STR_d STR_a "\0"
#define STRING_Katakana0 STR_K STR_a STR_t STR_a STR_k STR_a STR_n STR_a "\0"
#define STRING_Kayah_Li0 STR_K STR_a STR_y STR_a STR_h STR_UNDERSCORE STR_L STR_i "\0"
#define STRING_Kharoshthi0 STR_K STR_h STR_a STR_r STR_o STR_s STR_h STR_t STR_h STR_i "\0"
#define STRING_Khmer0 STR_K STR_h STR_m STR_e STR_r "\0"
#define STRING_Khojki0 STR_K STR_h STR_o STR_j STR_k STR_i "\0"
#define STRING_Khudawadi0 STR_K STR_h STR_u STR_d STR_a STR_w STR_a STR_d STR_i "\0"
#define STRING_L0 STR_L "\0"
#define STRING_L_AMPERSAND0 STR_L STR_AMPERSAND "\0"
#define STRING_Lao0 STR_L STR_a STR_o "\0"
#define STRING_Latin0 STR_L STR_a STR_t STR_i STR_n "\0"
#define STRING_Lepcha0 STR_L STR_e STR_p STR_c STR_h STR_a "\0"
#define STRING_Limbu0 STR_L STR_i STR_m STR_b STR_u "\0"
#define STRING_Linear_A0 STR_L STR_i STR_n STR_e STR_a STR_r STR_UNDERSCORE STR_A "\0"
#define STRING_Linear_B0 STR_L STR_i STR_n STR_e STR_a STR_r STR_UNDERSCORE STR_B "\0"
#define STRING_Lisu0 STR_L STR_i STR_s STR_u "\0"
#define STRING_Ll0 STR_L STR_l "\0"
#define STRING_Lm0 STR_L STR_m "\0"
#define STRING_Lo0 STR_L STR_o "\0"
#define STRING_Lt0 STR_L STR_t "\0"
#define STRING_Lu0 STR_L STR_u "\0"
#define STRING_Lycian0 STR_L STR_y STR_c STR_i STR_a STR_n "\0"
#define STRING_Lydian0 STR_L STR_y STR_d STR_i STR_a STR_n "\0"
#define STRING_M0 STR_M "\0"
#define STRING_Mahajani0 STR_M STR_a STR_h STR_a STR_j STR_a STR_n STR_i "\0"
#define STRING_Malayalam0 STR_M STR_a STR_l STR_a STR_y STR_a STR_l STR_a STR_m "\0"
#define STRING_Mandaic0 STR_M STR_a STR_n STR_d STR_a STR_i STR_c "\0"
#define STRING_Manichaean0 STR_M STR_a STR_n STR_i STR_c STR_h STR_a STR_e STR_a STR_n "\0"
#define STRING_Mc0 STR_M STR_c "\0"
#define STRING_Me0 STR_M STR_e "\0"
#define STRING_Meetei_Mayek0 STR_M STR_e STR_e STR_t STR_e STR_i STR_UNDERSCORE STR_M STR_a STR_y STR_e STR_k "\0"
#define STRING_Mende_Kikakui0 STR_M STR_e STR_n STR_d STR_e STR_UNDERSCORE STR_K STR_i STR_k STR_a STR_k STR_u STR_i "\0"
#define STRING_Meroitic_Cursive0 STR_M STR_e STR_r STR_o STR_i STR_t STR_i STR_c STR_UNDERSCORE STR_C STR_u STR_r STR_s STR_i STR_v STR_e "\0"
#define STRING_Meroitic_Hieroglyphs0 STR_M STR_e STR_r STR_o STR_i STR_t STR_i STR_c STR_UNDERSCORE STR_H STR_i STR_e STR_r STR_o STR_g STR_l STR_y STR_p STR_h STR_s "\0"
#define STRING_Miao0 STR_M STR_i STR_a STR_o "\0"
#define STRING_Mn0 STR_M STR_n "\0"
#define STRING_Modi0 STR_M STR_o STR_d STR_i "\0"
#define STRING_Mongolian0 STR_M STR_o STR_n STR_g STR_o STR_l STR_i STR_a STR_n "\0"
#define STRING_Mro0 STR_M STR_r STR_o "\0"
#define STRING_Myanmar0 STR_M STR_y STR_a STR_n STR_m STR_a STR_r "\0"
#define STRING_N0 STR_N "\0"
#define STRING_Nabataean0 STR_N STR_a STR_b STR_a STR_t STR_a STR_e STR_a STR_n "\0"
#define STRING_Nd0 STR_N STR_d "\0"
#define STRING_New_Tai_Lue0 STR_N STR_e STR_w STR_UNDERSCORE STR_T STR_a STR_i STR_UNDERSCORE STR_L STR_u STR_e "\0"
#define STRING_Nko0 STR_N STR_k STR_o "\0"
#define STRING_Nl0 STR_N STR_l "\0"
#define STRING_No0 STR_N STR_o "\0"
#define STRING_Ogham0 STR_O STR_g STR_h STR_a STR_m "\0"
#define STRING_Ol_Chiki0 STR_O STR_l STR_UNDERSCORE STR_C STR_h STR_i STR_k STR_i "\0"
#define STRING_Old_Italic0 STR_O STR_l STR_d STR_UNDERSCORE STR_I STR_t STR_a STR_l STR_i STR_c "\0"
#define STRING_Old_North_Arabian0 STR_O STR_l STR_d STR_UNDERSCORE STR_N STR_o STR_r STR_t STR_h STR_UNDERSCORE STR_A STR_r STR_a STR_b STR_i STR_a STR_n "\0"
#define STRING_Old_Permic0 STR_O STR_l STR_d STR_UNDERSCORE STR_P STR_e STR_r STR_m STR_i STR_c "\0"
#define STRING_Old_Persian0 STR_O STR_l STR_d STR_UNDERSCORE STR_P STR_e STR_r STR_s STR_i STR_a STR_n "\0"
#define STRING_Old_South_Arabian0 STR_O STR_l STR_d STR_UNDERSCORE STR_S STR_o STR_u STR_t STR_h STR_UNDERSCORE STR_A STR_r STR_a STR_b STR_i STR_a STR_n "\0"
#define STRING_Old_Turkic0 STR_O STR_l STR_d STR_UNDERSCORE STR_T STR_u STR_r STR_k STR_i STR_c "\0"
#define STRING_Oriya0 STR_O STR_r STR_i STR_y STR_a "\0"
#define STRING_Osmanya0 STR_O STR_s STR_m STR_a STR_n STR_y STR_a "\0"
#define STRING_P0 STR_P "\0"
#define STRING_Pahawh_Hmong0 STR_P STR_a STR_h STR_a STR_w STR_h STR_UNDERSCORE STR_H STR_m STR_o STR_n STR_g "\0"
#define STRING_Palmyrene0 STR_P STR_a STR_l STR_m STR_y STR_r STR_e STR_n STR_e "\0"
#define STRING_Pau_Cin_Hau0 STR_P STR_a STR_u STR_UNDERSCORE STR_C STR_i STR_n STR_UNDERSCORE STR_H STR_a STR_u "\0"
#define STRING_Pc0 STR_P STR_c "\0"
#define STRING_Pd0 STR_P STR_d "\0"
#define STRING_Pe0 STR_P STR_e "\0"
#define STRING_Pf0 STR_P STR_f "\0"
#define STRING_Phags_Pa0 STR_P STR_h STR_a STR_g STR_s STR_UNDERSCORE STR_P STR_a "\0"
#define STRING_Phoenician0 STR_P STR_h STR_o STR_e STR_n STR_i STR_c STR_i STR_a STR_n "\0"
#define STRING_Pi0 STR_P STR_i "\0"
#define STRING_Po0 STR_P STR_o "\0"
#define STRING_Ps0 STR_P STR_s "\0"
#define STRING_Psalter_Pahlavi0 STR_P STR_s STR_a STR_l STR_t STR_e STR_r STR_UNDERSCORE STR_P STR_a STR_h STR_l STR_a STR_v STR_i "\0"
#define STRING_Rejang0 STR_R STR_e STR_j STR_a STR_n STR_g "\0"
#define STRING_Runic0 STR_R STR_u STR_n STR_i STR_c "\0"
#define STRING_S0 STR_S "\0"
#define STRING_Samaritan0 STR_S STR_a STR_m STR_a STR_r STR_i STR_t STR_a STR_n "\0"
#define STRING_Saurashtra0 STR_S STR_a STR_u STR_r STR_a STR_s STR_h STR_t STR_r STR_a "\0"
#define STRING_Sc0 STR_S STR_c "\0"
#define STRING_Sharada0 STR_S STR_h STR_a STR_r STR_a STR_d STR_a "\0"
#define STRING_Shavian0 STR_S STR_h STR_a STR_v STR_i STR_a STR_n "\0"
#define STRING_Siddham0 STR_S STR_i STR_d STR_d STR_h STR_a STR_m "\0"
#define STRING_Sinhala0 STR_S STR_i STR_n STR_h STR_a STR_l STR_a "\0"
#define STRING_Sk0 STR_S STR_k "\0"
#define STRING_Sm0 STR_S STR_m "\0"
#define STRING_So0 STR_S STR_o "\0"
#define STRING_Sora_Sompeng0 STR_S STR_o STR_r STR_a STR_UNDERSCORE STR_S STR_o STR_m STR_p STR_e STR_n STR_g "\0"
#define STRING_Sundanese0 STR_S STR_u STR_n STR_d STR_a STR_n STR_e STR_s STR_e "\0"
#define STRING_Syloti_Nagri0 STR_S STR_y STR_l STR_o STR_t STR_i STR_UNDERSCORE STR_N STR_a STR_g STR_r STR_i "\0"
#define STRING_Syriac0 STR_S STR_y STR_r STR_i STR_a STR_c "\0"
#define STRING_Tagalog0 STR_T STR_a STR_g STR_a STR_l STR_o STR_g "\0"
#define STRING_Tagbanwa0 STR_T STR_a STR_g STR_b STR_a STR_n STR_w STR_a "\0"
#define STRING_Tai_Le0 STR_T STR_a STR_i STR_UNDERSCORE STR_L STR_e "\0"
#define STRING_Tai_Tham0 STR_T STR_a STR_i STR_UNDERSCORE STR_T STR_h STR_a STR_m "\0"
#define STRING_Tai_Viet0 STR_T STR_a STR_i STR_UNDERSCORE STR_V STR_i STR_e STR_t "\0"
#define STRING_Takri0 STR_T STR_a STR_k STR_r STR_i "\0"
#define STRING_Tamil0 STR_T STR_a STR_m STR_i STR_l "\0"
#define STRING_Telugu0 STR_T STR_e STR_l STR_u STR_g STR_u "\0"
#define STRING_Thaana0 STR_T STR_h STR_a STR_a STR_n STR_a "\0"
#define STRING_Thai0 STR_T STR_h STR_a STR_i "\0"
#define STRING_Tibetan0 STR_T STR_i STR_b STR_e STR_t STR_a STR_n "\0"
#define STRING_Tifinagh0 STR_T STR_i STR_f STR_i STR_n STR_a STR_g STR_h "\0"
#define STRING_Tirhuta0 STR_T STR_i STR_r STR_h STR_u STR_t STR_a "\0"
#define STRING_Ugaritic0 STR_U STR_g STR_a STR_r STR_i STR_t STR_i STR_c "\0"
#define STRING_Vai0 STR_V STR_a STR_i "\0"
#define STRING_Warang_Citi0 STR_W STR_a STR_r STR_a STR_n STR_g STR_UNDERSCORE STR_C STR_i STR_t STR_i "\0"
#define STRING_Xan0 STR_X STR_a STR_n "\0"
#define STRING_Xps0 STR_X STR_p STR_s "\0"
#define STRING_Xsp0 STR_X STR_s STR_p "\0"
#define STRING_Xuc0 STR_X STR_u STR_c "\0"
#define STRING_Xwd0 STR_X STR_w STR_d "\0"
#define STRING_Yi0 STR_Y STR_i "\0"
#define STRING_Z0 STR_Z "\0"
#define STRING_Zl0 STR_Z STR_l "\0"
#define STRING_Zp0 STR_Z STR_p "\0"
#define STRING_Zs0 STR_Z STR_s "\0"

const char PRIV(utt_names)[] =
  STRING_Any0
  STRING_Arabic0
  STRING_Armenian0
  STRING_Avestan0
  STRING_Balinese0
  STRING_Bamum0
  STRING_Bassa_Vah0
  STRING_Batak0
  STRING_Bengali0
  STRING_Bopomofo0
  STRING_Brahmi0
  STRING_Braille0
  STRING_Buginese0
  STRING_Buhid0
  STRING_C0
  STRING_Canadian_Aboriginal0
  STRING_Carian0
  STRING_Caucasian_Albanian0
  STRING_Cc0
  STRING_Cf0
  STRING_Chakma0
  STRING_Cham0
  STRING_Cherokee0
  STRING_Cn0
  STRING_Co0
  STRING_Common0
  STRING_Coptic0
  STRING_Cs0
  STRING_Cuneiform0
  STRING_Cypriot0
  STRING_Cyrillic0
  STRING_Deseret0
  STRING_Devanagari0
  STRING_Duployan0
  STRING_Egyptian_Hieroglyphs0
  STRING_Elbasan0
  STRING_Ethiopic0
  STRING_Georgian0
  STRING_Glagolitic0
  STRING_Gothic0
  STRING_Grantha0
  STRING_Greek0
  STRING_Gujarati0
  STRING_Gurmukhi0
  STRING_Han0
  STRING_Hangul0
  STRING_Hanunoo0
  STRING_Hebrew0
  STRING_Hiragana0
  STRING_Imperial_Aramaic0
  STRING_Inherited0
  STRING_Inscriptional_Pahlavi0
  STRING_Inscriptional_Parthian0
  STRING_Javanese0
  STRING_Kaithi0
  STRING_Kannada0
  STRING_Katakana0
  STRING_Kayah_Li0
  STRING_Kharoshthi0
  STRING_Khmer0
  STRING_Khojki0
  STRING_Khudawadi0
  STRING_L0
  STRING_L_AMPERSAND0
  STRING_Lao0
  STRING_Latin0
  STRING_Lepcha0
  STRING_Limbu0
  STRING_Linear_A0
  STRING_Linear_B0
  STRING_Lisu0
  STRING_Ll0
  STRING_Lm0
  STRING_Lo0
  STRING_Lt0
  STRING_Lu0
  STRING_Lycian0
  STRING_Lydian0
  STRING_M0
  STRING_Mahajani0
  STRING_Malayalam0
  STRING_Mandaic0
  STRING_Manichaean0
  STRING_Mc0
  STRING_Me0
  STRING_Meetei_Mayek0
  STRING_Mende_Kikakui0
  STRING_Meroitic_Cursive0
  STRING_Meroitic_Hieroglyphs0
  STRING_Miao0
  STRING_Mn0
  STRING_Modi0
  STRING_Mongolian0
  STRING_Mro0
  STRING_Myanmar0
  STRING_N0
  STRING_Nabataean0
  STRING_Nd0
  STRING_New_Tai_Lue0
  STRING_Nko0
  STRING_Nl0
  STRING_No0
  STRING_Ogham0
  STRING_Ol_Chiki0
  STRING_Old_Italic0
  STRING_Old_North_Arabian0
  STRING_Old_Permic0
  STRING_Old_Persian0
  STRING_Old_South_Arabian0
  STRING_Old_Turkic0
  STRING_Oriya0
  STRING_Osmanya0
  STRING_P0
  STRING_Pahawh_Hmong0
  STRING_Palmyrene0
  STRING_Pau_Cin_Hau0
  STRING_Pc0
  STRING_Pd0
  STRING_Pe0
  STRING_Pf0
  STRING_Phags_Pa0
  STRING_Phoenician0
  STRING_Pi0
  STRING_Po0
  STRING_Ps0
  STRING_Psalter_Pahlavi0
  STRING_Rejang0
  STRING_Runic0
  STRING_S0
  STRING_Samaritan0
  STRING_Saurashtra0
  STRING_Sc0
  STRING_Sharada0
  STRING_Shavian0
  STRING_Siddham0
  STRING_Sinhala0
  STRING_Sk0
  STRING_Sm0
  STRING_So0
  STRING_Sora_Sompeng0
  STRING_Sundanese0
  STRING_Syloti_Nagri0
  STRING_Syriac0
  STRING_Tagalog0
  STRING_Tagbanwa0
  STRING_Tai_Le0
  STRING_Tai_Tham0
  STRING_Tai_Viet0
  STRING_Takri0
  STRING_Tamil0
  STRING_Telugu0
  STRING_Thaana0
  STRING_Thai0
  STRING_Tibetan0
  STRING_Tifinagh0
  STRING_Tirhuta0
  STRING_Ugaritic0
  STRING_Vai0
  STRING_Warang_Citi0
  STRING_Xan0
  STRING_Xps0
  STRING_Xsp0
  STRING_Xuc0
  STRING_Xwd0
  STRING_Yi0
  STRING_Z0
  STRING_Zl0
  STRING_Zp0
  STRING_Zs0;

const ucp_type_table PRIV(utt)[] = {
  {   0, PT_ANY, 0 },
  {   4, PT_SC, ucp_Arabic },
  {  11, PT_SC, ucp_Armenian },
  {  20, PT_SC, ucp_Avestan },
  {  28, PT_SC, ucp_Balinese },
  {  37, PT_SC, ucp_Bamum },
  {  43, PT_SC, ucp_Bassa_Vah },
  {  53, PT_SC, ucp_Batak },
  {  59, PT_SC, ucp_Bengali },
  {  67, PT_SC, ucp_Bopomofo },
  {  76, PT_SC, ucp_Brahmi },
  {  83, PT_SC, ucp_Braille },
  {  91, PT_SC, ucp_Buginese },
  { 100, PT_SC, ucp_Buhid },
  { 106, PT_GC, ucp_C },
  { 108, PT_SC, ucp_Canadian_Aboriginal },
  { 128, PT_SC, ucp_Carian },
  { 135, PT_SC, ucp_Caucasian_Albanian },
  { 154, PT_PC, ucp_Cc },
  { 157, PT_PC, ucp_Cf },
  { 160, PT_SC, ucp_Chakma },
  { 167, PT_SC, ucp_Cham },
  { 172, PT_SC, ucp_Cherokee },
  { 181, PT_PC, ucp_Cn },
  { 184, PT_PC, ucp_Co },
  { 187, PT_SC, ucp_Common },
  { 194, PT_SC, ucp_Coptic },
  { 201, PT_PC, ucp_Cs },
  { 204, PT_SC, ucp_Cuneiform },
  { 214, PT_SC, ucp_Cypriot },
  { 222, PT_SC, ucp_Cyrillic },
  { 231, PT_SC, ucp_Deseret },
  { 239, PT_SC, ucp_Devanagari },
  { 250, PT_SC, ucp_Duployan },
  { 259, PT_SC, ucp_Egyptian_Hieroglyphs },
  { 280, PT_SC, ucp_Elbasan },
  { 288, PT_SC, ucp_Ethiopic },
  { 297, PT_SC, ucp_Georgian },
  { 306, PT_SC, ucp_Glagolitic },
  { 317, PT_SC, ucp_Gothic },
  { 324, PT_SC, ucp_Grantha },
  { 332, PT_SC, ucp_Greek },
  { 338, PT_SC, ucp_Gujarati },
  { 347, PT_SC, ucp_Gurmukhi },
  { 356, PT_SC, ucp_Han },
  { 360, PT_SC, ucp_Hangul },
  { 367, PT_SC, ucp_Hanunoo },
  { 375, PT_SC, ucp_Hebrew },
  { 382, PT_SC, ucp_Hiragana },
  { 391, PT_SC, ucp_Imperial_Aramaic },
  { 408, PT_SC, ucp_Inherited },
  { 418, PT_SC, ucp_Inscriptional_Pahlavi },
  { 440, PT_SC, ucp_Inscriptional_Parthian },
  { 463, PT_SC, ucp_Javanese },
  { 472, PT_SC, ucp_Kaithi },
  { 479, PT_SC, ucp_Kannada },
  { 487, PT_SC, ucp_Katakana },
  { 496, PT_SC, ucp_Kayah_Li },
  { 505, PT_SC, ucp_Kharoshthi },
  { 516, PT_SC, ucp_Khmer },
  { 522, PT_SC, ucp_Khojki },
  { 529, PT_SC, ucp_Khudawadi },
  { 539, PT_GC, ucp_L },
  { 541, PT_LAMP, 0 },
  { 544, PT_SC, ucp_Lao },
  { 548, PT_SC, ucp_Latin },
  { 554, PT_SC, ucp_Lepcha },
  { 561, PT_SC, ucp_Limbu },
  { 567, PT_SC, ucp_Linear_A },
  { 576, PT_SC, ucp_Linear_B },
  { 585, PT_SC, ucp_Lisu },
  { 590, PT_PC, ucp_Ll },
  { 593, PT_PC, ucp_Lm },
  { 596, PT_PC, ucp_Lo },
  { 599, PT_PC, ucp_Lt },
  { 602, PT_PC, ucp_Lu },
  { 605, PT_SC, ucp_Lycian },
  { 612, PT_SC, ucp_Lydian },
  { 619, PT_GC, ucp_M },
  { 621, PT_SC, ucp_Mahajani },
  { 630, PT_SC, ucp_Malayalam },
  { 640, PT_SC, ucp_Mandaic },
  { 648, PT_SC, ucp_Manichaean },
  { 659, PT_PC, ucp_Mc },
  { 662, PT_PC, ucp_Me },
  { 665, PT_SC, ucp_Meetei_Mayek },
  { 678, PT_SC, ucp_Mende_Kikakui },
  { 692, PT_SC, ucp_Meroitic_Cursive },
  { 709, PT_SC, ucp_Meroitic_Hieroglyphs },
  { 730, PT_SC, ucp_Miao },
  { 735, PT_PC, ucp_Mn },
  { 738, PT_SC, ucp_Modi },
  { 743, PT_SC, ucp_Mongolian },
  { 753, PT_SC, ucp_Mro },
  { 757, PT_SC, ucp_Myanmar },
  { 765, PT_GC, ucp_N },
  { 767, PT_SC, ucp_Nabataean },
  { 777, PT_PC, ucp_Nd },
  { 780, PT_SC, ucp_New_Tai_Lue },
  { 792, PT_SC, ucp_Nko },
  { 796, PT_PC, ucp_Nl },
  { 799, PT_PC, ucp_No },
  { 802, PT_SC, ucp_Ogham },
  { 808, PT_SC, ucp_Ol_Chiki },
  { 817, PT_SC, ucp_Old_Italic },
  { 828, PT_SC, ucp_Old_North_Arabian },
  { 846, PT_SC, ucp_Old_Permic },
  { 857, PT_SC, ucp_Old_Persian },
  { 869, PT_SC, ucp_Old_South_Arabian },
  { 887, PT_SC, ucp_Old_Turkic },
  { 898, PT_SC, ucp_Oriya },
  { 904, PT_SC, ucp_Osmanya },
  { 912, PT_GC, ucp_P },
  { 914, PT_SC, ucp_Pahawh_Hmong },
  { 927, PT_SC, ucp_Palmyrene },
  { 937, PT_SC, ucp_Pau_Cin_Hau },
  { 949, PT_PC, ucp_Pc },
  { 952, PT_PC, ucp_Pd },
  { 955, PT_PC, ucp_Pe },
  { 958, PT_PC, ucp_Pf },
  { 961, PT_SC, ucp_Phags_Pa },
  { 970, PT_SC, ucp_Phoenician },
  { 981, PT_PC, ucp_Pi },
  { 984, PT_PC, ucp_Po },
  { 987, PT_PC, ucp_Ps },
  { 990, PT_SC, ucp_Psalter_Pahlavi },
  { 1006, PT_SC, ucp_Rejang },
  { 1013, PT_SC, ucp_Runic },
  { 1019, PT_GC, ucp_S },
  { 1021, PT_SC, ucp_Samaritan },
  { 1031, PT_SC, ucp_Saurashtra },
  { 1042, PT_PC, ucp_Sc },
  { 1045, PT_SC, ucp_Sharada },
  { 1053, PT_SC, ucp_Shavian },
  { 1061, PT_SC, ucp_Siddham },
  { 1069, PT_SC, ucp_Sinhala },
  { 1077, PT_PC, ucp_Sk },
  { 1080, PT_PC, ucp_Sm },
  { 1083, PT_PC, ucp_So },
  { 1086, PT_SC, ucp_Sora_Sompeng },
  { 1099, PT_SC, ucp_Sundanese },
  { 1109, PT_SC, ucp_Syloti_Nagri },
  { 1122, PT_SC, ucp_Syriac },
  { 1129, PT_SC, ucp_Tagalog },
  { 1137, PT_SC, ucp_Tagbanwa },
  { 1146, PT_SC, ucp_Tai_Le },
  { 1153, PT_SC, ucp_Tai_Tham },
  { 1162, PT_SC, ucp_Tai_Viet },
  { 1171, PT_SC, ucp_Takri },
  { 1177, PT_SC, ucp_Tamil },
  { 1183, PT_SC, ucp_Telugu },
  { 1190, PT_SC, ucp_Thaana },
  { 1197, PT_SC, ucp_Thai },
  { 1202, PT_SC, ucp_Tibetan },
  { 1210, PT_SC, ucp_Tifinagh },
  { 1219, PT_SC, ucp_Tirhuta },
  { 1227, PT_SC, ucp_Ugaritic },
  { 1236, PT_SC, ucp_Vai },
  { 1240, PT_SC, ucp_Warang_Citi },
  { 1252, PT_ALNUM, 0 },
  { 1256, PT_PXSPACE, 0 },
  { 1260, PT_SPACE, 0 },
  { 1264, PT_UCNC, 0 },
  { 1268, PT_WORD, 0 },
  { 1272, PT_SC, ucp_Yi },
  { 1275, PT_GC, ucp_Z },
  { 1277, PT_PC, ucp_Zl },
  { 1280, PT_PC, ucp_Zp },
  { 1283, PT_PC, ucp_Zs }
};

const int PRIV(utt_size) = sizeof(PRIV(utt)) / sizeof(ucp_type_table);

#endif /* SUPPORT_UTF */

/* End of pcre_tables.c */
