/* Extended regular expression matching and search library.
   Copyright (C) 2002-2011 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Isamu Hasegawa <isamu@yamato.ibm.com>.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA. */

#ifndef _REGEX_INTERNAL_H
#define _REGEX_INTERNAL_H 1

#if defined _LIBC
	#include <bits/libc-lock.h>
#else
	#define __libc_lock_init(NAME) do { } while(0)
	#define __libc_lock_lock(NAME) do { } while(0)
	#define __libc_lock_unlock(NAME) do { } while(0)
#endif
/* In case that the system doesn't have isblank().  */
#if !defined _LIBC && !(defined isblank || (HAVE_ISBLANK && HAVE_DECL_ISBLANK))
	#define isblank(ch) ((ch) == ' ' || (ch) == '\t')
#endif
#ifdef _LIBC
	#ifndef _RE_DEFINE_LOCALE_FUNCTIONS
		#define _RE_DEFINE_LOCALE_FUNCTIONS 1
		#include <locale/localeinfo.h>
		#include <locale/elem-hash.h>
		#include <locale/coll-lookup.h>
	#endif
#endif
/* This is for other GNU distributions with internationalized messages.  */
#if (HAVE_LIBINTL_H && ENABLE_NLS) || defined _LIBC
	#include <libintl.h>
	#ifdef _LIBC
		#undef gettext
		#define gettext(msgid) INTUSE(__dcgettext) (_libc_intl_domainname, msgid, LC_MESSAGES)
	#endif
#else
	//#define gettext(msgid) (msgid)
#endif
#ifndef gettext_noop
	#define gettext_noop(String) String // This define is so xgettext can find the internationalizable strings. 
#endif
/* For loser systems without the definition.  */
#ifndef SIZE_MAX
	#define SIZE_MAX ((size_t)-1)
#endif
#if (defined MB_CUR_MAX && HAVE_WCTYPE_H && HAVE_ISWCTYPE && HAVE_WCSCOLL) || _LIBC
	#define RE_ENABLE_I18N
#endif
#if __GNUC__ >= 3
	#define BE(expr, val) __builtin_expect(expr, val)
#else
	#define BE(expr, val) (expr)
	#ifdef _LIBC
		#define inline
	#endif
#endif
#include "regex.h"

#define ASCII_CHARS 0x80 /* Number of ASCII characters.  */
#define SBC_MAX (UCHAR_MAX + 1) /* Number of single byte characters.  */
#define COLL_ELEM_LEN_MAX 8
/* The character which represents newline.  */
#define NEWLINE_CHAR '\n'
#define WIDE_NEWLINE_CHAR L'\n'
/* Rename to standard API for using out of glibc.  */
#ifndef _LIBC
	#define __wctype wctype
	#define __iswctype iswctype
	#define __btowc btowc
	#define __wcrtomb wcrtomb
	#define __mbrtowc mbrtowc
	#define __regfree regfree
	#define attribute_hidden
#endif /* not _LIBC */
#if __GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1)
	#define __attribute(arg) __attribute__ (arg)
#else
	#define __attribute(arg)
#endif
typedef __re_idx_t Idx;

#define REG_MISSING ((Idx) -1) /* Special return value for failure to match.  */
#define REG_ERROR ((Idx) -2) /* Special return value for internal error.  */
/* Test whether N is a valid index, and is not one of the above.  */
#ifdef _REGEX_LARGE_OFFSETS 
	#define REG_VALID_INDEX(n) ((Idx)(n) < REG_ERROR) 
#else
	#define REG_VALID_INDEX(n) (0 <= (n))
#endif
/* Test whether N is a valid nonzero index.  */
#ifdef _REGEX_LARGE_OFFSETS
	#define REG_VALID_NONZERO_INDEX(n) ((Idx)((n) - 1) < (Idx)(REG_ERROR - 1))
#else
	#define REG_VALID_NONZERO_INDEX(n) (0 < (n))
#endif
typedef __re_size_t re_hashval_t; /* A hash value, suitable for computing hash tables.  */

/* An integer used to represent a set of bits.  It must be unsigned,
   and must be at least as wide as uint.  */
typedef ulong bitset_word_t;
#define BITSET_WORD_MAX ULONG_MAX /* All bits set in a bitset_word_t.  */
/* Number of bits in a bitset_word_t.  For portability to hosts with
   padding bits, do not use '(sizeof(bitset_word_t) * CHAR_BIT)';
   instead, deduce it directly from BITSET_WORD_MAX.  Avoid
   greater-than-32-bit integers and unconditional shifts by more than
   31 bits, as they're not portable.  */
#if BITSET_WORD_MAX == 0xffffffffUL
	#define BITSET_WORD_BITS 32
#elif BITSET_WORD_MAX >> 31 >> 4 == 1
	#define BITSET_WORD_BITS 36
#elif BITSET_WORD_MAX >> 31 >> 16 == 1
	#define BITSET_WORD_BITS 48
#elif BITSET_WORD_MAX >> 31 >> 28 == 1
	#define BITSET_WORD_BITS 60
#elif BITSET_WORD_MAX >> 31 >> 31 >> 1 == 1
	#define BITSET_WORD_BITS 64
#elif BITSET_WORD_MAX >> 31 >> 31 >> 9 == 1
	#define BITSET_WORD_BITS 72
#elif BITSET_WORD_MAX >> 31 >> 31 >> 31 >> 31 >> 3 == 1
	#define BITSET_WORD_BITS 128
#elif BITSET_WORD_MAX >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 7 == 1
	#define BITSET_WORD_BITS 256
#elif BITSET_WORD_MAX >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 31 >> 7 > 1
	#define BITSET_WORD_BITS 257 /* any value > SBC_MAX will do here */
	#if BITSET_WORD_BITS <= SBC_MAX
		#error "Invalid SBC_MAX"
	#endif
#else
	#error "Add case for new bitset_word_t size"
#endif

// Number of bitset_word_t values in a bitset_t.
#define BITSET_WORDS ((SBC_MAX + BITSET_WORD_BITS - 1) / BITSET_WORD_BITS)

typedef bitset_word_t bitset_t[BITSET_WORDS];
typedef bitset_word_t * re_bitset_ptr_t;
typedef const bitset_word_t * re_const_bitset_ptr_t;

#define PREV_WORD_CONSTRAINT 0x0001
#define PREV_NOTWORD_CONSTRAINT 0x0002
#define NEXT_WORD_CONSTRAINT 0x0004
#define NEXT_NOTWORD_CONSTRAINT 0x0008
#define PREV_NEWLINE_CONSTRAINT 0x0010
#define NEXT_NEWLINE_CONSTRAINT 0x0020
#define PREV_BEGBUF_CONSTRAINT 0x0040
#define NEXT_ENDBUF_CONSTRAINT 0x0080
#define WORD_DELIM_CONSTRAINT 0x0100
#define NOT_WORD_DELIM_CONSTRAINT 0x0200

typedef enum {
	INSIDE_WORD = PREV_WORD_CONSTRAINT | NEXT_WORD_CONSTRAINT,
	WORD_FIRST = PREV_NOTWORD_CONSTRAINT | NEXT_WORD_CONSTRAINT,
	WORD_LAST = PREV_WORD_CONSTRAINT | NEXT_NOTWORD_CONSTRAINT,
	INSIDE_NOTWORD = PREV_NOTWORD_CONSTRAINT | NEXT_NOTWORD_CONSTRAINT,
	LINE_FIRST = PREV_NEWLINE_CONSTRAINT,
	LINE_LAST = NEXT_NEWLINE_CONSTRAINT,
	BUF_FIRST = PREV_BEGBUF_CONSTRAINT,
	BUF_LAST = NEXT_ENDBUF_CONSTRAINT,
	WORD_DELIM = WORD_DELIM_CONSTRAINT,
	NOT_WORD_DELIM = NOT_WORD_DELIM_CONSTRAINT
} re_context_type;

typedef struct {
	Idx alloc;
	Idx nelem;
	Idx * elems;
} re_node_set;

typedef enum {
	NON_TYPE = 0,
	/* Node type, These are used by token, node, tree.  */
	CHARACTER = 1,
	END_OF_RE = 2,
	SIMPLE_BRACKET = 3,
	OP_BACK_REF = 4,
	OP_PERIOD = 5,
#ifdef RE_ENABLE_I18N
	COMPLEX_BRACKET = 6,
	OP_UTF8_PERIOD = 7,
#endif /* RE_ENABLE_I18N */

	/* We define EPSILON_BIT as a macro so that OP_OPEN_SUBEXP is used
	   when the debugger shows values of this enum type.  */
#define EPSILON_BIT 8
	OP_OPEN_SUBEXP = EPSILON_BIT | 0,
	OP_CLOSE_SUBEXP = EPSILON_BIT | 1,
	OP_ALT = EPSILON_BIT | 2,
	OP_DUP_ASTERISK = EPSILON_BIT | 3,
	ANCHOR = EPSILON_BIT | 4,

	/* Tree type, these are used only by tree. */
	CONCAT = 16,
	SUBEXP = 17,

	/* Token type, these are used only by token.  */
	OP_DUP_PLUS = 18,
	OP_DUP_QUESTION,
	OP_OPEN_BRACKET,
	OP_CLOSE_BRACKET,
	OP_CHARSET_RANGE,
	OP_OPEN_DUP_NUM,
	OP_CLOSE_DUP_NUM,
	OP_NON_MATCH_LIST,
	OP_OPEN_COLL_ELEM,
	OP_CLOSE_COLL_ELEM,
	OP_OPEN_EQUIV_CLASS,
	OP_CLOSE_EQUIV_CLASS,
	OP_OPEN_CHAR_CLASS,
	OP_CLOSE_CHAR_CLASS,
	OP_WORD,
	OP_NOTWORD,
	OP_SPACE,
	OP_NOTSPACE,
	BACK_SLASH
} re_token_type_t;

#ifdef RE_ENABLE_I18N
typedef struct {
	/* Multibyte characters.  */
	wchar_t * mbchars;
	/* Collating symbols.  */
# ifdef _LIBC
	int32_t * coll_syms;
#endif
	/* Equivalence classes. */
# ifdef _LIBC
	int32_t * equiv_classes;
#endif
	/* Range expressions. */
# ifdef _LIBC
	uint32_t * range_starts;
	uint32_t * range_ends;
#else /* not _LIBC */
	wchar_t * range_starts;
	wchar_t * range_ends;
#endif /* not _LIBC */
	wctype_t * char_classes; /* Character classes. */
	uint non_match : 1; /* If this character set is the non-matching list.  */
	Idx nmbchars; /* # of multibyte characters.  */
	Idx ncoll_syms; /* # of collating symbols.  */
	Idx nequiv_classes; /* # of equivalence classes. */
	Idx nranges; /* # of range expressions. */
	Idx nchar_classes; /* # of character classes. */
} re_charset_t;
#endif /* RE_ENABLE_I18N */

typedef struct {
	union {
		uchar c; /* for CHARACTER */
		re_bitset_ptr_t sbcset; /* for SIMPLE_BRACKET */
#ifdef RE_ENABLE_I18N
		re_charset_t * mbcset; /* for COMPLEX_BRACKET */
#endif /* RE_ENABLE_I18N */
		Idx idx;        /* for BACK_REF */
		re_context_type ctx_type; /* for ANCHOR */
	} opr;

#if __GNUC__ >= 2 && !__STRICT_ANSI__
	re_token_type_t type : 8;
#else
	re_token_type_t type;
#endif
	uint constraint : 10; /* context constraint */
	uint duplicated : 1;
	uint opt_subexp : 1;
#ifdef RE_ENABLE_I18N
	uint accept_mb : 1;
	/* These 2 bits can be moved into the union if needed (e.g. if running out
	   of bits; move opr.c to opr.c.c and move the flags to opr.c.flags).  */
	uint mb_partial : 1;
#endif
	uint word_char : 1;
} re_token_t;

#define IS_EPSILON_NODE(type) ((type) & EPSILON_BIT)

struct re_string_t {
	/* Indicate the raw buffer which is the original string passed as an
	   argument of regexec(), re_search(), etc..  */
	const uchar * raw_mbs;
	/* Store the multibyte string.  In case of "case insensitive mode" like
	   REG_ICASE, upper cases of the string are stored, otherwise MBS points
	   the same address that RAW_MBS points.  */
	uchar * mbs;
#ifdef RE_ENABLE_I18N
	/* Store the wide character string which is corresponding to MBS.  */
	wint_t * wcs;
	Idx * offsets;
	mbstate_t cur_state;
#endif
	Idx raw_mbs_idx; /* Index in RAW_MBS.  Each character mbs[i] corresponds to raw_mbs[raw_mbs_idx + i].  */
	Idx valid_len; /* The length of the valid characters in the buffers.  */
	Idx valid_raw_len; /* The corresponding number of bytes in raw_mbs array.  */
	Idx bufs_len; /* The length of the buffers MBS and WCS.  */
	Idx cur_idx; /* The index in MBS, which is updated by re_string_fetch_byte.  */
	Idx raw_len; /* length of RAW_MBS array.  */
	Idx len; /* This is RAW_LEN - RAW_MBS_IDX + VALID_LEN - VALID_RAW_LEN.  */
	/* End of the buffer may be shorter than its length in the cases such
	   as re_match_2, re_search_2.  Then, we use STOP for end of the buffer
	   instead of LEN.  */
	Idx raw_stop;
	Idx stop; /* This is RAW_STOP - RAW_MBS_IDX adjusted through OFFSETS.  */
	/* The context of mbs[0].  We store the context independently, since
	   the context of mbs[0] may be different from raw_mbs[0], which is
	   the beginning of the input string.  */
	uint tip_context;
	/* The translation passed as a part of an argument of re_compile_pattern.  */
	RE_TRANSLATE_TYPE trans;
	/* Copy of re_dfa_t's word_char.  */
	re_const_bitset_ptr_t word_char;
	/* true if REG_ICASE.  */
	uchar icase;
	uchar is_utf8;
	uchar map_notascii;
	uchar mbs_allocated;
	uchar offsets_needed;
	uchar newline_anchor;
	uchar word_ops_used;
	int mb_cur_max;
};

struct re_dfa_t;
typedef struct re_string_t re_string_t;
typedef struct re_dfa_t re_dfa_t;

#ifndef _LIBC
	#if defined __i386__ && !defined __EMX__
		#define internal_function   __attribute((regparm(3), stdcall))
	#else
		#define internal_function
	#endif
#endif

static reg_errcode_t re_string_realloc_buffers(re_string_t * pstr, Idx new_buf_len) internal_function;
#ifdef RE_ENABLE_I18N
	static void build_wcs_buffer(re_string_t * pstr) internal_function;
	static reg_errcode_t build_wcs_upper_buffer(re_string_t * pstr) internal_function;
#endif /* RE_ENABLE_I18N */
static void build_upper_buffer(re_string_t * pstr) internal_function;
static void re_string_translate_buffer(re_string_t * pstr) internal_function;
static uint re_string_context_at(const re_string_t * input, Idx idx, int eflags) internal_function __attribute((pure));
#define re_string_peek_byte(pstr, offset) ((pstr)->mbs[(pstr)->cur_idx + offset])
#define re_string_fetch_byte(pstr) ((pstr)->mbs[(pstr)->cur_idx++])
#define re_string_first_byte(pstr, idx) ((idx) == (pstr)->valid_len || (pstr)->wcs[idx] != WEOF)
#define re_string_is_single_byte_char(pstr, idx) ((pstr)->wcs[idx] != WEOF && ((pstr)->valid_len == (idx) + 1 || (pstr)->wcs[(idx) + 1] != WEOF))
#define re_string_eoi(pstr) ((pstr)->stop <= (pstr)->cur_idx)
#define re_string_cur_idx(pstr) ((pstr)->cur_idx)
#define re_string_get_buffer(pstr) ((pstr)->mbs)
#define re_string_length(pstr) ((pstr)->len)
#define re_string_byte_at(pstr, idx) ((pstr)->mbs[idx])
#define re_string_skip_bytes(pstr, idx) ((pstr)->cur_idx += (idx))
#define re_string_set_index(pstr, idx) ((pstr)->cur_idx = (idx))

//#include <alloca.h>

#ifndef _LIBC
	#if HAVE_ALLOCA
	/* The OS usually guarantees only one guard page at the bottom of the stack,
	   and a page size can be as small as 4096 bytes.  So we cannot safely
	   allocate anything larger than 4096 bytes.  Also care for the possibility
	   of a few compiler-allocated temporary stack slots.  */
	#  define __libc_use_alloca(n) ((n) < 4032)
	#else
	/* alloca is implemented with malloc, so just use malloc.  */
		#define __libc_use_alloca(n) 0
		#undef alloca
		#define alloca(n) SAlloc::M(n)
	#endif
#endif
//#ifndef MAX
	//#define MAX(a, b) ((a) < (b) ? (b) : (a))
//#endif
#define re_malloc(t, n) ((t *)SAlloc::M((n) * sizeof(t)))
#define re_realloc(p, t, n) ((t *)SAlloc::R(p, (n) * sizeof(t)))
#define re_free(p) SAlloc::F(p)

struct bin_tree_t {
	struct bin_tree_t * parent;
	struct bin_tree_t * left;
	struct bin_tree_t * right;
	struct bin_tree_t * first;
	struct bin_tree_t * next;
	re_token_t token;
	/* `node_idx' is the index in dfa->nodes, if `type' == 0. Otherwise `type' indicate the type of this node.  */
	Idx node_idx;
};

typedef struct bin_tree_t bin_tree_t;

#define BIN_TREE_STORAGE_SIZE ((1024 - sizeof(void *)) / sizeof(bin_tree_t))

struct bin_tree_storage_t {
	struct bin_tree_storage_t * next;
	bin_tree_t data[BIN_TREE_STORAGE_SIZE];
};

typedef struct bin_tree_storage_t bin_tree_storage_t;

#define CONTEXT_WORD 1
#define CONTEXT_NEWLINE (CONTEXT_WORD << 1)
#define CONTEXT_BEGBUF (CONTEXT_NEWLINE << 1)
#define CONTEXT_ENDBUF (CONTEXT_BEGBUF << 1)

#define IS_WORD_CONTEXT(c) ((c) & CONTEXT_WORD)
#define IS_NEWLINE_CONTEXT(c) ((c) & CONTEXT_NEWLINE)
#define IS_BEGBUF_CONTEXT(c) ((c) & CONTEXT_BEGBUF)
#define IS_ENDBUF_CONTEXT(c) ((c) & CONTEXT_ENDBUF)
#define IS_ORDINARY_CONTEXT(c) ((c) == 0)

#define IS_WORD_CHAR(ch) (isalnum(ch) || (ch) == '_')
#define IS_NEWLINE(ch) ((ch) == NEWLINE_CHAR)
#define IS_WIDE_WORD_CHAR(ch) (iswalnum(ch) || (ch) == L'_')
#define IS_WIDE_NEWLINE(ch) ((ch) == WIDE_NEWLINE_CHAR)

#define NOT_SATISFY_PREV_CONSTRAINT(constraint, context) \
	((((constraint) & PREV_WORD_CONSTRAINT) && !IS_WORD_CONTEXT(context)) \
	|| ((constraint & PREV_NOTWORD_CONSTRAINT) && IS_WORD_CONTEXT(context)) \
	|| ((constraint & PREV_NEWLINE_CONSTRAINT) && !IS_NEWLINE_CONTEXT(context)) \
	|| ((constraint & PREV_BEGBUF_CONSTRAINT) && !IS_BEGBUF_CONTEXT(context)))

#define NOT_SATISFY_NEXT_CONSTRAINT(constraint, context) \
	((((constraint) & NEXT_WORD_CONSTRAINT) && !IS_WORD_CONTEXT(context)) \
	|| (((constraint) & NEXT_NOTWORD_CONSTRAINT) && IS_WORD_CONTEXT(context)) \
	|| (((constraint) & NEXT_NEWLINE_CONSTRAINT) && !IS_NEWLINE_CONTEXT(context)) \
	|| (((constraint) & NEXT_ENDBUF_CONSTRAINT) && !IS_ENDBUF_CONTEXT(context)))

struct re_dfastate_t {
	re_hashval_t hash;
	re_node_set nodes;
	re_node_set non_eps_nodes;
	re_node_set inveclosure;
	re_node_set * entrance_nodes;
	struct re_dfastate_t ** trtable, ** word_trtable;
	uint context : 4;
	uint halt : 1;
	/* If this state can accept `multi byte'.
	   Note that we refer to multibyte characters, and multi character
	   collating elements as `multi byte'.  */
	uint accept_mb : 1;
	/* If this state has backreference node(s).  */
	uint has_backref : 1;
	uint has_constraint : 1;
};

typedef struct re_dfastate_t re_dfastate_t;

struct re_state_table_entry {
	Idx num;
	Idx alloc;
	re_dfastate_t ** array;
};

/* Array type used in re_sub_match_last_t and re_sub_match_top_t.  */

typedef struct {
	Idx next_idx;
	Idx alloc;
	re_dfastate_t ** array;
} state_array_t;

/* Store information about the node NODE whose type is OP_CLOSE_SUBEXP.  */

typedef struct {
	Idx node;
	Idx str_idx; /* The position NODE match at.  */
	state_array_t path;
} re_sub_match_last_t;

/* Store information about the node NODE whose type is OP_OPEN_SUBEXP.
   And information about the node, whose type is OP_CLOSE_SUBEXP,
   corresponding to NODE is stored in LASTS.  */

typedef struct {
	Idx str_idx;
	Idx node;
	state_array_t * path;
	Idx alasts; /* Allocation size of LASTS.  */
	Idx nlasts; /* The number of LASTS.  */
	re_sub_match_last_t ** lasts;
} re_sub_match_top_t;

struct re_backref_cache_entry {
	Idx node;
	Idx str_idx;
	Idx subexp_from;
	Idx subexp_to;
	char more;
	char unused;
	ushort eps_reachable_subexps_map;
};

typedef struct {
	/* The string object corresponding to the input string.  */
	re_string_t input;
#if defined _LIBC || (defined __STDC_VERSION__ && __STDC_VERSION__ >= 199901L)
	const re_dfa_t * const dfa;
#else
	const re_dfa_t * dfa;
#endif
	/* EFLAGS of the argument of regexec.  */
	int eflags;
	/* Where the matching ends.  */
	Idx match_last;
	Idx last_node;
	/* The state log used by the matcher.  */
	re_dfastate_t ** state_log;
	Idx state_log_top;
	/* Back reference cache.  */
	Idx nbkref_ents;
	Idx abkref_ents;
	struct re_backref_cache_entry * bkref_ents;
	int max_mb_elem_len;
	Idx nsub_tops;
	Idx asub_tops;
	re_sub_match_top_t ** sub_tops;
} re_match_context_t;

typedef struct {
	re_dfastate_t ** sifted_states;
	re_dfastate_t ** limited_states;
	Idx last_node;
	Idx last_str_idx;
	re_node_set limits;
} re_sift_context_t;

struct re_fail_stack_ent_t {
	Idx idx;
	Idx node;
	regmatch_t * regs;
	re_node_set eps_via_nodes;
};

struct re_fail_stack_t {
	Idx num;
	Idx alloc;
	struct re_fail_stack_ent_t * stack;
};

struct re_dfa_t {
	re_token_t * nodes;
	size_t nodes_alloc;
	size_t nodes_len;
	Idx * nexts;
	Idx * org_indices;
	re_node_set * edests;
	re_node_set * eclosures;
	re_node_set * inveclosures;
	struct re_state_table_entry * state_table;
	re_dfastate_t * init_state;
	re_dfastate_t * init_state_word;
	re_dfastate_t * init_state_nl;
	re_dfastate_t * init_state_begbuf;
	bin_tree_t * str_tree;
	bin_tree_storage_t * str_tree_storage;
	re_bitset_ptr_t sb_char;
	int str_tree_storage_idx;

	/* number of subexpressions `re_nsub' is in regex_t.  */
	re_hashval_t state_hash_mask;
	Idx init_node;
	Idx nbackref; /* The number of backreference in this dfa.  */

	/* Bitmap expressing which backreference is used.  */
	bitset_word_t used_bkref_map;
	bitset_word_t completed_bkref_map;

	uint has_plural_match : 1;
	/* If this dfa has "multibyte node", which is a backreference or
	   a node which can accept multibyte character or multi character
	   collating element.  */
	uint has_mb_node : 1;
	uint is_utf8 : 1;
	uint map_notascii : 1;
	uint word_ops_used : 1;
	int mb_cur_max;
	bitset_t word_char;
	reg_syntax_t syntax;
	Idx * subexp_map;
#ifdef DEBUG
	char* re_str;
#endif
#ifdef _LIBC
	__libc_lock_define(, lock)
#endif
};

#define re_node_set_init_empty(set) memzero(set, sizeof(re_node_set))
#define re_node_set_remove(set, id) (re_node_set_remove_at(set, re_node_set_contains(set, id) - 1))
#define re_node_set_empty(p) ((p)->nelem = 0)
#define re_node_set_free(set) re_free((set)->elems)

typedef enum {
	SB_CHAR,
	MB_CHAR,
	EQUIV_CLASS,
	COLL_SYM,
	CHAR_CLASS
} bracket_elem_type;

typedef struct {
	bracket_elem_type type;
	union {
		uchar ch;
		uchar * name;
		wchar_t wch;
	} opr;
} bracket_elem_t;

/* Inline functions for bitset_t operation.  */

static void bitset_set(bitset_t set, Idx i)
{
	set[i / BITSET_WORD_BITS] |= (bitset_word_t)1 << i % BITSET_WORD_BITS;
}

static void bitset_clear(bitset_t set, Idx i)
{
	set[i / BITSET_WORD_BITS] &= ~((bitset_word_t)1 << i % BITSET_WORD_BITS);
}

static bool bitset_contain(const bitset_t set, Idx i)
{
	return (set[i / BITSET_WORD_BITS] >> i % BITSET_WORD_BITS) & 1;
}

static void bitset_empty(bitset_t set)
{
	memzero(set, sizeof(bitset_t));
}

static void bitset_set_all(bitset_t set)
{
	memset(set, -1, sizeof(bitset_word_t) * (SBC_MAX / BITSET_WORD_BITS));
	if(SBC_MAX % BITSET_WORD_BITS != 0)
		set[BITSET_WORDS - 1] = ((bitset_word_t)1 << SBC_MAX % BITSET_WORD_BITS) - 1;
}

static void bitset_copy(bitset_t dest, const bitset_t src)
{
	memcpy(dest, src, sizeof(bitset_t));
}

static void bitset_not(bitset_t set)
{
	int bitset_i;
	for(bitset_i = 0; bitset_i < SBC_MAX / BITSET_WORD_BITS; ++bitset_i)
		set[bitset_i] = ~set[bitset_i];
	if(SBC_MAX % BITSET_WORD_BITS != 0)
		set[BITSET_WORDS - 1] = ((((bitset_word_t)1 << SBC_MAX % BITSET_WORD_BITS) - 1) & ~set[BITSET_WORDS - 1]);
}

static void bitset_merge(bitset_t dest, const bitset_t src)
{
	int bitset_i;
	for(bitset_i = 0; bitset_i < BITSET_WORDS; ++bitset_i)
		dest[bitset_i] |= src[bitset_i];
}

static void bitset_mask(bitset_t dest, const bitset_t src)
{
	int bitset_i;
	for(bitset_i = 0; bitset_i < BITSET_WORDS; ++bitset_i)
		dest[bitset_i] &= src[bitset_i];
}

#ifdef RE_ENABLE_I18N
/* Inline functions for re_string.  */
static int internal_function __attribute((pure)) re_string_char_size_at(const re_string_t *pstr, Idx idx)
{
	int byte_idx;
	if(pstr->mb_cur_max == 1)
		return 1;
	for(byte_idx = 1; idx + byte_idx < pstr->valid_len; ++byte_idx)
		if(pstr->wcs[idx + byte_idx] != WEOF)
			break;
	return byte_idx;
}

static wint_t internal_function __attribute((pure)) re_string_wchar_at(const re_string_t *pstr, Idx idx)
{
	if(pstr->mb_cur_max == 1)
		return (wint_t)pstr->mbs[idx];
	return (wint_t)pstr->wcs[idx];
}

static int internal_function __attribute((pure)) re_string_elem_size_at(const re_string_t *pstr, Idx idx)
{
# ifdef _LIBC
	const uchar * p, * extra;
	const int32_t * table, * indirect;
	int32_t tmp;
#  include <locale/weight.h>
	uint_fast32_t nrules = _NL_CURRENT_WORD(LC_COLLATE, _NL_COLLATE_NRULES);
	if(nrules != 0) {
		table = (const int32_t*)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_TABLEMB);
		extra = (const uchar*)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_EXTRAMB);
		indirect = (const int32_t*)_NL_CURRENT(LC_COLLATE, _NL_COLLATE_INDIRECTMB);
		p = pstr->mbs + idx;
		tmp = findidx(&p);
		return p - pstr->mbs - idx;
	}
	else
#endif /* _LIBC */
	return 1;
}
#endif /* RE_ENABLE_I18N */

#ifndef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
#  define __GNUC_PREREQ(maj, min) \
	((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#  define __GNUC_PREREQ(maj, min) 0
#endif
#endif

#if __GNUC_PREREQ(3, 4)
	#undef __attribute_warn_unused_result__
	#define __attribute_warn_unused_result__ __attribute__ ((__warn_unused_result__))
#else
	#define __attribute_warn_unused_result__ /* empty */
#endif

#endif /*  _REGEX_INTERNAL_H */
