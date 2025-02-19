/** @file
 * @brief Interface to Btree cursors
 */
/* Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002,2003,2004,2006,2007,2008,2009,2010,2012,2013,2014,2016 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifndef XAPIAN_INCLUDED_GLASS_CURSOR_H
#define XAPIAN_INCLUDED_GLASS_CURSOR_H

#include "glass_defs.h"
#include "alignment_cast.h"
//#include "omassert.h"
//#include <algorithm>
//#include <cstring>
//#include <string>

using std::string;
class GlassTable;

#define BLK_UNUSED uint4(-1)

namespace Glass {
class Cursor {
	// Prevent copying
	Cursor(const Cursor &);
	Cursor & operator=(const Cursor &);
	char * data; /// Pointer to reference counted data.
public:
	Cursor() : data(0), c(-1), rewrite(false) 
	{
	}
	~Cursor() 
	{
		destroy();
	}
	uint8 * init(unsigned block_size) 
	{
		if(data && refs() > 1) {
			--refs();
			data = NULL;
		}
		if(!data)
			data = new char[block_size + 8];
		refs() = 1;
		set_n(BLK_UNUSED);
		rewrite = false;
		c = -1;
		return reinterpret_cast<uint8*>(data + 8);
	}
	const uint8 * clone(const Cursor & o) 
	{
		if(data != o.data) {
			destroy();
			data = o.data;
			++refs();
		}
		return reinterpret_cast<uint8*>(data + 8);
	}
	void swap(Cursor & o) 
	{
		std::swap(data, o.data);
		std::swap(c, o.c);
		std::swap(rewrite, o.rewrite);
	}
	void destroy() 
	{
		if(data) {
			if(--refs() == 0)
				delete [] data;
			data = NULL;
			rewrite = false;
		}
	}
	uint4 & refs() const 
	{
		Assert(data);
		return *alignment_cast<uint4*>(data);
	}
	/** Get the block number.
	 *
	 *  Returns BLK_UNUSED if no block is currently loaded.
	 */
	uint4 get_n() const 
	{
		Assert(data);
		return *alignment_cast<uint4*>(data + 4);
	}
	void set_n(uint4 n) 
	{
		Assert(data);
		// Assert(refs() == 1);
		*alignment_cast<uint4*>(data + 4) = n;
	}
	/** Get pointer to block.
	 *
	 * Returns NULL if no block is currently loaded.
	 */
	const uint8 * get_p() const 
	{
		if(UNLIKELY(!data)) 
			return NULL;
		return reinterpret_cast<uint8*>(data + 8);
	}
	uint8 * get_modifiable_p(unsigned block_size) 
	{
		if(UNLIKELY(!data)) 
			return NULL;
		if(refs() > 1) {
			char * new_data = new char[block_size + 8];
			memcpy(new_data, data, block_size + 8);
			--refs();
			data = new_data;
			refs() = 1;
		}
		return reinterpret_cast<uint8*>(data + 8);
	}

	int c; /// offset in the block's directory
	bool rewrite; /// true if the block is not the same as on disk, and so needs rewriting
};
}

/** A cursor pointing to a position in a Btree table, for reading several
 *  entries in order, or finding approximate matches.
 */
class GlassCursor {
	/// Copying not allowed
	GlassCursor(const GlassCursor &);
	/// Assignment not allowed
	GlassCursor & operator=(const GlassCursor &);
	/** Rebuild the cursor.
	 *
	 *  Called when the table has changed.
	 */
	void rebuild();
protected:
	/** Whether the cursor is positioned at a valid entry.
	 *
	 *  false initially, and after the cursor has dropped
	 *  off either end of the list of items
	 */
	bool is_positioned;
	bool is_after_end; /** Whether the cursor is off the end of the table. */
private:
	/** Status of the current_tag member. */
	enum { UNREAD, UNREAD_ON_LAST_CHUNK, UNCOMPRESSED, COMPRESSED } tag_status;
protected:
	const GlassTable * B; /// The Btree table
private:
	Glass::Cursor * C; /// Pointer to an array of Cursors
	unsigned long version;
	int level; /** The value of level in the Btree structure. */
	/** Get the key.
	 *
	 *  The key of the item at the cursor is copied into key.
	 *
	 *  The cursor must be positioned before calling this method.
	 *
	 *  e.g.
	 *
	 *    GlassCursor BC(&btree);
	 *    string key;
	 *
	 *    // Now do something to each key in the Btree
	 *    BC.rewind();
	 *    while (BC.next()) {
	 *        BC.get_key(&key);
	 *        do_something_to(key);
	 *    }
	 */
	void get_key(string * key) const;
public:
	/** Create a cursor attached to a Btree.
	 *
	 *  Creates a cursor, which can be used to remember a position inside
	 *  the B-tree. The position is simply the item (key and tag) to which
	 *  the cursor points. A cursor is either positioned or unpositioned,
	 *  and is initially unpositioned.
	 *
	 *  NB: You must not try to use a GlassCursor after the Btree it is
	 *  attached to is destroyed.  It's safe to destroy the GlassCursor
	 *  after the Btree though, you just may not use the GlassCursor.
	 */
	explicit GlassCursor(const GlassTable * B,
	    const Glass::Cursor * C_ = NULL);

	/** Clone a cursor.
	 *
	 *  Creates a new cursor, primed with the blocks in the first cursor.
	 *  The new cursor is initially *unpositioned*.
	 */
	GlassCursor * clone() const { return new GlassCursor(B, C); }
	/** Destroy the GlassCursor */
	~GlassCursor();
	string current_key; /** Current key pointed to by cursor. */
	string current_tag; /** Current tag pointed to by cursor.  You must call read_tag to make this value available. */
	/** Position cursor on the dummy empty key.
	 *
	 *  Calling next() after this moves the cursor to the first entry.
	 */
	void rewind() 
	{
		(void)find_entry_ge(string());
	}
	/** Read the tag from the table and store it in current_tag.
	 *
	 *  Some cursor users don't need the tag, so for efficiency we
	 *  only read it when asked to.
	 *
	 *  @param keep_compressed  Don't uncompress the tag - e.g. useful
	 *			    if it's just being opaquely copied
	 *			    (default: false).
	 *
	 *  @return	true if current_tag holds compressed data (always
	 *		false if keep_compressed was false).
	 */
	bool read_tag(bool keep_compressed = false);
	/** Advance to the next key.
	 *
	 *  If cursor is unpositioned, the result is simply false.
	 *
	 *  If cursor is positioned, and points to the very last item in the
	 *  Btree the cursor is made unpositioned, and the result is false.
	 *  Otherwise the cursor is moved to the next item in the B-tree,
	 *  and the result is true.
	 *
	 *  Effectively, GlassCursor::next() loses the position of BC when it
	 *  drops off the end of the list of items. If this is awkward, one can
	 *  always arrange for a key to be present which has a rightmost
	 *  position in a set of keys,
	 */
	bool next();

	/** Position the cursor on the highest entry with key <= @a key.
	 *
	 *  If the exact key is found in the table, the cursor will be
	 *  set to point to it, and the method will return true.
	 *
	 *  If the key is not found, the cursor will be set to point to
	 *  the key preceding that asked for, and the method will return
	 *  false.  If there is no key preceding that asked for, the cursor
	 *  will point to a null key.
	 *
	 *  Note:  Since the B-tree always contains a null key, which precedes
	 *  everything, a call to GlassCursor::find_entry always results in a
	 *  valid key being pointed to by the cursor.
	 *
	 *  Note: Calling this method with a null key, then calling next()
	 *  will leave the cursor pointing to the first key.
	 *
	 *  @param key    The key to look for in the table.
	 *
	 *  @return true if the exact key was found in the table, false
	 *          otherwise.
	 */
	bool find_entry(const string &key);

	/** Position the cursor exactly on a key.
	 *
	 *  If key is found, the cursor will be set to it, the tag read.  If
	 *  it is not found, the cursor is left unpositioned.
	 *
	 *  @param key	The key to search for.
	 *  @return true if the key was found.
	 */
	bool find_exact(const string &key);

	/// Position the cursor on the highest entry with key < @a key.
	void find_entry_lt(const string &key);

	/** Position the cursor on the lowest entry with key >= @a key.
	 *
	 *  @return true if the exact key was found in the table, false
	 *          otherwise.
	 */
	bool find_entry_ge(const string &key);

	/** Set the cursor to be off the end of the table.
	 */
	void to_end() {
		is_after_end = true;
	}

	/** Determine whether cursor is off the end of table.
	 *
	 *  @return true if the cursor has been moved off the end of the
	 *          table, past the last entry in it, and false otherwise.
	 */
	bool after_end() const {
		return is_after_end;
	}

	/// Return a pointer to the GlassTable we're a cursor for.
	const GlassTable * get_table() const {
		return B;
	}
};

class MutableGlassCursor : public GlassCursor {
public:
	/** Create a mutable cursor attached to a Btree.
	 *
	 *  A mutable cursor allows the items to be deleted.
	 *
	 *  Creates a cursor, which can be used to remember a position inside
	 *  the B-tree. The position is simply the item (key and tag) to which
	 *  the cursor points. A cursor is either positioned or unpositioned,
	 *  and is initially unpositioned.
	 *
	 *  NB: You must not try to use a MutableGlassCursor after the Btree it is
	 *  attached to is destroyed.  It's safe to destroy the MutableGlassCursor
	 *  after the Btree though, you just may not use the MutableGlassCursor.
	 */
	explicit MutableGlassCursor(GlassTable * B_) : GlassCursor(B_) 
	{
	}
	/** Delete the current key/tag pair, leaving the cursor on the next
	 *  entry.
	 *
	 *  @return false if the cursor ends up unpositioned.
	 */
	bool del();
};

#ifdef DISABLE_GPL_LIBXAPIAN
#error GPL source we cannot relicense included in libxapian
#endif

#endif /* XAPIAN_INCLUDED_GLASS_CURSOR_H */
