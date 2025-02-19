/*
 * Copyright © 2018  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * Google Author(s): Behdad Esfahbod
 */
#include "harfbuzz-internal.h"
#pragma hdrstop
#include "hb-map.hh"
/**
 * SECTION:hb-map
 * @title: hb-map
 * @short_description: Object representing integer to integer mapping
 * @include: hb.h
 *
 * Map objects are integer-to-integer hash-maps.  Currently they are
 * not used in the HarfBuzz public API, but are provided for client's
 * use if desired.
 **/

/**
 * hb_map_create: (Xconstructor)
 *
 * Return value: (transfer full):
 *
 * Since: 1.7.7
 **/
hb_map_t * hb_map_create()
{
	hb_map_t * map;
	if(!(map = hb_object_create<hb_map_t> ()))
		return hb_map_get_empty();
	map->init_shallow();
	return map;
}
/**
 * hb_map_get_empty:
 *
 * Return value: (transfer full):
 *
 * Since: 1.7.7
 **/
hb_map_t * hb_map_get_empty()
{
	return const_cast<hb_map_t *> (&Null(hb_map_t));
}

/**
 * hb_map_reference: (skip)
 * @map: a map.
 *
 * Return value: (transfer full):
 *
 * Since: 1.7.7
 **/
hb_map_t * hb_map_reference(hb_map_t * map)
{
	return hb_object_reference(map);
}

/**
 * hb_map_destroy: (skip)
 * @map: a map.
 *
 * Since: 1.7.7
 **/
void hb_map_destroy(hb_map_t * map)
{
	if(!hb_object_destroy(map)) return;
	map->fini_shallow();
	SAlloc::F(map);
}
/**
 * hb_map_set_user_data: (skip)
 * @map: a map.
 * @key:
 * @data:
 * @destroy:
 * @replace:
 *
 * Return value:
 *
 * Since: 1.7.7
 **/
hb_bool_t hb_map_set_user_data(hb_map_t * map, hb_user_data_key_t * key, void * data, hb_destroy_func_t destroy, hb_bool_t replace)
{
	return hb_object_set_user_data(map, key, data, destroy, replace);
}

/**
 * hb_map_get_user_data: (skip)
 * @map: a map.
 * @key:
 *
 * Return value: (transfer none):
 *
 * Since: 1.7.7
 **/
void * hb_map_get_user_data(hb_map_t * map,
    hb_user_data_key_t * key)
{
	return hb_object_get_user_data(map, key);
}

/**
 * hb_map_allocation_successful:
 * @map: a map.
 *
 *
 *
 * Return value:
 *
 * Since: 1.7.7
 **/
hb_bool_t hb_map_allocation_successful(const hb_map_t * map)
{
	return map->successful;
}

/**
 * hb_map_set:
 * @map: a map.
 * @key:
 * @value:
 *
 *
 *
 * Since: 1.7.7
 **/
void hb_map_set(hb_map_t * map,
    hb_codepoint_t key,
    hb_codepoint_t value)
{
	map->set(key, value);
}

/**
 * hb_map_get:
 * @map: a map.
 * @key:
 *
 *
 *
 * Since: 1.7.7
 **/
hb_codepoint_t hb_map_get(const hb_map_t * map,
    hb_codepoint_t key)
{
	return map->get(key);
}

/**
 * hb_map_del:
 * @map: a map.
 * @key:
 *
 *
 *
 * Since: 1.7.7
 **/
void hb_map_del(hb_map_t * map,
    hb_codepoint_t key)
{
	map->del(key);
}

/**
 * hb_map_has:
 * @map: a map.
 * @key:
 *
 *
 *
 * Since: 1.7.7
 **/
hb_bool_t hb_map_has(const hb_map_t * map,
    hb_codepoint_t key)
{
	return map->has(key);
}

/**
 * hb_map_clear:
 * @map: a map.
 *
 *
 *
 * Since: 1.7.7
 **/
void hb_map_clear(hb_map_t * map)
{
	return map->clear();
}

/**
 * hb_map_is_empty:
 * @map: a map.
 *
 *
 *
 * Since: 1.7.7
 **/
hb_bool_t hb_map_is_empty(const hb_map_t * map)
{
	return map->is_empty();
}

/**
 * hb_map_get_population:
 * @map: a map.
 *
 *
 *
 * Since: 1.7.7
 **/
uint hb_map_get_population(const hb_map_t * map)
{
	return map->get_population();
}
