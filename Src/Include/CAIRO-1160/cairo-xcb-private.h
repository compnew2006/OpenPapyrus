/* Cairo - a vector graphics library with display and print output
 *
 * Copyright © 2005 Red Hat, Inc.
 * Copyright © 2009 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Contributors(s):
 *	Chris Wilson <chris@chris-wilson.co.uk>
 */

#ifndef CAIRO_XCB_PRIVATE_H
#define CAIRO_XCB_PRIVATE_H

#include "cairoint.h"
#include "cairo-xcb.h"
#include "cairo-cache-private.h"
#include "cairo-list-private.h"
#include "cairo-mutex-private.h"
#include "cairo-surface-private.h"
#include <xcb/xcb.h>
#include <xcb/render.h>
#include <xcb/xcbext.h>
#include <pixman.h>

#define XLIB_COORD_MAX 32767

/* maximum number of cached GC's */
#define GC_CACHE_SIZE 4

#define CAIRO_XCB_RENDER_AT_LEAST(major, minor) \
	((XCB_RENDER_MAJOR_VERSION > major) ||  \
	((XCB_RENDER_MAJOR_VERSION == major) && (XCB_RENDER_MINOR_VERSION >= minor)))

typedef struct _cairo_xcb_connection cairo_xcb_connection_t;
typedef struct _cairo_xcb_font cairo_xcb_font_t;
typedef struct _cairo_xcb_screen cairo_xcb_screen_t;
typedef struct _cairo_xcb_surface cairo_xcb_surface_t;
typedef struct _cairo_xcb_picture cairo_xcb_picture_t;
typedef struct _cairo_xcb_shm_mem_pool cairo_xcb_shm_mem_pool_t;
typedef struct _cairo_xcb_shm_info cairo_xcb_shm_info_t;
typedef struct _cairo_xcb_resources cairo_xcb_resources_t;

struct _cairo_xcb_shm_info {
	cairo_xcb_connection_t * connection;
	uint32 shm;
	uint32 offset;
	size_t size;
	void * mem;
	cairo_xcb_shm_mem_pool_t * pool;
	xcb_get_input_focus_cookie_t sync;
	cairo_list_t pending;
};

struct _cairo_xcb_surface {
	cairo_surface_t base;
	cairo_image_surface_t * fallback;
	cairo_boxes_t fallback_damage;

	cairo_xcb_connection_t * connection;
	cairo_xcb_screen_t * screen;

	xcb_drawable_t drawable;
	boolint owns_pixmap;

	boolint deferred_clear;
	cairo_color_t deferred_clear_color;

	int width;
	int height;
	int depth;

	xcb_render_picture_t picture;
	xcb_render_pictformat_t xrender_format;
	pixman_format_code_t pixman_format;
	uint32 precision;

	cairo_list_t link;
};

struct _cairo_xcb_picture {
	cairo_surface_t base;

	cairo_xcb_screen_t * screen;
	xcb_render_picture_t picture;
	xcb_render_pictformat_t xrender_format;
	pixman_format_code_t pixman_format;

	int width, height;

	cairo_extend_t extend;
	cairo_filter_t filter;
	boolint has_component_alpha;
	xcb_render_transform_t transform;

	int x0, y0;
	int x, y;

	cairo_list_t link;
};

#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
typedef struct _cairo_xlib_xcb_surface {
	cairo_surface_t base;

	cairo_xcb_surface_t * xcb;

	/* original settings for query */
	void * display;
	void * screen;
	void * visual;
	void * format;
} cairo_xlib_xcb_surface_t;
#endif

enum {
	GLYPHSET_INDEX_ARGB32,
	GLYPHSET_INDEX_A8,
	GLYPHSET_INDEX_A1,
	NUM_GLYPHSETS
};

typedef struct _cairo_xcb_font_glyphset_free_glyphs {
	xcb_render_glyphset_t glyphset;
	int glyph_count;
	xcb_render_glyph_t glyph_indices[128];
} cairo_xcb_font_glyphset_free_glyphs_t;

typedef struct _cairo_xcb_font_glyphset_info {
	xcb_render_glyphset_t glyphset;
	cairo_format_t format;
	xcb_render_pictformat_t xrender_format;
	cairo_xcb_font_glyphset_free_glyphs_t * pending_free_glyphs;
} cairo_xcb_font_glyphset_info_t;

struct _cairo_xcb_font {
	cairo_scaled_font_private_t base;
	cairo_scaled_font_t             * scaled_font;
	cairo_xcb_connection_t          * connection;
	cairo_xcb_font_glyphset_info_t glyphset_info[NUM_GLYPHSETS];
	cairo_list_t link;
};

struct _cairo_xcb_screen {
	cairo_xcb_connection_t * connection;

	xcb_screen_t           * xcb_screen;
	xcb_render_sub_pixel_t subpixel_order;

	xcb_gcontext_t gc[GC_CACHE_SIZE];
	uint8 gc_depths[GC_CACHE_SIZE];

	cairo_surface_t * stock_colors[CAIRO_STOCK_NUM_COLORS];
	struct {
		cairo_surface_t * picture;
		cairo_color_t color;
	} solid_cache[16];

	int solid_cache_size;

	cairo_cache_t linear_pattern_cache;
	cairo_cache_t radial_pattern_cache;
	cairo_freelist_t pattern_cache_entry_freelist;

	cairo_list_t link;
	cairo_list_t surfaces;
	cairo_list_t pictures;

	boolint has_font_options;
	cairo_font_options_t font_options;
};

struct _cairo_xcb_connection {
	cairo_device_t device;

	xcb_connection_t * xcb_connection;

	xcb_render_pictformat_t standard_formats[5];
	cairo_hash_table_t * xrender_formats;
	cairo_hash_table_t * visual_to_xrender_format;

	uint maximum_request_length;
	uint flags;
	uint original_flags;

	int force_precision;

	const xcb_setup_t * root;
	const xcb_query_extension_reply_t * render;
	const xcb_query_extension_reply_t * shm;
	xcb_render_sub_pixel_t * subpixel_orders;

	cairo_list_t free_xids;
	cairo_freepool_t xid_pool;

	cairo_mutex_t shm_mutex;
	cairo_list_t shm_pools;
	cairo_list_t shm_pending;
	cairo_freepool_t shm_info_freelist;

	cairo_mutex_t screens_mutex;
	cairo_list_t screens;

	cairo_list_t fonts;

	cairo_list_t link;
};

struct _cairo_xcb_resources {
	boolint xft_antialias;
	int xft_lcdfilter;
	boolint xft_hinting;
	int xft_hintstyle;
	int xft_rgba;
};

enum {
	CAIRO_XCB_HAS_RENDER                = 0x0001,
	CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES        = 0x0002,
	CAIRO_XCB_RENDER_HAS_COMPOSITE      = 0x0004,
	CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS   = 0x0008,
	CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS       = 0x0010,
	CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM      = 0x0020,
	CAIRO_XCB_RENDER_HAS_FILTERS        = 0x0040,
	CAIRO_XCB_RENDER_HAS_PDF_OPERATORS  = 0x0080,
	CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT        = 0x0100,
	CAIRO_XCB_RENDER_HAS_GRADIENTS      = 0x0200,
	CAIRO_XCB_RENDER_HAS_FILTER_GOOD    = 0x0400,
	CAIRO_XCB_RENDER_HAS_FILTER_BEST    = 0x0800,

	CAIRO_XCB_HAS_SHM                   = 0x80000000,

	CAIRO_XCB_RENDER_MASK = CAIRO_XCB_HAS_RENDER |
	    CAIRO_XCB_RENDER_HAS_FILL_RECTANGLES |
	    CAIRO_XCB_RENDER_HAS_COMPOSITE |
	    CAIRO_XCB_RENDER_HAS_COMPOSITE_TRAPEZOIDS |
	    CAIRO_XCB_RENDER_HAS_COMPOSITE_GLYPHS |
	    CAIRO_XCB_RENDER_HAS_PICTURE_TRANSFORM |
	    CAIRO_XCB_RENDER_HAS_FILTERS |
	    CAIRO_XCB_RENDER_HAS_PDF_OPERATORS |
	    CAIRO_XCB_RENDER_HAS_EXTENDED_REPEAT |
	    CAIRO_XCB_RENDER_HAS_GRADIENTS |
	    CAIRO_XCB_RENDER_HAS_FILTER_GOOD |
	    CAIRO_XCB_RENDER_HAS_FILTER_BEST,
	CAIRO_XCB_SHM_MASK    = CAIRO_XCB_HAS_SHM
};

#define CAIRO_XCB_SHM_SMALL_IMAGE 8192

cairo_private extern const cairo_surface_backend_t _cairo_xcb_surface_backend;

/**
 * _cairo_surface_is_xcb:
 * @surface: a #cairo_surface_t
 *
 * Checks if a surface is an #cairo_xcb_surface_t
 *
 * Return value: %TRUE if the surface is an xcb surface
 **/
static inline boolint _cairo_surface_is_xcb(const cairo_surface_t * surface)
{
	/* _cairo_surface_nil sets a NULL backend so be safe */
	return surface->backend && surface->backend->type == CAIRO_SURFACE_TYPE_XCB;
}

cairo_private cairo_xcb_connection_t * _cairo_xcb_connection_get(xcb_connection_t * connection);

static inline cairo_xcb_connection_t * _cairo_xcb_connection_reference(cairo_xcb_connection_t * connection)
{
	return (cairo_xcb_connection_t*)cairo_device_reference(&connection->device);
}

cairo_private xcb_render_pictformat_t _cairo_xcb_connection_get_xrender_format(cairo_xcb_connection_t * connection,
    pixman_format_code_t pixman_format);

cairo_private xcb_render_pictformat_t _cairo_xcb_connection_get_xrender_format_for_visual(cairo_xcb_connection_t * connection,
    const xcb_visualid_t visual);

static inline cairo_status_t cairo_warn _cairo_xcb_connection_acquire(cairo_xcb_connection_t * connection)
{
	return cairo_device_acquire(&connection->device);
}

cairo_private uint32 _cairo_xcb_connection_get_xid(cairo_xcb_connection_t * connection);

cairo_private void _cairo_xcb_connection_put_xid(cairo_xcb_connection_t * connection,
    uint32 xid);

static inline void _cairo_xcb_connection_release(cairo_xcb_connection_t * connection)
{
	cairo_device_release(&connection->device);
}

static inline void _cairo_xcb_connection_destroy(cairo_xcb_connection_t * connection)
{
	cairo_device_destroy(&connection->device);
}

cairo_private cairo_int_status_t _cairo_xcb_connection_allocate_shm_info(cairo_xcb_connection_t * display,
    size_t size,
    boolint might_reuse,
    cairo_xcb_shm_info_t ** shm_info_out);

cairo_private void _cairo_xcb_shm_info_destroy(cairo_xcb_shm_info_t * shm_info);
cairo_private void _cairo_xcb_connection_shm_mem_pools_flush(cairo_xcb_connection_t * connection);
cairo_private void _cairo_xcb_connection_shm_mem_pools_fini(cairo_xcb_connection_t * connection);
cairo_private void _cairo_xcb_font_close(cairo_xcb_font_t * font);
cairo_private cairo_xcb_screen_t * _cairo_xcb_screen_get(xcb_connection_t * connection, xcb_screen_t * screen);
cairo_private void _cairo_xcb_screen_finish(cairo_xcb_screen_t * screen);
cairo_private xcb_gcontext_t _cairo_xcb_screen_get_gc(cairo_xcb_screen_t * screen, xcb_drawable_t drawable, int depth);
cairo_private void _cairo_xcb_screen_put_gc(cairo_xcb_screen_t * screen, int depth, xcb_gcontext_t gc);
cairo_private cairo_font_options_t * _cairo_xcb_screen_get_font_options(cairo_xcb_screen_t * screen);
cairo_private cairo_status_t _cairo_xcb_screen_store_linear_picture(cairo_xcb_screen_t * screen, const cairo_linear_pattern_t * linear, cairo_surface_t * picture);
cairo_private cairo_surface_t * _cairo_xcb_screen_lookup_linear_picture(cairo_xcb_screen_t * screen, const cairo_linear_pattern_t * linear);
cairo_private cairo_status_t _cairo_xcb_screen_store_radial_picture(cairo_xcb_screen_t * screen, const cairo_radial_pattern_t * radial, cairo_surface_t * picture);
cairo_private cairo_surface_t * _cairo_xcb_screen_lookup_radial_picture(cairo_xcb_screen_t * screen, const cairo_radial_pattern_t * radial);
cairo_private cairo_surface_t * _cairo_xcb_surface_create_similar_image(void * abstrct_other, cairo_format_t format, int width, int height);
cairo_private cairo_surface_t * _cairo_xcb_surface_create_similar(void * abstract_other, cairo_content_t content, int width, int height);
cairo_private cairo_surface_t * _cairo_xcb_surface_create_internal(cairo_xcb_screen_t          * screen,
    xcb_drawable_t drawable,
    boolint owns_pixmap,
    pixman_format_code_t pixman_format,
    xcb_render_pictformat_t xrender_format,
    int width,
    int height);

cairo_private_no_warn boolint _cairo_xcb_surface_get_extents(void * abstract_surface,
    cairo_rectangle_int_t * extents);

cairo_private cairo_int_status_t _cairo_xcb_render_compositor_paint(const cairo_compositor_t     * compositor,
    cairo_composite_rectangles_t * extents);

cairo_private cairo_int_status_t _cairo_xcb_render_compositor_mask(const cairo_compositor_t     * compositor,
    cairo_composite_rectangles_t * extents);

cairo_private cairo_int_status_t _cairo_xcb_render_compositor_stroke(const cairo_compositor_t     * compositor,
    cairo_composite_rectangles_t * extents,
    const cairo_path_fixed_t     * path,
    const cairo_stroke_style_t   * style,
    const cairo_matrix_t         * ctm,
    const cairo_matrix_t         * ctm_inverse,
    double tolerance,
    cairo_antialias_t antialias);

cairo_private cairo_int_status_t _cairo_xcb_render_compositor_fill(const cairo_compositor_t     * compositor,
    cairo_composite_rectangles_t * extents,
    const cairo_path_fixed_t     * path,
    cairo_fill_rule_t fill_rule,
    double tolerance,
    cairo_antialias_t antialias);

cairo_private cairo_int_status_t _cairo_xcb_render_compositor_glyphs(const cairo_compositor_t     * compositor,
    cairo_composite_rectangles_t * extents,
    cairo_scaled_font_t * scaled_font,
    cairo_glyph_t * glyphs,
    int num_glyphs,
    boolint overlap);
cairo_private void _cairo_xcb_surface_scaled_font_fini(cairo_scaled_font_t * scaled_font);

cairo_private void _cairo_xcb_surface_scaled_glyph_fini(cairo_scaled_glyph_t * scaled_glyph,
    cairo_scaled_font_t  * scaled_font);

cairo_private cairo_status_t _cairo_xcb_surface_clear(cairo_xcb_surface_t * dst);

cairo_private cairo_status_t _cairo_xcb_surface_core_copy_boxes(cairo_xcb_surface_t         * dst,
    const cairo_pattern_t        * src_pattern,
    const cairo_rectangle_int_t  * extents,
    const cairo_boxes_t          * boxes);

cairo_private cairo_status_t _cairo_xcb_surface_core_fill_boxes(cairo_xcb_surface_t * dst,
    const cairo_color_t * color,
    cairo_boxes_t * boxes);

cairo_private xcb_pixmap_t _cairo_xcb_connection_create_pixmap(cairo_xcb_connection_t * connection,
    uint8 depth,
    xcb_drawable_t drawable,
    uint16 width,
    uint16 height);

cairo_private void _cairo_xcb_connection_free_pixmap(cairo_xcb_connection_t * connection,
    xcb_pixmap_t pixmap);

cairo_private xcb_gcontext_t _cairo_xcb_connection_create_gc(cairo_xcb_connection_t * connection,
    xcb_drawable_t drawable,
    uint32 value_mask,
    uint32 * values);

cairo_private void _cairo_xcb_connection_free_gc(cairo_xcb_connection_t * connection,
    xcb_gcontext_t gc);

cairo_private void _cairo_xcb_connection_change_gc(cairo_xcb_connection_t * connection,
    xcb_gcontext_t gc,
    uint32 value_mask,
    uint32 * values);

cairo_private void _cairo_xcb_connection_copy_area(cairo_xcb_connection_t * connection,
    xcb_drawable_t src,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    int16 src_x,
    int16 src_y,
    int16 dst_x,
    int16 dst_y,
    uint16 width,
    uint16 height);

cairo_private void _cairo_xcb_connection_put_image(cairo_xcb_connection_t * connection,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    uint16 width,
    uint16 height,
    int16 dst_x,
    int16 dst_y,
    uint8 depth,
    uint32 length,
    void * data);

cairo_private void _cairo_xcb_connection_put_subimage(cairo_xcb_connection_t * connection,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    int16 src_x,
    int16 src_y,
    uint16 width,
    uint16 height,
    uint16 cpp,
    int stride,
    int16 dst_x,
    int16 dst_y,
    uint8 depth,
    void * data);

cairo_private xcb_get_image_reply_t * _cairo_xcb_connection_get_image(cairo_xcb_connection_t * connection,
    xcb_drawable_t src,
    int16 src_x,
    int16 src_y,
    uint16 width,
    uint16 height);

cairo_private void _cairo_xcb_connection_poly_fill_rectangle(cairo_xcb_connection_t * connection,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    uint32 num_rectangles,
    xcb_rectangle_t * rectangles);

cairo_private cairo_status_t _cairo_xcb_shm_image_create(cairo_xcb_connection_t * connection,
    pixman_format_code_t pixman_format,
    int width, int height,
    cairo_image_surface_t ** image_out,
    cairo_xcb_shm_info_t ** shm_info_out);

#if CAIRO_HAS_XCB_SHM_FUNCTIONS
cairo_private uint32 _cairo_xcb_connection_shm_attach(cairo_xcb_connection_t * connection,
    uint32 id,
    boolint readonly);

cairo_private void _cairo_xcb_connection_shm_put_image(cairo_xcb_connection_t * connection,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    uint16 total_width,
    uint16 total_height,
    int16 src_x,
    int16 src_y,
    uint16 width,
    uint16 height,
    int16 dst_x,
    int16 dst_y,
    uint8 depth,
    uint32 shm,
    uint32 offset);

cairo_private cairo_status_t _cairo_xcb_connection_shm_get_image(cairo_xcb_connection_t * connection,
    xcb_drawable_t src,
    int16 src_x,
    int16 src_y,
    uint16 width,
    uint16 height,
    uint32 shmseg,
    uint32 offset);

cairo_private void _cairo_xcb_connection_shm_detach(cairo_xcb_connection_t * connection,
    uint32 segment);
#else
static inline void _cairo_xcb_connection_shm_put_image(cairo_xcb_connection_t * connection,
    xcb_drawable_t dst,
    xcb_gcontext_t gc,
    uint16 total_width,
    uint16 total_height,
    int16 src_x,
    int16 src_y,
    uint16 width,
    uint16 height,
    int16 dst_x,
    int16 dst_y,
    uint8 depth,
    uint32 shm,
    uint32 offset)
{
	ASSERT_NOT_REACHED;
}

#endif

cairo_private void _cairo_xcb_connection_render_create_picture(cairo_xcb_connection_t  * connection,
    xcb_render_picture_t picture,
    xcb_drawable_t drawable,
    xcb_render_pictformat_t format,
    uint32 value_mask,
    uint32                  * value_list);

cairo_private void _cairo_xcb_connection_render_change_picture(cairo_xcb_connection_t     * connection,
    xcb_render_picture_t picture,
    uint32 value_mask,
    uint32             * value_list);

cairo_private void _cairo_xcb_connection_render_set_picture_clip_rectangles(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture,
    int16 clip_x_origin,
    int16 clip_y_origin,
    uint32 rectangles_len,
    xcb_rectangle_t       * rectangles);

cairo_private void _cairo_xcb_connection_render_free_picture(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture);

cairo_private void _cairo_xcb_connection_render_composite(cairo_xcb_connection_t     * connection,
    uint8 op,
    xcb_render_picture_t src,
    xcb_render_picture_t mask,
    xcb_render_picture_t dst,
    int16 src_x,
    int16 src_y,
    int16 mask_x,
    int16 mask_y,
    int16 dst_x,
    int16 dst_y,
    uint16 width,
    uint16 height);

cairo_private void _cairo_xcb_connection_render_trapezoids(cairo_xcb_connection_t * connection,
    uint8 op,
    xcb_render_picture_t src,
    xcb_render_picture_t dst,
    xcb_render_pictformat_t mask_format,
    int16 src_x,
    int16 src_y,
    uint32 traps_len,
    xcb_render_trapezoid_t       * traps);

cairo_private void _cairo_xcb_connection_render_create_glyph_set(cairo_xcb_connection_t   * connection,
    xcb_render_glyphset_t id,
    xcb_render_pictformat_t format);

cairo_private void _cairo_xcb_connection_render_free_glyph_set(cairo_xcb_connection_t * connection,
    xcb_render_glyphset_t glyphset);

cairo_private void _cairo_xcb_connection_render_add_glyphs(cairo_xcb_connection_t             * connection,
    xcb_render_glyphset_t glyphset,
    uint32 num_glyphs,
    uint32                     * glyphs_id,
    xcb_render_glyphinfo_t       * glyphs,
    uint32 data_len,
    uint8 * data);

cairo_private void _cairo_xcb_connection_render_free_glyphs(cairo_xcb_connection_t * connection,
    xcb_render_glyphset_t glyphset,
    uint32 num_glyphs,
    xcb_render_glyph_t       * glyphs);

cairo_private void _cairo_xcb_connection_render_composite_glyphs_8(cairo_xcb_connection_t * connection,
    uint8 op,
    xcb_render_picture_t src,
    xcb_render_picture_t dst,
    xcb_render_pictformat_t mask_format,
    xcb_render_glyphset_t glyphset,
    int16 src_x,
    int16 src_y,
    uint32 glyphcmds_len,
    uint8 * glyphcmds);

cairo_private void _cairo_xcb_connection_render_composite_glyphs_16(cairo_xcb_connection_t * connection,
    uint8 op,
    xcb_render_picture_t src,
    xcb_render_picture_t dst,
    xcb_render_pictformat_t mask_format,
    xcb_render_glyphset_t glyphset,
    int16 src_x,
    int16 src_y,
    uint32 glyphcmds_len,
    uint8 * glyphcmds);

cairo_private void _cairo_xcb_connection_render_composite_glyphs_32(cairo_xcb_connection_t * connection,
    uint8 op,
    xcb_render_picture_t src,
    xcb_render_picture_t dst,
    xcb_render_pictformat_t mask_format,
    xcb_render_glyphset_t glyphset,
    int16 src_x,
    int16 src_y,
    uint32 glyphcmds_len,
    uint8 * glyphcmds);

cairo_private void _cairo_xcb_connection_render_fill_rectangles(cairo_xcb_connection_t * connection,
    uint8 op,
    xcb_render_picture_t dst,
    xcb_render_color_t color,
    uint32 num_rects,
    xcb_rectangle_t       * rects);

cairo_private void _cairo_xcb_connection_render_set_picture_transform(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture,
    xcb_render_transform_t  * transform);

cairo_private void _cairo_xcb_connection_render_set_picture_filter(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture,
    uint16 filter_len,
    char * filter);

cairo_private void _cairo_xcb_connection_render_create_solid_fill(cairo_xcb_connection_t     * connection,
    xcb_render_picture_t picture,
    xcb_render_color_t color);

cairo_private void _cairo_xcb_connection_render_create_linear_gradient(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture,
    xcb_render_pointfix_t p1,
    xcb_render_pointfix_t p2,
    uint32 num_stops,
    xcb_render_fixed_t * stops,
    xcb_render_color_t * colors);

cairo_private void _cairo_xcb_connection_render_create_radial_gradient(cairo_xcb_connection_t * connection,
    xcb_render_picture_t picture,
    xcb_render_pointfix_t inner,
    xcb_render_pointfix_t outer,
    xcb_render_fixed_t inner_radius,
    xcb_render_fixed_t outer_radius,
    uint32 num_stops,
    xcb_render_fixed_t * stops,
    xcb_render_color_t * colors);

cairo_private void _cairo_xcb_connection_render_create_conical_gradient(cairo_xcb_connection_t * c,
    xcb_render_picture_t picture,
    xcb_render_pointfix_t center,
    xcb_render_fixed_t angle,
    uint32 num_stops,
    xcb_render_fixed_t * stops,
    xcb_render_color_t * colors);
#if CAIRO_HAS_XLIB_XCB_FUNCTIONS
slim_hidden_proto(cairo_xcb_surface_create);
slim_hidden_proto(cairo_xcb_surface_create_for_bitmap);
slim_hidden_proto(cairo_xcb_surface_create_with_xrender_format);
slim_hidden_proto(cairo_xcb_surface_set_size);
slim_hidden_proto(cairo_xcb_surface_set_drawable);
slim_hidden_proto(cairo_xcb_device_debug_get_precision);
slim_hidden_proto_no_warn(cairo_xcb_device_debug_set_precision);
slim_hidden_proto_no_warn(cairo_xcb_device_debug_cap_xrender_version);
#endif

cairo_private void _cairo_xcb_resources_get(cairo_xcb_screen_t * screen,
    cairo_xcb_resources_t * resources);

#endif /* CAIRO_XCB_PRIVATE_H */
