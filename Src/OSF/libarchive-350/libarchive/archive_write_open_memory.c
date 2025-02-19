/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_open_memory.c,v 1.3 2007/01/09 08:05:56 kientzle Exp $");

#include "archive.h"

struct write_memory_data {
	size_t used;
	size_t size;
	size_t * client_size;
	uchar * buff;
};

static int memory_write_free(struct archive *, void *);
static int memory_write_open(struct archive *, void *);
static ssize_t  memory_write(struct archive *, void *, const void * buff, size_t);

/*
 * Client provides a pointer to a block of memory to receive
 * the data.  The 'size' param both tells us the size of the
 * client buffer and lets us tell the client the final size.
 */
int archive_write_open_memory(struct archive * a, void * buff, size_t buffSize, size_t * used)
{
	struct write_memory_data * mine = (struct write_memory_data *)SAlloc::C(1, sizeof(*mine));
	if(mine == NULL) {
		archive_set_error(a, ENOMEM, "No memory");
		return ARCHIVE_FATAL;
	}
	mine->buff = static_cast<uchar *>(buff);
	mine->size = buffSize;
	mine->client_size = used;
	return (archive_write_open2(a, mine, memory_write_open, reinterpret_cast<archive_write_callback *>(memory_write), NULL, memory_write_free));
}

static int memory_write_open(struct archive * a, void * client_data)
{
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	mine->used = 0;
	if(mine->client_size != NULL)
		*mine->client_size = mine->used;
	/* Disable padding if it hasn't been set explicitly. */
	if(-1 == archive_write_get_bytes_in_last_block(a))
		archive_write_set_bytes_in_last_block(a, 1);
	return ARCHIVE_OK;
}

/*
 * Copy the data into the client buffer.
 * Note that we update mine->client_size on every write.
 * In particular, this means the client can follow exactly
 * how much has been written into their buffer at any time.
 */
static ssize_t memory_write(struct archive * a, void * client_data, const void * buff, size_t length)
{
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	if(mine->used + length > mine->size) {
		archive_set_error(a, ENOMEM, "Buffer exhausted");
		return ARCHIVE_FATAL;
	}
	memcpy(mine->buff + mine->used, buff, length);
	mine->used += length;
	if(mine->client_size != NULL)
		*mine->client_size = mine->used;
	return (length);
}

static int memory_write_free(struct archive * a, void * client_data)
{
	CXX_UNUSED(a);
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	if(mine == NULL)
		return ARCHIVE_OK;
	SAlloc::F(mine);
	return ARCHIVE_OK;
}
