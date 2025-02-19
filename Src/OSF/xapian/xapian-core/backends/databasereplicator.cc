/** @file
 * @brief Support class for database replication.
 */
/* Copyright 2008 Lemur Consulting Ltd
 * Copyright 2009,2010,2011,2012 Olly Betts
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
#include <xapian-internal.h>
#pragma hdrstop
#include "databasereplicator.h"
#include "filetests.h"
#if defined XAPIAN_HAS_GLASS_BACKEND && defined XAPIAN_HAS_REMOTE_BACKEND
#include "glass/glass_databasereplicator.h"
#endif

using namespace std;

namespace Xapian {
DatabaseReplicator::~DatabaseReplicator()
{
}

DatabaseReplicator * DatabaseReplicator::open(const string & path)
{
	LOGCALL_STATIC(DB, DatabaseReplicator *, "DatabaseReplicator::open", path);

#ifdef XAPIAN_HAS_GLASS_BACKEND
	if(file_exists(path + "/iamglass")) {
#ifdef XAPIAN_HAS_REMOTE_BACKEND
		return new GlassDatabaseReplicator(path);
# else
		throw FeatureUnavailableError("Replication disabled");
#endif
	}
#endif

	// FIXME: Replication of honey databases.

	if(file_exists(path + "/iamchert")) {
		throw FeatureUnavailableError("Chert backend no longer supported");
	}

	if(file_exists(path + "/iamflint")) {
		throw FeatureUnavailableError("Flint backend no longer supported");
	}

	if(file_exists(path + "/iambrass")) {
		throw FeatureUnavailableError("Brass backend no longer supported");
	}

	throw DatabaseOpeningError("Couldn't detect type of database: " + path);
}
}
