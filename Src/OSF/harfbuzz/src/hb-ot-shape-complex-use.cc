/*
 * Copyright © 2015  Mozilla Foundation.
 * Copyright © 2015  Google, Inc.
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
 * Mozilla Author(s): Jonathan Kew
 * Google Author(s): Behdad Esfahbod
 */
#include "harfbuzz-internal.h"
#pragma hdrstop

#ifndef HB_NO_OT_SHAPE

#include "hb-ot-shape-complex-use.hh"
#include "hb-ot-shape-complex-arabic.hh"
#include "hb-ot-shape-complex-arabic-joining-list.hh"
#include "hb-ot-shape-complex-vowel-constraints.hh"

/* buffer var allocations */
#define use_category() complex_var_u8_1()

/*
 * Universal Shaping Engine.
 * https://docs.microsoft.com/en-us/typography/script-development/use
 */

static const hb_tag_t
    use_basic_features[] =
{
	/*
	 * Basic features.
	 * These features are applied all at once, before reordering.
	 */
	HB_TAG('r', 'k', 'r', 'f'),
	HB_TAG('a', 'b', 'v', 'f'),
	HB_TAG('b', 'l', 'w', 'f'),
	HB_TAG('h', 'a', 'l', 'f'),
	HB_TAG('p', 's', 't', 'f'),
	HB_TAG('v', 'a', 't', 'u'),
	HB_TAG('c', 'j', 'c', 't'),
};
static const hb_tag_t
    use_topographical_features[] =
{
	HB_TAG('i', 's', 'o', 'l'),
	HB_TAG('i', 'n', 'i', 't'),
	HB_TAG('m', 'e', 'd', 'i'),
	HB_TAG('f', 'i', 'n', 'a'),
};
/* Same order as use_topographical_features. */
enum joining_form_t {
	USE_ISOL,
	USE_INIT,
	USE_MEDI,
	USE_FINA,
	_USE_NONE
};

static const hb_tag_t
    use_other_features[] =
{
	/*
	 * Other features.
	 * These features are applied all at once, after reordering and
	 * clearing syllables.
	 */
	HB_TAG('a', 'b', 'v', 's'),
	HB_TAG('b', 'l', 'w', 's'),
	HB_TAG('h', 'a', 'l', 'n'),
	HB_TAG('p', 'r', 'e', 's'),
	HB_TAG('p', 's', 't', 's'),
};

static void setup_syllables_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);
static void record_rphf_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);
static void record_pref_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);
static void reorder_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer);

static void collect_features_use(hb_ot_shape_planner_t * plan)
{
	hb_ot_map_builder_t * map = &plan->map;

	/* Do this before any lookups have been applied. */
	map->add_gsub_pause(setup_syllables_use);

	/* "Default glyph pre-processing group" */
	map->enable_feature(HB_TAG('l', 'o', 'c', 'l'));
	map->enable_feature(HB_TAG('c', 'c', 'm', 'p'));
	map->enable_feature(HB_TAG('n', 'u', 'k', 't'));
	map->enable_feature(HB_TAG('a', 'k', 'h', 'n'), F_MANUAL_ZWJ);

	/* "Reordering group" */
	map->add_gsub_pause(_hb_clear_substitution_flags);
	map->add_feature(HB_TAG('r', 'p', 'h', 'f'), F_MANUAL_ZWJ);
	map->add_gsub_pause(record_rphf_use);
	map->add_gsub_pause(_hb_clear_substitution_flags);
	map->enable_feature(HB_TAG('p', 'r', 'e', 'f'), F_MANUAL_ZWJ);
	map->add_gsub_pause(record_pref_use);

	/* "Orthographic unit shaping group" */
	for(uint i = 0; i < ARRAY_LENGTH(use_basic_features); i++)
		map->enable_feature(use_basic_features[i], F_MANUAL_ZWJ);

	map->add_gsub_pause(reorder_use);
	map->add_gsub_pause(_hb_clear_syllables);

	/* "Topographical features" */
	for(uint i = 0; i < ARRAY_LENGTH(use_topographical_features); i++)
		map->add_feature(use_topographical_features[i]);
	map->add_gsub_pause(nullptr);

	/* "Standard typographic presentation" */
	for(uint i = 0; i < ARRAY_LENGTH(use_other_features); i++)
		map->enable_feature(use_other_features[i], F_MANUAL_ZWJ);
}

struct use_shape_plan_t {
	hb_mask_t rphf_mask;

	arabic_shape_plan_t * arabic_plan;
};

static void * data_create_use(const hb_ot_shape_plan_t * plan)
{
	use_shape_plan_t * use_plan = (use_shape_plan_t*)SAlloc::C(1, sizeof(use_shape_plan_t));
	if(UNLIKELY(!use_plan))
		return nullptr;

	use_plan->rphf_mask = plan->map.get_1_mask(HB_TAG('r', 'p', 'h', 'f'));

	if(has_arabic_joining(plan->props.script)) {
		use_plan->arabic_plan = (arabic_shape_plan_t*)data_create_arabic(plan);
		if(UNLIKELY(!use_plan->arabic_plan)) {
			SAlloc::F(use_plan);
			return nullptr;
		}
	}

	return use_plan;
}

static void data_destroy_use(void * data)
{
	use_shape_plan_t * use_plan = (use_shape_plan_t*)data;

	if(use_plan->arabic_plan)
		data_destroy_arabic(use_plan->arabic_plan);

	SAlloc::F(data);
}

enum use_syllable_type_t {
	use_independent_cluster,
	use_virama_terminated_cluster,
	use_sakot_terminated_cluster,
	use_standard_cluster,
	use_number_joiner_terminated_cluster,
	use_numeral_cluster,
	use_symbol_cluster,
	use_broken_cluster,
	use_non_cluster,
};

#include "hb-ot-shape-complex-use-machine.hh"

static void setup_masks_use(const hb_ot_shape_plan_t * plan,
    hb_buffer_t * buffer,
    hb_font_t * font CXX_UNUSED_PARAM)
{
	const use_shape_plan_t * use_plan = (const use_shape_plan_t*)plan->data;

	/* Do this before allocating use_category(). */
	if(use_plan->arabic_plan) {
		setup_masks_arabic_plan(use_plan->arabic_plan, buffer, plan->props.script);
	}

	HB_BUFFER_ALLOCATE_VAR(buffer, use_category);

	/* We cannot setup masks here.  We save information about characters
	 * and setup masks later on in a pause-callback. */

	uint count = buffer->len;
	hb_glyph_info_t * info = buffer->info;
	for(uint i = 0; i < count; i++)
		info[i].use_category() = hb_use_get_category(info[i].codepoint);
}

static void setup_rphf_mask(const hb_ot_shape_plan_t * plan,
    hb_buffer_t * buffer)
{
	const use_shape_plan_t * use_plan = (const use_shape_plan_t*)plan->data;

	hb_mask_t mask = use_plan->rphf_mask;
	if(!mask) return;

	hb_glyph_info_t * info = buffer->info;

	foreach_syllable(buffer, start, end)
	{
		uint limit = info[start].use_category() == USE_R ? 1 : hb_min(3u, end - start);
		for(uint i = start; i < start + limit; i++)
			info[i].mask |= mask;
	}
}

static void setup_topographical_masks(const hb_ot_shape_plan_t * plan,
    hb_buffer_t * buffer)
{
	const use_shape_plan_t * use_plan = (const use_shape_plan_t*)plan->data;
	if(use_plan->arabic_plan)
		return;

	static_assert((USE_INIT < 4 && USE_ISOL < 4 && USE_MEDI < 4 && USE_FINA < 4), "");
	hb_mask_t masks[4], all_masks = 0;
	for(uint i = 0; i < 4; i++) {
		masks[i] = plan->map.get_1_mask(use_topographical_features[i]);
		if(masks[i] == plan->map.get_global_mask())
			masks[i] = 0;
		all_masks |= masks[i];
	}
	if(!all_masks)
		return;
	hb_mask_t other_masks = ~all_masks;

	uint last_start = 0;
	joining_form_t last_form = _USE_NONE;
	hb_glyph_info_t * info = buffer->info;
	foreach_syllable(buffer, start, end)
	{
		use_syllable_type_t syllable_type = (use_syllable_type_t)(info[start].syllable() & 0x0F);
		switch(syllable_type)
		{
			case use_independent_cluster:
			case use_symbol_cluster:
			case use_non_cluster:
			    /* These don't join.  Nothing to do. */
			    last_form = _USE_NONE;
			    break;

			case use_virama_terminated_cluster:
			case use_sakot_terminated_cluster:
			case use_standard_cluster:
			case use_number_joiner_terminated_cluster:
			case use_numeral_cluster:
			case use_broken_cluster:

			    bool join = last_form == USE_FINA || last_form == USE_ISOL;

			    if(join) {
				    /* Fixup previous syllable's form. */
				    last_form = last_form == USE_FINA ? USE_MEDI : USE_INIT;
				    for(uint i = last_start; i < start; i++)
					    info[i].mask = (info[i].mask & other_masks) | masks[last_form];
			    }

			    /* Form for this syllable. */
			    last_form = join ? USE_FINA : USE_ISOL;
			    for(uint i = start; i < end; i++)
				    info[i].mask = (info[i].mask & other_masks) | masks[last_form];

			    break;
		}

		last_start = start;
	}
}

static void setup_syllables_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font CXX_UNUSED_PARAM,
    hb_buffer_t * buffer)
{
	find_syllables_use(buffer);
	foreach_syllable(buffer, start, end)
	buffer->unsafe_to_break(start, end);
	setup_rphf_mask(plan, buffer);
	setup_topographical_masks(plan, buffer);
}

static void record_rphf_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font CXX_UNUSED_PARAM,
    hb_buffer_t * buffer)
{
	const use_shape_plan_t * use_plan = (const use_shape_plan_t*)plan->data;

	hb_mask_t mask = use_plan->rphf_mask;
	if(!mask) return;
	hb_glyph_info_t * info = buffer->info;

	foreach_syllable(buffer, start, end)
	{
		/* Mark a substituted repha as USE_R. */
		for(uint i = start; i < end && (info[i].mask & mask); i++)
			if(_hb_glyph_info_substituted(&info[i])) {
				info[i].use_category() = USE_R;
				break;
			}
	}
}

static void record_pref_use(const hb_ot_shape_plan_t * plan CXX_UNUSED_PARAM,
    hb_font_t * font CXX_UNUSED_PARAM,
    hb_buffer_t * buffer)
{
	hb_glyph_info_t * info = buffer->info;

	foreach_syllable(buffer, start, end)
	{
		/* Mark a substituted pref as VPre, as they behave the same way. */
		for(uint i = start; i < end; i++)
			if(_hb_glyph_info_substituted(&info[i])) {
				info[i].use_category() = USE_VPre;
				break;
			}
	}
}

static inline bool is_halant_use(const hb_glyph_info_t &info)
{
	return (info.use_category() == USE_H || info.use_category() == USE_HVM) &&
	       !_hb_glyph_info_ligated(&info);
}

static void reorder_syllable_use(hb_buffer_t * buffer, uint start, uint end)
{
	use_syllable_type_t syllable_type = (use_syllable_type_t)(buffer->info[start].syllable() & 0x0F);
	/* Only a few syllable types need reordering. */
	if(UNLIKELY(!(FLAG_UNSAFE(syllable_type) &
	    (FLAG(use_virama_terminated_cluster) |
	    FLAG(use_sakot_terminated_cluster) |
	    FLAG(use_standard_cluster) |
	    FLAG(use_broken_cluster) |
	    0))))
		return;

	hb_glyph_info_t * info = buffer->info;

#define POST_BASE_FLAGS64 (FLAG64(USE_FM) | \
	FLAG64(USE_FAbv) | \
	FLAG64(USE_FBlw) | \
	FLAG64(USE_FPst) | \
	FLAG64(USE_MAbv) | \
	FLAG64(USE_MBlw) | \
	FLAG64(USE_MPst) | \
	FLAG64(USE_MPre) | \
	FLAG64(USE_VAbv) | \
	FLAG64(USE_VBlw) | \
	FLAG64(USE_VPst) | \
	FLAG64(USE_VPre) | \
	FLAG64(USE_VMAbv) | \
	FLAG64(USE_VMBlw) | \
	FLAG64(USE_VMPst) | \
	FLAG64(USE_VMPre))

	/* Move things forward. */
	if(info[start].use_category() == USE_R && end - start > 1) {
		/* Got a repha.  Reorder it towards the end, but before the first post-base
		 * glyph. */
		for(uint i = start + 1; i < end; i++) {
			bool is_post_base_glyph = (FLAG64_UNSAFE(info[i].use_category()) & POST_BASE_FLAGS64) ||
			    is_halant_use(info[i]);
			if(is_post_base_glyph || i == end - 1) {
				/* If we hit a post-base glyph, move before it; otherwise move to the
				 * end. Shift things in between backward. */

				if(is_post_base_glyph)
					i--;

				buffer->merge_clusters(start, i + 1);
				hb_glyph_info_t t = info[start];
				memmove(&info[start], &info[start + 1], (i - start) * sizeof(info[0]));
				info[i] = t;

				break;
			}
		}
	}

	/* Move things back. */
	uint j = start;
	for(uint i = start; i < end; i++) {
		uint32_t flag = FLAG_UNSAFE(info[i].use_category());
		if(is_halant_use(info[i])) {
			/* If we hit a halant, move after it; otherwise move to the beginning, and
			 * shift things in between forward. */
			j = i + 1;
		}
		else if(((flag) & (FLAG(USE_VPre) | FLAG(USE_VMPre))) &&
		    /* Only move the first component of a MultipleSubst. */
		    0 == _hb_glyph_info_get_lig_comp(&info[i]) &&
		    j < i) {
			buffer->merge_clusters(j, i + 1);
			hb_glyph_info_t t = info[i];
			memmove(&info[j + 1], &info[j], (i - j) * sizeof(info[0]));
			info[j] = t;
		}
	}
}

static inline void insert_dotted_circles_use(const hb_ot_shape_plan_t * plan CXX_UNUSED_PARAM,
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
		if((info[i].syllable() & 0x0F) == use_broken_cluster) {
			has_broken_syllables = true;
			break;
		}
	if(LIKELY(!has_broken_syllables))
		return;

	hb_glyph_info_t dottedcircle = {0};
	if(!font->get_nominal_glyph(0x25CCu, &dottedcircle.codepoint))
		return;
	dottedcircle.use_category() = hb_use_get_category(0x25CC);

	buffer->clear_output();

	buffer->idx = 0;
	uint last_syllable = 0;
	while(buffer->idx < buffer->len && buffer->successful) {
		uint syllable = buffer->cur().syllable();
		use_syllable_type_t syllable_type = (use_syllable_type_t)(syllable & 0x0F);
		if(UNLIKELY(last_syllable != syllable && syllable_type == use_broken_cluster)) {
			last_syllable = syllable;

			hb_glyph_info_t ginfo = dottedcircle;
			ginfo.cluster = buffer->cur().cluster;
			ginfo.mask = buffer->cur().mask;
			ginfo.syllable() = buffer->cur().syllable();

			/* Insert dottedcircle after possible Repha. */
			while(buffer->idx < buffer->len && buffer->successful &&
			    last_syllable == buffer->cur().syllable() &&
			    buffer->cur().use_category() == USE_R)
				buffer->next_glyph();

			buffer->output_info(ginfo);
		}
		else
			buffer->next_glyph();
	}
	buffer->swap_buffers();
}

static void reorder_use(const hb_ot_shape_plan_t * plan,
    hb_font_t * font,
    hb_buffer_t * buffer)
{
	insert_dotted_circles_use(plan, font, buffer);

	foreach_syllable(buffer, start, end)
	reorder_syllable_use(buffer, start, end);

	HB_BUFFER_DEALLOCATE_VAR(buffer, use_category);
}

static void preprocess_text_use(const hb_ot_shape_plan_t * plan,
    hb_buffer_t * buffer,
    hb_font_t * font)
{
	_hb_preprocess_text_vowel_constraints(plan, buffer, font);
}

static bool compose_use(const hb_ot_shape_normalize_context_t * c,
    hb_codepoint_t a,
    hb_codepoint_t b,
    hb_codepoint_t * ab)
{
	/* Avoid recomposing split matras. */
	if(HB_UNICODE_GENERAL_CATEGORY_IS_MARK(c->unicode->general_category(a)))
		return false;

	return (bool)c->unicode->compose(a, b, ab);
}

const hb_ot_complex_shaper_t _hb_ot_complex_shaper_use =
{
	collect_features_use,
	nullptr, /* override_features */
	data_create_use,
	data_destroy_use,
	preprocess_text_use,
	nullptr, /* postprocess_glyphs */
	HB_OT_SHAPE_NORMALIZATION_MODE_COMPOSED_DIACRITICS_NO_SHORT_CIRCUIT,
	nullptr, /* decompose */
	compose_use,
	setup_masks_use,
	HB_TAG_NONE, /* gpos_tag */
	nullptr, /* reorder_marks */
	HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_GDEF_EARLY,
	false, /* fallback_position */
};

#endif
