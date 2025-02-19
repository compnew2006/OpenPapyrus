/** @file
 * @brief Iterate all document ids when they form a contiguous range.
 */
/* Copyright (C) 2007,2008,2009,2011,2017 Olly Betts
 * Copyright (C) 2009 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef XAPIAN_INCLUDED_CONTIGUOUSALLDOCSPOSTLIST_H
#define XAPIAN_INCLUDED_CONTIGUOUSALLDOCSPOSTLIST_H

//#include <string>
#include "leafpostlist.h"

/// A PostList iterating all docids when they form a contiguous range.
class ContiguousAllDocsPostList : public LeafPostList {
	/// Don't allow assignment.
	void operator=(const ContiguousAllDocsPostList &);
	/// Don't allow copying.
	ContiguousAllDocsPostList(const ContiguousAllDocsPostList &);
	/** The current document id.
	 *
	 *  This will be 0 before we start and once we reach the end.
	 */
	Xapian::docid did;
	Xapian::doccount doccount; /// The number of documents in the database.
public:
	/// Constructor.
	explicit ContiguousAllDocsPostList(Xapian::doccount doccount_) : LeafPostList(std::string()), did(0), doccount(doccount_) 
	{
	}
	/** Return the term frequency.
	 *
	 *  For an all documents postlist, this is the number of documents in the
	 *  database.
	 */
	Xapian::doccount get_termfreq() const;
	/// Return the current docid.
	Xapian::docid get_docid() const;
	/// Always return 1 (wdf isn't totally meaningful for us).
	Xapian::termcount get_wdf() const;
	/// Throws InvalidOperationError.
	PositionList * read_position_list();
	/// Throws InvalidOperationError.
	PositionList * open_position_list() const;
	/// Advance to the next document.
	PostList * next(double w_min);
	/// Skip ahead to next document with docid >= target.
	PostList * skip_to(Xapian::docid target, double w_min);
	/// Return true if and only if we're off the end of the list.
	bool at_end() const;
	/// Always return 1 (wdf isn't totally meaningful for us).
	Xapian::termcount get_wdf_upper_bound() const;
	/// Return a string description of this object.
	std::string get_description() const;
};

#endif // XAPIAN_INCLUDED_CONTIGUOUSALLDOCSPOSTLIST_H
