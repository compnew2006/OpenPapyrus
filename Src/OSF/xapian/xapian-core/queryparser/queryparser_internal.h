/** @file
 * @brief The non-lemon-generated parts of the QueryParser class.
 */
/* Copyright (C) 2005,2006,2007,2010,2011,2012,2013,2015,2016,2018,2019 Olly Betts
 * Copyright (C) 2010 Adam Sjøgren
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
#ifndef XAPIAN_INCLUDED_QUERYPARSER_INTERNAL_H
#define XAPIAN_INCLUDED_QUERYPARSER_INTERNAL_H

#include <xapian/intrusive_ptr.h>
#include <xapian/database.h>
#include <xapian/query.h>
#include <xapian/queryparser.h>
#include <xapian/stem.h>
//#include <list>
//#include <map>

class State;

typedef enum { 
    NON_BOOLEAN, 
	BOOLEAN_,
	BOOLEAN_EXCLUSIVE 
} filter_type;

/** Information about how to handle a field prefix in the query string. */
struct FieldInfo {
    /// The type of this field.
    filter_type type;

    std::string grouping;

    /// Field prefix strings.
    std::vector<std::string> prefixes;

    /// Field processor.  Currently only one is supported.
    Xapian::Internal::opt_intrusive_ptr<Xapian::FieldProcessor> proc;

    FieldInfo(filter_type type_, const std::string & prefix,
	      const std::string & grouping_ = std::string())
	: type(type_), grouping(grouping_)
    {
	prefixes.push_back(prefix);
    }

    FieldInfo(filter_type type_, Xapian::FieldProcessor* proc_,
	      const std::string & grouping_ = std::string())
	: type(type_), grouping(grouping_), proc(proc_)
    {
    }
};

namespace Xapian {

class Utf8Iterator;

struct RangeProc {
    Xapian::Internal::opt_intrusive_ptr<RangeProcessor> proc;
    std::string grouping;
    bool default_grouping;

    RangeProc(RangeProcessor * range_proc, const std::string* grouping_)
	: proc(range_proc),
	  grouping(grouping_ ? *grouping_ : std::string()),
	  default_grouping(grouping_ == NULL) { }
};

class QueryParser::Internal : public Xapian::Internal::intrusive_base {
    friend class QueryParser;
    friend class ::State;
    Stem stemmer;
    stem_strategy stem_action;
    Xapian::Internal::opt_intrusive_ptr<const Stopper> stopper;
    Query::op default_op;
    const char * errmsg;
    Database db;
    std::list<std::string> stoplist;
    std::multimap<std::string, std::string> unstem;

    // Map "from" -> "A" ; "subject" -> "C" ; "newsgroups" -> "G" ;
    // "foobar" -> "XFOO". FIXME: it does more than this now!
    std::map<std::string, FieldInfo> field_map;

    std::list<RangeProc> rangeprocs;

    std::string corrected_query;

    Xapian::termcount max_wildcard_expansion = 0;

    Xapian::termcount max_partial_expansion = 100;

    Xapian::termcount max_fuzzy_expansion = 0;

    int max_wildcard_type = Xapian::Query::WILDCARD_LIMIT_ERROR;

    int max_partial_type = Xapian::Query::WILDCARD_LIMIT_MOST_FREQUENT;

    int max_fuzzy_type = Xapian::Query::WILDCARD_LIMIT_ERROR;

    unsigned min_wildcard_prefix_len = 0;

    unsigned min_partial_prefix_len = 2;

    void add_prefix(const std::string & field, const std::string & prefix);

    void add_prefix(const std::string & field, Xapian::FieldProcessor* proc);

    void add_boolean_prefix(const std::string & field,
			    const std::string & prefix,
			    const std::string* grouping);

    void add_boolean_prefix(const std::string & field,
			    Xapian::FieldProcessor* proc,
			    const std::string* grouping);

    std::string parse_term(Utf8Iterator& it, const Utf8Iterator& end,
			   bool cjk_enable, unsigned flags,
			   bool& is_cjk_term, bool& was_acronym,
			   size_t& first_wildcard,
			   size_t& char_count,
			   unsigned& edit_distance);

  public:
    Internal() : stem_action(STEM_SOME), stopper(NULL),
	default_op(Query::OP_OR), errmsg(NULL) { }

    Query parse_query(const std::string & query_string,
		      uint flags,
		      const std::string & default_prefix);
};

}

#endif // XAPIAN_INCLUDED_QUERYPARSER_INTERNAL_H
