/** @file
 * @brief N-way OR postlist with wt=max(wt_i)
 */
/* Copyright (C) 2007,2009,2010,2011,2012,2013,2014,2017 Olly Betts
 * Copyright (C) 2009 Lemur Consulting Ltd
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */
#include <xapian-internal.h>
#pragma hdrstop
#include "maxpostlist.h"

using namespace std;

MaxPostList::~MaxPostList()
{
	if(plist) {
		for(size_t i = 0; i < n_kids; ++i) {
			delete plist[i];
		}
		delete [] plist;
	}
}

Xapian::doccount MaxPostList::get_termfreq_min() const
{
	Xapian::doccount res = plist[0]->get_termfreq_min();
	for(size_t i = 1; i < n_kids; ++i) {
		res = max(res, plist[i]->get_termfreq_min());
	}
	return res;
}

Xapian::doccount MaxPostList::get_termfreq_max() const
{
	Xapian::doccount res = plist[0]->get_termfreq_max();
	for(size_t i = 1; i < n_kids; ++i) {
		Xapian::doccount c = plist[i]->get_termfreq_max();
		if(db_size - res <= c)
			return db_size;
		res += c;
	}
	return res;
}

Xapian::doccount MaxPostList::get_termfreq_est() const
{
	if(UNLIKELY(db_size == 0))
		return 0;

	// We calculate the estimate assuming independence.  The simplest
	// way to calculate this seems to be a series of (n_kids - 1) pairwise
	// calculations, which gives the same answer regardless of the order.
	double scale = 1.0 / db_size;
	double P_est = plist[0]->get_termfreq_est() * scale;
	for(size_t i = 1; i < n_kids; ++i) {
		double P_i = plist[i]->get_termfreq_est() * scale;
		P_est += P_i - P_est * P_i;
	}
	return static_cast<Xapian::doccount>(P_est * db_size + 0.5);
}

TermFreqs MaxPostList::get_termfreq_est_using_stats(const Xapian::Weight::Internal & stats) const
{
	// We calculate the estimate assuming independence.  The simplest
	// way to calculate this seems to be a series of (n_kids - 1) pairwise
	// calculations, which gives the same answer regardless of the order.
	TermFreqs freqs(plist[0]->get_termfreq_est_using_stats(stats));

	// Our caller should have ensured this.
	Assert(stats.collection_size);
	double scale = 1.0 / stats.collection_size;
	double P_est = freqs.termfreq * scale;
	double rtf_scale = 0.0;
	if(stats.rset_size != 0) {
		rtf_scale = 1.0 / stats.rset_size;
	}
	double Pr_est = freqs.reltermfreq * rtf_scale;
	// If total_length is 0, cf must always be 0 so cf_scale is irrelevant.
	double cf_scale = 0.0;
	if(LIKELY(stats.total_length != 0)) {
		cf_scale = 1.0 / stats.total_length;
	}
	double Pc_est = freqs.collfreq * cf_scale;

	for(size_t i = 1; i < n_kids; ++i) {
		freqs = plist[i]->get_termfreq_est_using_stats(stats);
		double P_i = freqs.termfreq * scale;
		P_est += P_i - P_est * P_i;
		double Pc_i = freqs.collfreq * cf_scale;
		Pc_est += Pc_i - Pc_est * Pc_i;
		// If the rset is empty, Pr_est should be 0 already, so leave
		// it alone.
		if(stats.rset_size != 0) {
			double Pr_i = freqs.reltermfreq * rtf_scale;
			Pr_est += Pr_i - Pr_est * Pr_i;
		}
	}
	return TermFreqs(Xapian::doccount(P_est * stats.collection_size + 0.5),
		   Xapian::doccount(Pr_est * stats.rset_size + 0.5),
		   Xapian::termcount(Pc_est * stats.total_length + 0.5));
}

Xapian::docid MaxPostList::get_docid() const
{
	return did;
}

double MaxPostList::get_weight(Xapian::termcount doclen,
    Xapian::termcount unique_terms,
    Xapian::termcount wdfdocmax) const
{
	Assert(did);
	double res = 0.0;
	for(size_t i = 0; i < n_kids; ++i) {
		if(plist[i]->get_docid() == did)
			res = max(res, plist[i]->get_weight(doclen,
				unique_terms,
				wdfdocmax));
	}
	return res;
}

bool MaxPostList::at_end() const
{
	return (did == 0);
}

double MaxPostList::recalc_maxweight()
{
	double result = plist[0]->recalc_maxweight();
	for(size_t i = 1; i < n_kids; ++i) {
		result = max(result, plist[i]->recalc_maxweight());
	}
	return result;
}

PostList * MaxPostList::next(double w_min)
{
	Xapian::docid old_did = did;
	did = 0;
	for(size_t i = 0; i < n_kids; ++i) {
		Xapian::docid cur_did = 0;
		if(old_did != 0)
			cur_did = plist[i]->get_docid();
		if(cur_did <= old_did) {
			PostList * res;
			if(old_did == 0 || cur_did == old_did) {
				res = plist[i]->next(w_min);
			}
			else {
				res = plist[i]->skip_to(old_did + 1, w_min);
			}
			if(res) {
				delete plist[i];
				plist[i] = res;
			}

			if(plist[i]->at_end()) {
				erase_sublist(i--);
				continue;
			}

			if(res)
				matcher->force_recalc();

			cur_did = plist[i]->get_docid();
		}

		if(did == 0 || cur_did < did) {
			did = cur_did;
		}
	}

	if(n_kids == 1) {
		n_kids = 0;
		return plist[0];
	}

	return NULL;
}

PostList * MaxPostList::skip_to(Xapian::docid did_min, double w_min)
{
	Xapian::docid old_did = did;
	did = 0;
	for(size_t i = 0; i < n_kids; ++i) {
		Xapian::docid cur_did = 0;
		if(old_did != 0)
			cur_did = plist[i]->get_docid();
		if(cur_did < did_min) {
			PostList * res = plist[i]->skip_to(did_min, w_min);
			if(res) {
				delete plist[i];
				plist[i] = res;
			}

			if(plist[i]->at_end()) {
				erase_sublist(i--);
				continue;
			}

			if(res)
				matcher->force_recalc();

			cur_did = plist[i]->get_docid();
		}

		if(did == 0 || cur_did < did) {
			did = cur_did;
		}
	}

	if(n_kids == 1) {
		n_kids = 0;
		return plist[0];
	}

	return NULL;
}

string MaxPostList::get_description() const
{
	string desc("(");
	desc += plist[0]->get_description();
	for(size_t i = 1; i < n_kids; ++i) {
		desc += " MAX ";
		desc += plist[i]->get_description();
	}
	desc += ')';
	return desc;
}

Xapian::termcount MaxPostList::get_wdf() const
{
	Xapian::termcount totwdf = 0;
	for(size_t i = 0; i < n_kids; ++i) {
		if(plist[i]->get_docid() == did)
			totwdf += plist[i]->get_wdf();
	}
	return totwdf;
}

Xapian::termcount MaxPostList::count_matching_subqs() const
{
	return 1;
}
