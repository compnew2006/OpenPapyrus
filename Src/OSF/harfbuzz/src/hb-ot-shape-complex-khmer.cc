/*
 * Copyright © 2011,2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */
#include "harfbuzz-internal.h"
#pragma hdrstop

#ifndef HB_NO_OT_SHAPE

#include "hb-ot-shape-complex-khmer.hh"
#include "hb-ot-layout.hh"
/*
 * Khmer shaper.
 */

static const hb_ot_map_feature_t
    khmer_features[] =
{
	/*
	 * Basic features.
	 * These features are applied in order, one at a time, after reordering.
	 */
	{HB_TAG('p', 'r', 'e', 'f'), F_MANUAL_JOINERS},
	{HB_TAG('b', 'l', 'w', 'f'), F_MANUAL_JOINERS},
	{HB_TAG('a', 'b', 'v', 'f'), F_MANUAL_JOINERS},
	{HB_TAG('p', 's', 't', 'f'), F_MANUAL_JOINERS},
	{HB_TAG('c', 'f', 'a', 'r'), F_MANUAL_JOINERS},
	/*
	 * Other features.
	 * These features are applied all at once after clearing syllables.
	 */
	{HB_TAG('p', 'r', 'e', 's'), F_GLOBAL_MANUAL_JOINERS},
	{HB_TAG('a', 'b', 'v', 's'), F_GLOBAL_MANUAL_JOINERS},
	{HB_TAG('b', 'l', 'w', 's'), F_GLOBAL_MANUAL_JOINERS},
	{HB_TAG('p', 's', 't', 's'), F_GLOBAL_MANUAL_JOINERS},
};

/*
 * Must be in the same order as the khmer_features array.
 */
enum {
	KHMER_PREF,
	KHMER_BLWF,
	KHMER_ABVF,
	KHMER_PSTF,
	KHMER_CFAR,

	_KHMER_PRES,
	_KHMER_ABVS,
	_KHMER_BLWS,
	_KHMER_PSTS,

	KHMER_NUM_FEATURES,
	KHMER_BASIC_FEATURES = _KHMER_PRES, /* Don't forget to update this! */
};

static void setup_syllables_khmer(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);
static void reorder_khmer(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);

static void collect_features_khmer(hb_ot_shape_planner_t * plan)
{
	hb_ot_map_builder_t * map = &plan->map;

	/* Do this before any lookups have been applied. */
	map->add_gsub_pause(setup_syllables_khmer);
	map->add_gsub_pause(reorder_khmer);

	/* Testing suggests that Uniscribe does NOT pause between basic
	 * features.  Test with KhmerUI.ttf and the following three
	 * sequences:
	 *
	 *   U+1789,U+17BC
	 *   U+1789,U+17D2,U+1789
	 *   U+1789,U+17D2,U+1789,U+17BC
	 *
	 * https://github.com/harfbuzz/harfbuzz/issues/974
	 */
	map->enable_feature(HB_TAG('l', 'o', 'c', 'l'));
	map->enable_feature(HB_TAG('c', 'c', 'm', 'p'));

	uint i = 0;
	for(; i < KHMER_BASIC_FEATURES; i++)
		map->add_feature(khmer_features[i]);

	map->add_gsub_pause(_hb_clear_syllables);

	for(; i < KHMER_NUM_FEATURES; i++)
		map->add_feature(khmer_features[i]);
}

static void override_features_khmer(hb_ot_shape_planner_t * plan)
{
	hb_ot_map_builder_t * map = &plan->map;

	/* Khmer spec has 'clig' as part of required shaping features:
	 * "Apply feature 'clig' to form ligatures that are desired for
	 * typographical correctness.", hence in overrides... */
	map->enable_feature(HB_TAG('c', 'l', 'i', 'g'));

	/* Uniscribe does not apply 'kern' in Khmer. */
	if(hb_options().uniscribe_bug_compatible) {
		map->disable_feature(HB_TAG('k', 'e', 'r', 'n'));
	}

	map->disable_feature(HB_TAG('l', 'i', 'g', 'a'));
}

struct khmer_shape_plan_t {
	bool get_virama_glyph(hb_font_t * font, hb_codepoint_t * pglyph) const
	{
		hb_codepoint_t glyph = virama_glyph;
		if(UNLIKELY(virama_glyph == (hb_codepoint_t)-1)) {
			if(!font->get_nominal_glyph(0x17D2u, &glyph))
				glyph = 0;
			/* Technically speaking, the spec says we should apply 'locl' to virama too.
			 * Maybe one day... */

			/* Our get_nominal_glyph() function needs a font, so we can't get the virama glyph
			 * during shape planning...  Instead, overwrite it here.  It's safe.  Don't worry! */
			virama_glyph = glyph;
		}

		* pglyph = glyph;
		return glyph != 0;
	}

	mutable hb_codepoint_t virama_glyph;

	hb_mask_t mask_array[KHMER_NUM_FEATURES];
};

static void * data_create_khmer(const hb_ot_shape_plan_t * plan)
{
	khmer_shape_plan_t * khmer_plan = (khmer_shape_plan_t*)SAlloc::C(1, sizeof(khmer_shape_plan_t));
	if(UNLIKELY(!khmer_plan))
		return nullptr;

	khmer_plan->virama_glyph = (hb_codepoint_t)-1;

	for(uint i = 0; i < ARRAY_LENGTH(khmer_plan->mask_array); i++)
		khmer_plan->mask_array[i] = (khmer_features[i].flags & F_GLOBAL) ?
		    0 : plan->map.get_1_mask(khmer_features[i].tag);

	return khmer_plan;
}

static void data_destroy_khmer(void * data)
{
	SAlloc::F(data);
}

enum khmer_syllable_type_t {
	khmer_consonant_syllable,
	khmer_broken_cluster,
	khmer_non_khmer_cluster,
};

#include "hb-ot-shape-complex-khmer-machine.hh"

static void setup_masks_khmer(const hb_ot_shape_plan_t * plan CXX_UNUSED_PARAM,
    hb_buffer_t * buffer,
    hb_font_t * font CXX_UNUSED_PARAM)
{
	HB_BUFFER_ALLOCATE_VAR(buffer, khmer_category);

	/* We cannot setup masks here.  We save information about characters
	 * and setup masks later on in a pause-callback. */

	uint count = buffer->len;
	hb_glyph_info_t * info = buffer->info;
	for(uint i = 0; i < count; i++)
		set_khmer_properties(info[i]);
}

static void setup_syllables_khmer(const hb_ot_shape_plan_t * plan CXX_UNUSED_PARAM,
    hb_font_t * font CXX_UNUSED_PARAM,
    hb_buffer_t * buffer)
{
	find_syllables_khmer(buffer);
	foreach_syllable(buffer, start, end)
	buffer->unsafe_to_break(start, end);
}

/* Rules from:
 * https://docs.microsoft.com/en-us/typography/script-development/devanagari */

static void reorder_consonant_syllable(const hb_ot_shape_plan_t * plan,
    hb_face_t * face CXX_UNUSED_PARAM,
    hb_buffer_t * buffer,
    uint start, uint end)
{
	const khmer_shape_plan_t * khmer_plan = (const khmer_shape_plan_t*)plan->data;
	hb_glyph_info_t * info = buffer->info;

	/* Setup masks. */
	{
		/* Post-base */
		hb_mask_t mask = khmer_plan->mask_array[KHMER_BLWF] |
		    khmer_plan->mask_array[KHMER_ABVF] |
		    khmer_plan->mask_array[KHMER_PSTF];
		for(uint i = start + 1; i < end; i++)
			info[i].mask  |= mask;
	}

	uint num_coengs = 0;
	for(uint i = start + 1; i < end; i++) {
		/* """
		 * When a COENG + (Cons | IndV) combination are found (and subscript count
		 * is less than two) the character combination is handled according to the
		 * subscript type of the character following the COENG.
		 *
		 * ...
		 *
		 * Subscript Type 2 - The COENG + RO characters are reordered to immediately
		 * before the base glyph. Then the COENG + RO characters are assigned to have
		 * the 'pref' OpenType feature applied to them.
		 * """
		 */
		if(info[i].khmer_category() == OT_Coeng && num_coengs <= 2 && i + 1 < end) {
			num_coengs++;

			if(info[i+1].khmer_category() == OT_Ra) {
				for(uint j = 0; j < 2; j++)
					info[i + j].mask |= khmer_plan->mask_array[KHMER_PREF];

				/* Move the Coeng,Ro sequence to the start. */
				buffer->merge_clusters(start, i + 2);
				hb_glyph_info_t t0 = info[i];
				hb_glyph_info_t t1 = info[i+1];
				memmove(&info[start + 2], &info[start], (i - start) * sizeof(info[0]));
				info[start] = t0;
				info[start + 1] = t1;

				/* Mark the subsequent stuff with 'cfar'.  Used in Khmer.
				 * Read the feature spec.
				 * This allows distinguishing the following cases with MS Khmer fonts:
				 * U+1784,U+17D2,U+179A,U+17D2,U+1782
				 * U+1784,U+17D2,U+1782,U+17D2,U+179A
				 */
				if(khmer_plan->mask_array[KHMER_CFAR])
					for(uint j = i + 2; j < end; j++)
						info[j].mask |= khmer_plan->mask_array[KHMER_CFAR];

				num_coengs = 2; /* Done. */
			}
		}

		/* Reorder left matra piece. */
		else if(info[i].khmer_category() == OT_VPre) {
			/* Move to the start. */
			buffer->merge_clusters(start, i + 1);
			hb_glyph_info_t t = info[i];
			memmove(&info[start + 1], &info[start], (i - start) * sizeof(info[0]));
			info[start] = t;
		}
	}
}

static void reorder_syllable_khmer(const hb_ot_shape_plan_t * plan,
    hb_face_t * face,
    hb_buffer_t * buffer,
    uint start, uint end)
{
	khmer_syllable_type_t syllable_type = (khmer_syllable_type_t)(buffer->info[start].syllable() & 0x0F);
	switch(syllable_type)
	{
		case khmer_broken_cluster: /* We already inserted dotted-circles, so just call the consonant_syllable.
		                              */
		case khmer_consonant_syllable:
		    reorder_consonant_syllable(plan, face, buffer, start, end);
		    break;

		case khmer_non_khmer_cluster:
		    break;
	}
}

static inline void insert_dotted_circles_khmer(const hb_ot_shape_plan_t * plan CXX_UNUSED_PARAM,
    hb_font_t * font,
    hb_buffer_t * buffer)
{
	if(UNLIKELY(buffer->flags & HB_BUFFER_FLAG_DO_NOT_INSERT_DOTTED_CIRCLE))
		return;

	/* Note: This loop is extra overhead, but should not be measurable.
	 * TODO Use a buffer scratch flag to remove the loop. */
	bool has_broken_syllables = false;
	uint count = buffer->len;
	hb_glyph_info_t * info = buffer->info;
	for(uint i = 0; i < count; i++)
		if((info[i].syllable() & 0x0F) == khmer_broken_cluster) {
			has_broken_syllables = true;
			break;
		}
	if(LIKELY(!has_broken_syllables))
		return;

	hb_codepoint_t dottedcircle_glyph;
	if(!font->get_nominal_glyph(0x25CCu, &dottedcircle_glyph))
		return;

	hb_glyph_info_t dottedcircle = {0};
	dottedcircle.codepoint = 0x25CCu;
	set_khmer_properties(dottedcircle);
	dottedcircle.codepoint = dottedcircle_glyph;

	buffer->clear_output();

	buffer->idx = 0;
	uint last_syllable = 0;
	while(buffer->idx < buffer->len && buffer->successful) {
		uint syllable = buffer->cur().syllable();
		khmer_syllable_type_t syllable_type = (khmer_syllable_type_t)(syllable & 0x0F);
		if(UNLIKELY(last_syllable != syllable && syllable_type == khmer_broken_cluster)) {
			last_syllable = syllable;

			hb_glyph_info_t ginfo = dottedcircle;
			ginfo.cluster = buffer->cur().cluster;
			ginfo.mask = buffer->cur().mask;
			ginfo.syllable() = buffer->cur().syllable();

			/* Insert dottedcircle after possible Repha. */
			while(buffer->idx < buffer->len && buffer->successful &&
			    last_syllable == buffer->cur().syllable() &&
			    buffer->cur().khmer_category() == OT_Repha)
				buffer->next_glyph();

			buffer->output_info(ginfo);
		}
		else
			buffer->next_glyph();
	}
	buffer->swap_buffers();
}

static void reorder_khmer(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer)
{
	insert_dotted_circles_khmer(plan, font, buffer);

	foreach_syllable(buffer, start, end)
	reorder_syllable_khmer(plan, font->face, buffer, start, end);

	HB_BUFFER_DEALLOCATE_VAR(buffer, khmer_category);
}

static bool decompose_khmer(const hb_ot_shape_normalize_context_t * c,
    hb_codepoint_t ab,
    hb_codepoint_t * a,
    hb_codepoint_t * b)
{
	switch(ab)
	{
		/*
		 * Decompose split matras that don't have Unicode decompositions.
		 */

		/* Khmer */
		case 0x17BEu: *a = 0x17C1u; *b = 0x17BEu; return true;
		case 0x17BFu: *a = 0x17C1u; *b = 0x17BFu; return true;
		case 0x17C0u: *a = 0x17C1u; *b = 0x17C0u; return true;
		case 0x17C4u: *a = 0x17C1u; *b = 0x17C4u; return true;
		case 0x17C5u: *a = 0x17C1u; *b = 0x17C5u; return true;
	}

	return (bool)c->unicode->decompose(ab, a, b);
}

static bool compose_khmer(const hb_ot_shape_normalize_context_t * c,
    hb_codepoint_t a,
    hb_codepoint_t b,
    hb_codepoint_t * ab)
{
	/* Avoid recomposing split matras. */
	if(HB_UNICODE_GENERAL_CATEGORY_IS_MARK(c->unicode->general_category(a)))
		return false;

	return (bool)c->unicode->compose(a, b, ab);
}

const hb_ot_complex_shaper_t _hb_ot_complex_shaper_khmer =
{
	collect_features_khmer,
	override_features_khmer,
	data_create_khmer,
	data_destroy_khmer,
	nullptr, /* preprocess_text */
	nullptr, /* postprocess_glyphs */
	HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_DIACRITICS_NO_SHORT_CIRCUIT,
	decompose_khmer,
	compose_khmer,
	setup_masks_khmer,
	HB_TAG_NONE, /* gpos_tag */
	nullptr, /* reorder_marks */
	HB_OT_SHAPE_ZERO_WIDTH_MARKS_NONE,
	false, /* fallback_position */
};

#endif
