/*
 * Copyright © 2012  Google, Inc.
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
/**
 * SECTION:hb-set
 * @title: hb-set
 * @short_description: Object representing a set of integers
 * @include: hb.h
 *
 * Set objects represent a mathematical set of integer values.  They are
 * used in non-shaping API to query certain set of characters or glyphs,
 * or other integer values.
 **/

/**
 * hb_set_create: (Xconstructor)
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_set_t * hb_set_create()
{
	hb_set_t * set;
	if(!(set = hb_object_create<hb_set_t> ()))
		return hb_set_get_empty();
	set->init_shallow();
	return set;
}
/**
 * hb_set_get_empty:
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_set_t * hb_set_get_empty()
{
	return const_cast<hb_set_t *> (&Null(hb_set_t));
}

/**
 * hb_set_reference: (skip)
 * @set: a set.
 *
 * Return value: (transfer full):
 *
 * Since: 0.9.2
 **/
hb_set_t * hb_set_reference(hb_set_t * set)
{
	return hb_object_reference(set);
}

/**
 * hb_set_destroy: (skip)
 * @set: a set.
 *
 * Since: 0.9.2
 **/
void hb_set_destroy(hb_set_t * set)
{
	if(hb_object_destroy(set)) {
		set->fini_shallow();
		SAlloc::F(set);
	}
}
/**
 * hb_set_set_user_data: (skip)
 * @set: a set.
 * @key:
 * @data:
 * @destroy:
 * @replace:
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_bool_t hb_set_set_user_data(hb_set_t * set, hb_user_data_key_t * key, void * data, hb_destroy_func_t destroy, hb_bool_t replace)
{
	return hb_object_set_user_data(set, key, data, destroy, replace);
}
/**
 * hb_set_get_user_data: (skip)
 * @set: a set.
 * @key:
 *
 * Return value: (transfer none):
 *
 * Since: 0.9.2
 **/
void * hb_set_get_user_data(hb_set_t * set,
    hb_user_data_key_t * key)
{
	return hb_object_get_user_data(set, key);
}

/**
 * hb_set_allocation_successful:
 * @set: a set.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_bool_t hb_set_allocation_successful(const hb_set_t * set)
{
	return set->successful;
}
/**
 * hb_set_clear:
 * @set: a set.
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_clear(hb_set_t * set)
{
	set->clear();
}
/**
 * hb_set_is_empty:
 * @set: a set.
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.7
 **/
hb_bool_t hb_set_is_empty(const hb_set_t * set)
{
	return set->is_empty();
}

/**
 * hb_set_has:
 * @set: a set.
 * @codepoint:
 *
 *
 *
 * Return value:
 *
 * Since: 0.9.2
 **/
hb_bool_t hb_set_has(const hb_set_t * set, hb_codepoint_t codepoint)
{
	return set->has(codepoint);
}
/**
 * hb_set_add:
 * @set: a set.
 * @codepoint:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_add(hb_set_t * set, hb_codepoint_t codepoint)
{
	set->add(codepoint);
}
/**
 * hb_set_add_range:
 * @set: a set.
 * @first:
 * @last:
 *
 *
 *
 * Since: 0.9.7
 **/
void hb_set_add_range(hb_set_t * set,
    hb_codepoint_t first,
    hb_codepoint_t last)
{
	set->add_range(first, last);
}

/**
 * hb_set_del:
 * @set: a set.
 * @codepoint:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_del(hb_set_t * set,
    hb_codepoint_t codepoint)
{
	set->del(codepoint);
}

/**
 * hb_set_del_range:
 * @set: a set.
 * @first:
 * @last:
 *
 *
 *
 * Since: 0.9.7
 **/
void hb_set_del_range(hb_set_t * set,
    hb_codepoint_t first,
    hb_codepoint_t last)
{
	set->del_range(first, last);
}

/**
 * hb_set_is_equal:
 * @set: a set.
 * @other: other set.
 *
 *
 *
 * Return value: %TRUE if the two sets are equal, %FALSE otherwise.
 *
 * Since: 0.9.7
 **/
hb_bool_t hb_set_is_equal(const hb_set_t * set,
    const hb_set_t * other)
{
	return set->is_equal(other);
}

/**
 * hb_set_is_subset:
 * @set: a set.
 * @larger_set: other set.
 *
 *
 *
 * Return value: %TRUE if the @set is a subset of (or equal to) @larger_set, %FALSE otherwise.
 *
 * Since: 1.8.1
 **/
hb_bool_t hb_set_is_subset(const hb_set_t * set,
    const hb_set_t * larger_set)
{
	return set->is_subset(larger_set);
}

/**
 * hb_set_set:
 * @set: a set.
 * @other:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_set(hb_set_t * set,
    const hb_set_t * other)
{
	set->set(other);
}

/**
 * hb_set_union:
 * @set: a set.
 * @other:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_union(hb_set_t * set,
    const hb_set_t * other)
{
	set->union_(other);
}

/**
 * hb_set_intersect:
 * @set: a set.
 * @other:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_intersect(hb_set_t * set,
    const hb_set_t * other)
{
	set->intersect(other);
}

/**
 * hb_set_subtract:
 * @set: a set.
 * @other:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_subtract(hb_set_t * set,
    const hb_set_t * other)
{
	set->subtract(other);
}

/**
 * hb_set_symmetric_difference:
 * @set: a set.
 * @other:
 *
 *
 *
 * Since: 0.9.2
 **/
void hb_set_symmetric_difference(hb_set_t * set,
    const hb_set_t * other)
{
	set->symmetric_difference(other);
}

#ifndef HB_DISABLE_DEPRECATED
/**
 * hb_set_invert:
 * @set: a set.
 *
 *
 *
 * Since: 0.9.10
 *
 * Deprecated: 1.6.1
 **/
void hb_set_invert(hb_set_t * set CXX_UNUSED_PARAM)
{
}

#endif

/**
 * hb_set_get_population:
 * @set: a set.
 *
 * Returns the number of numbers in the set.
 *
 * Return value: set population.
 *
 * Since: 0.9.7
 **/
uint hb_set_get_population(const hb_set_t * set)
{
	return set->get_population();
}

/**
 * hb_set_get_min:
 * @set: a set.
 *
 * Finds the minimum number in the set.
 *
 * Return value: minimum of the set, or %HB_SET_VALUE_INVALID if set is empty.
 *
 * Since: 0.9.7
 **/
hb_codepoint_t hb_set_get_min(const hb_set_t * set)
{
	return set->get_min();
}

/**
 * hb_set_get_max:
 * @set: a set.
 *
 * Finds the maximum number in the set.
 *
 * Return value: minimum of the set, or %HB_SET_VALUE_INVALID if set is empty.
 *
 * Since: 0.9.7
 **/
hb_codepoint_t hb_set_get_max(const hb_set_t * set)
{
	return set->get_max();
}

/**
 * hb_set_next:
 * @set: a set.
 * @codepoint: (inout):
 *
 * Gets the next number in @set that is greater than current value of @codepoint.
 *
 * Set @codepoint to %HB_SET_VALUE_INVALID to get started.
 *
 * Return value: whether there was a next value.
 *
 * Since: 0.9.2
 **/
hb_bool_t hb_set_next(const hb_set_t * set,
    hb_codepoint_t * codepoint)
{
	return set->next(codepoint);
}

/**
 * hb_set_previous:
 * @set: a set.
 * @codepoint: (inout):
 *
 * Gets the previous number in @set that is lower than current value of @codepoint.
 *
 * Set @codepoint to %HB_SET_VALUE_INVALID to get started.
 *
 * Return value: whether there was a previous value.
 *
 * Since: 1.8.0
 **/
hb_bool_t hb_set_previous(const hb_set_t * set,
    hb_codepoint_t * codepoint)
{
	return set->previous(codepoint);
}

/**
 * hb_set_next_range:
 * @set: a set.
 * @first: (out): output first codepoint in the range.
 * @last: (inout): input current last and output last codepoint in the range.
 *
 * Gets the next consecutive range of numbers in @set that
 * are greater than current value of @last.
 *
 * Set @last to %HB_SET_VALUE_INVALID to get started.
 *
 * Return value: whether there was a next range.
 *
 * Since: 0.9.7
 **/
hb_bool_t hb_set_next_range(const hb_set_t * set,
    hb_codepoint_t * first,
    hb_codepoint_t * last)
{
	return set->next_range(first, last);
}

/**
 * hb_set_previous_range:
 * @set: a set.
 * @first: (inout): input current first and output first codepoint in the range.
 * @last: (out): output last codepoint in the range.
 *
 * Gets the previous consecutive range of numbers in @set that
 * are less than current value of @first.
 *
 * Set @first to %HB_SET_VALUE_INVALID to get started.
 *
 * Return value: whether there was a previous range.
 *
 * Since: 1.8.0
 **/
hb_bool_t hb_set_previous_range(const hb_set_t * set,
    hb_codepoint_t * first,
    hb_codepoint_t * last)
{
	return set->previous_range(first, last);
}
