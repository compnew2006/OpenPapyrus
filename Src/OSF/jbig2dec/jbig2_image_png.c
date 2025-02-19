/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
 */

/*
    jbig2dec
 */
#include "jbig2dec-internal.h"
#pragma hdrstop
#include <png.h>
#ifndef OLD_LIB_PNG
	#if PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR < 2
		#define OLD_LIB_PNG 1
	#else
		#define OLD_LIB_PNG 0
	#endif
#endif
#if OLD_LIB_PNG
	#include <pngstruct.h>
#endif
#define CVT_PTR(ptr) (ptr)

/* take an image structure and write it out in png format */

static void jbig2_png_write_data(png_structp png_ptr, png_bytep data, size_t length)
{
	size_t check;
#if OLD_LIB_PNG
	png_FILE_p f = (png_FILE_p)png_ptr->io_ptr;
#else
	png_FILE_p f = (png_FILE_p)png_get_io_ptr(png_ptr);
#endif
	check = fwrite(data, 1, length, f);
	if(check != length) {
		png_error(png_ptr, "write error");
	}
}

static void jbig2_png_flush(png_structp png_ptr)
{
#if OLD_LIB_PNG
	png_FILE_p f = (png_FILE_p)png_ptr->io_ptr;
#else
	png_FILE_p f = (png_FILE_p)png_get_io_ptr(png_ptr);
#endif
	if(f != NULL)
		fflush(f);
}

/* write out an image struct in png format to an open file pointer */

int jbig2_image_write_png(Jbig2Image * image, FILE * out)
{
	uint32_t i;
	png_infop info;
	png_bytep rowpointer;
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(png == NULL) {
		slfprintf_stderr("unable to create png structure\n");
		return 2;
	}
	info = png_create_info_struct(png);
	if(info == NULL) {
		slfprintf_stderr("unable to create png info structure\n");
		png_destroy_write_struct(&png, (png_infopp)NULL);
		return 3;
	}
	/* set/check error handling */
	if(setjmp(png_jmpbuf(png))) {
		/* we've returned here after an internal error */
		slfprintf_stderr("internal error in libpng saving file\n");
		png_destroy_write_struct(&png, &info);
		return 4;
	}
	/* png_init_io() doesn't work linking dynamically to libpng on win32
	   one has to either link statically or use callbacks because of runtime
	   variations */
	/* png_init_io(png, out); */
	png_set_write_fn(png, (void *)out, jbig2_png_write_data, jbig2_png_flush);
	/* now we fill out the info structure with our format data */
	png_set_IHDR(png,
	    info,
	    image->width,
	    image->height,
	    1,
	    PNG_COLOR_TYPE_GRAY,
	    PNG_INTERLACE_NONE,
	    PNG_COMPRESSION_TYPE_DEFAULT,
	    PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);
	/* png natively treats 0 as black. This will convert for us */
	png_set_invert_mono(png);
	/* write out each row in turn */
	rowpointer = (png_bytep)image->data;
	for(i = 0; i < image->height; i++) {
		png_write_row(png, rowpointer);
		rowpointer += image->stride;
	}
	/* finish and clean up */
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);
	return 0;
}

int jbig2_image_write_png_file(Jbig2Image * image, char * filename)
{
	FILE * out;
	int code;
	if((out = fopen(filename, "wb")) == NULL) {
		slfprintf_stderr("unable to open '%s' for writing\n", filename);
		return 1;
	}
	code = jbig2_image_write_png(image, out);
	fclose(out);
	return (code);
}
