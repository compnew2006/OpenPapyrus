/** @file
 * @brief Return document ids from an external source.
 */
// Copyright 2008,2009,2010,2011,2019 Olly Betts
// Copyright 2009 Lemur Consulting Ltd
// @license GNU GPL
//
#include <xapian-internal.h>
#pragma hdrstop
#include "externalpostlist.h"

using namespace std;

ExternalPostList::ExternalPostList(const Xapian::Database& db, Xapian::PostingSource* source_, double factor_,
    bool* max_weight_cached_flag_ptr, Xapian::doccount shard_index) : current(0), factor(factor_)
{
	Assert(source_);
	Xapian::PostingSource* newsource = source_->clone();
	if(newsource != NULL) {
		source = newsource->release();
	}
	else if(shard_index == 0) {
		// Allow use of a non-clone-able PostingSource with a non-sharded Database.
		source = source_;
	}
	else {
		throw Xapian::InvalidOperationError("PostingSource subclass must implement clone() to support use with a sharded database");
	}
	source->set_max_weight_cached_flag_ptr_(max_weight_cached_flag_ptr);
	source->reset(db, shard_index);
}

Xapian::doccount ExternalPostList::get_termfreq_min() const
{
	Assert(source.get());
	return source->get_termfreq_min();
}

Xapian::doccount ExternalPostList::get_termfreq_est() const
{
	Assert(source.get());
	return source->get_termfreq_est();
}

Xapian::doccount ExternalPostList::get_termfreq_max() const
{
	Assert(source.get());
	return source->get_termfreq_max();
}

Xapian::docid ExternalPostList::get_docid() const
{
	LOGCALL(MATCH, Xapian::docid, "ExternalPostList::get_docid", NO_ARGS);
	Assert(current);
	RETURN(current);
}

double ExternalPostList::get_weight(Xapian::termcount, Xapian::termcount, Xapian::termcount) const
{
	LOGCALL(MATCH, double, "ExternalPostList::get_weight", NO_ARGS);
	Assert(source.get());
	if(factor == 0.0) RETURN(factor);
	RETURN(factor * source->get_weight());
}

double ExternalPostList::recalc_maxweight()
{
	LOGCALL(MATCH, double, "ExternalPostList::recalc_maxweight", NO_ARGS);
	// source will be NULL here if we've reached the end.
	if(source.get() == NULL) RETURN(0.0);
	if(factor == 0.0) RETURN(0.0);
	RETURN(factor * source->get_maxweight());
}

PositionList * ExternalPostList::read_position_list()
{
	return NULL;
}

PostList * ExternalPostList::update_after_advance() 
{
	LOGCALL(MATCH, PostList *, "ExternalPostList::update_after_advance", NO_ARGS);
	Assert(source.get());
	if(source->at_end()) {
		LOGLINE(MATCH, "ExternalPostList now at end");
		source = NULL;
	}
	else {
		current = source->get_docid();
	}
	RETURN(NULL);
}

PostList * ExternalPostList::next(double w_min)
{
	LOGCALL(MATCH, PostList *, "ExternalPostList::next", w_min);
	Assert(source.get());
	source->next(w_min);
	RETURN(update_after_advance());
}

PostList * ExternalPostList::skip_to(Xapian::docid did, double w_min)
{
	LOGCALL(MATCH, PostList *, "ExternalPostList::skip_to", did | w_min);
	Assert(source.get());
	if(did <= current) RETURN(NULL);
	source->skip_to(did, w_min);
	RETURN(update_after_advance());
}

PostList * ExternalPostList::check(Xapian::docid did, double w_min, bool &valid)
{
	LOGCALL(MATCH, PostList *, "ExternalPostList::check", did | w_min | valid);
	Assert(source.get());
	if(did <= current) {
		valid = true;
		RETURN(NULL);
	}
	valid = source->check(did, w_min);
	if(source->at_end()) {
		LOGLINE(MATCH, "ExternalPostList now at end");
		source = NULL;
	}
	else {
		current = valid ? source->get_docid() : current;
	}
	RETURN(NULL);
}

bool ExternalPostList::at_end() const
{
	LOGCALL(MATCH, bool, "ExternalPostList::at_end", NO_ARGS);
	RETURN(source.get() == NULL);
}

Xapian::termcount ExternalPostList::count_matching_subqs() const
{
	return 1;
}

string ExternalPostList::get_description() const
{
	string desc = "ExternalPostList(";
	if(source.get()) 
		desc += source->get_description();
	desc += ")";
	return desc;
}
