/** @file
 *  @brief SubMatch class for a local database.
 */
/* Copyright (C) 2006,2007,2009,2010,2011,2013,2014,2015,2016,2017,2018,2019 Olly Betts
 * Copyright (C) 2007 Lemur Consulting Ltd
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

#ifndef XAPIAN_INCLUDED_LOCALSUBMATCH_H
#define XAPIAN_INCLUDED_LOCALSUBMATCH_H

#include "api/queryinternal.h"
#include "backends/databaseinternal.h"
#include "weight/weightinternal.h"
#include "xapian/enquire.h"
#include "xapian/weight.h"
#include <map>

class PostListTree;

namespace Xapian {
	namespace Internal {
		class PostList;
	}
}

using Xapian::Internal::PostList;

class LocalSubMatch {
	/// Don't allow assignment.
	LocalSubMatch& operator=(const LocalSubMatch &) = delete;
	/// Don't allow copying.
	LocalSubMatch(const LocalSubMatch &) = delete;
	
	Xapian::Weight::Internal* total_stats; /// The statistics for the collection.
	Xapian::Query query; /// The original query before any rearrangement.
	Xapian::termcount qlen; /// The query length (used by some weighting schemes).
	const Xapian::Database::Internal* db; /// The (sub-)Database we're searching.
	const Xapian::Weight& wt_factory; /// Weight object (used as a factory by calling create on it).
	Xapian::doccount shard_index; /// 0-based index for the subdatabase.
public:
	/// Constructor.
	LocalSubMatch(const Xapian::Database::Internal* db_, const Xapian::Query& query_, Xapian::termcount qlen_,
	    const Xapian::Weight& wt_factory_, Xapian::doccount shard_index_) : total_stats(NULL), query(query_), qlen(qlen_), db(db_),
		wt_factory(wt_factory_), shard_index(shard_index_)
	{
	}
	/** Fetch and collate statistics.
	 *
	 *  Before we can calculate term weights we need to fetch statistics from
	 *  each database involved and collate them.
	 *
	 *  @param rset	The RSet for this shard.
	 *  @param stats	Weight::Internal object to add the statistics to.
	 */
	void prepare_match(const Xapian::RSet& rset, Xapian::Weight::Internal& stats)
	{
		stats.accumulate_stats(*db, rset);
	}
	/** Set the collated statistics.
	 *
	 *  These will be used when generating the PostList tree.
	 */
	void start_match(Xapian::Weight::Internal& total_stats_)
	{
		total_stats = &total_stats_;
	}
	/// Get PostList.
	PostList * get_postlist(PostListTree* matcher, Xapian::termcount* total_subqs_ptr);
	/** Convert a postlist into a synonym postlist.
	 */
	PostList * make_synonym_postlist(PostListTree* pltree, PostList* or_pl, Xapian::Internal::QueryOptimiser* qopt, double factor, bool wdf_disjoint);
	PostList * open_post_list(const std::string & term, Xapian::termcount wqf, double factor,
	    bool need_positions, bool compound_weight, Xapian::Internal::QueryOptimiser* qopt, bool lazy_weight);
	bool weight_needs_wdf() const { return wt_factory.get_sumpart_needs_wdf_(); }
};

#endif /* XAPIAN_INCLUDED_LOCALSUBMATCH_H */
