/*
 * Copyright © 2006 Joonas Pihlaja
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 */
#include "cairoint.h"
#pragma hdrstop

void _cairo_freelist_init(cairo_freelist_t * freelist, unsigned nodesize)
{
	memzero(freelist, sizeof(cairo_freelist_t));
	freelist->nodesize = nodesize;
}

void _cairo_freelist_fini(cairo_freelist_t * freelist)
{
	for(cairo_freelist_node_t * node = freelist->first_free_node; node;) {
		cairo_freelist_node_t * next;
		VG(VALGRIND_MAKE_MEM_DEFINED(node, sizeof(node->next)));
		next = node->next;
		SAlloc::F(node);
		node = next;
	}
}

void * _cairo_freelist_alloc(cairo_freelist_t * freelist)
{
	if(freelist->first_free_node) {
		cairo_freelist_node_t * node = freelist->first_free_node;
		VG(VALGRIND_MAKE_MEM_DEFINED(node, sizeof(node->next)));
		freelist->first_free_node = node->next;
		VG(VALGRIND_MAKE_MEM_UNDEFINED(node, freelist->nodesize));
		return node;
	}
	return _cairo_malloc(freelist->nodesize);
}

void * _cairo_freelist_calloc(cairo_freelist_t * freelist)
{
	void * node = _cairo_freelist_alloc(freelist);
	memzero(node, freelist->nodesize);
	return node;
}

void _cairo_freelist_free(cairo_freelist_t * freelist, void * voidnode)
{
	cairo_freelist_node_t * node = static_cast<cairo_freelist_node_t *>(voidnode);
	if(node) {
		node->next = freelist->first_free_node;
		freelist->first_free_node = node;
		VG(VALGRIND_MAKE_MEM_UNDEFINED(node, freelist->nodesize));
	}
}

void _cairo_freepool_init(cairo_freepool_t * freepool, unsigned nodesize)
{
	freepool->first_free_node = NULL;
	freepool->pools = &freepool->embedded_pool;
	freepool->freepools = NULL;
	freepool->nodesize = nodesize;

	freepool->embedded_pool.next = NULL;
	freepool->embedded_pool.size = sizeof(freepool->embedded_data);
	freepool->embedded_pool.rem = sizeof(freepool->embedded_data);
	freepool->embedded_pool.data = freepool->embedded_data;

	VG(VALGRIND_MAKE_MEM_UNDEFINED(freepool->embedded_data, sizeof(freepool->embedded_data)));
}

void FASTCALL _cairo_freepool_fini(cairo_freepool_t * freepool)
{
	cairo_freelist_pool_t * pool = freepool->pools;
	while(pool != &freepool->embedded_pool) {
		cairo_freelist_pool_t * next = pool->next;
		SAlloc::F(pool);
		pool = next;
	}
	pool = freepool->freepools;
	while(pool != NULL) {
		cairo_freelist_pool_t * next = pool->next;
		SAlloc::F(pool);
		pool = next;
	}
	VG(VALGRIND_MAKE_MEM_UNDEFINED(freepool, sizeof(freepool)));
}

void * _cairo_freepool_alloc_from_new_pool(cairo_freepool_t * freepool)
{
	cairo_freelist_pool_t * pool;
	int poolsize;
	if(freepool->freepools != NULL) {
		pool = freepool->freepools;
		freepool->freepools = pool->next;
		poolsize = pool->size;
	}
	else {
		if(freepool->pools != &freepool->embedded_pool)
			poolsize = 2 * freepool->pools->size;
		else
			poolsize = (128 * freepool->nodesize + 8191) & -8192;
		pool = (cairo_freelist_pool_t *)_cairo_malloc(sizeof(cairo_freelist_pool_t) + poolsize);
		if(UNLIKELY(pool == NULL))
			return pool;
		pool->size = poolsize;
	}
	pool->next = freepool->pools;
	freepool->pools = pool;
	pool->rem = poolsize - freepool->nodesize;
	pool->data = (uint8 *)(pool + 1) + freepool->nodesize;
	VG(VALGRIND_MAKE_MEM_UNDEFINED(pool->data, pool->rem));
	return pool + 1;
}

cairo_status_t _cairo_freepool_alloc_array(cairo_freepool_t * freepool, int count, void ** array)
{
	int i;
	for(i = 0; i < count; i++) {
		cairo_freelist_node_t * node = freepool->first_free_node;
		if(LIKELY(node != NULL)) {
			VG(VALGRIND_MAKE_MEM_DEFINED(node, sizeof(node->next)));
			freepool->first_free_node = node->next;
			VG(VALGRIND_MAKE_MEM_UNDEFINED(node, freepool->nodesize));
		}
		else {
			node = (cairo_freelist_node_t *)_cairo_freepool_alloc_from_pool(freepool);
			if(UNLIKELY(node == NULL))
				goto CLEANUP;
		}
		array[i] = node;
	}
	return CAIRO_STATUS_SUCCESS;
CLEANUP:
	while(i--)
		_cairo_freepool_free(freepool, array[i]);
	return _cairo_error(CAIRO_STATUS_NO_MEMORY);
}
