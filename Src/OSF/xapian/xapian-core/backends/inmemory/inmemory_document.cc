/** @file
 * @brief A document read from a InMemoryDatabase.
 */
/* Copyright (C) 2008,2009,2016 Olly Betts
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
#include "inmemory_document.h"
#include "inmemory_database.h"

using namespace std;

string InMemoryDocument::fetch_value(Xapian::valueno slot) const
{
	LOGCALL(DB, string, "InMemoryDocument::fetch_value", slot);
	const InMemoryDatabase * db;
	db = static_cast<const InMemoryDatabase*>(database.get());
	if(UNLIKELY(did > db->valuelists.size()))
		RETURN(string());
	map<Xapian::valueno, string> values_ = db->valuelists[did - 1];
	map<Xapian::valueno, string>::const_iterator i;
	i = values_.find(slot);
	if(i == values_.end())
		RETURN(string());
	RETURN(i->second);
}

void InMemoryDocument::fetch_all_values(map<Xapian::valueno, string> &values_) const
{
	LOGCALL_VOID(DB, "InMemoryDocument::fetch_all_values", values_);
	const InMemoryDatabase * db;
	db = static_cast<const InMemoryDatabase*>(database.get());
	if(db->closed) InMemoryDatabase::throw_database_closed();
	if(UNLIKELY(did > db->valuelists.size())) {
		values_.clear();
		return;
	}
	values_ = db->valuelists[did - 1];
}

string InMemoryDocument::fetch_data() const
{
	LOGCALL(DB, string, "InMemoryDocument::fetch_data", NO_ARGS);
	const InMemoryDatabase * db;
	db = static_cast<const InMemoryDatabase*>(database.get());
	if(db->closed) InMemoryDatabase::throw_database_closed();
	if(UNLIKELY(did > db->valuelists.size()))
		RETURN(string());
	RETURN(db->doclists[did - 1]);
}
