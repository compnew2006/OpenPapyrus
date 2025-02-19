/** @file
 *  @brief Public interfaces for the Xapian library.
 */
// Copyright (C) 2003,2004,2005,2007,2008,2009,2010,2012,2013,2015,2016,2019 Olly Betts
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

#ifndef XAPIAN_INCLUDED_XAPIAN_H
#define XAPIAN_INCLUDED_XAPIAN_H

#ifdef slots
#ifdef Q_OBJECT
// Qt headers '#define slots' by default, which clashes with us using it as a
// class member name.  Including <xapian.h> first is a simple workaround, or
// you can use 'no_keywords' to stop Qt polluting the global macro namespace,
// as described here:
//
// https://doc.qt.io/qt-5/signalsandslots.html#using-qt-with-3rd-party-signals-and-slots
#  error Include <xapian.h> before Qt headers, or put 'CONFIG += no_keywords' in your .pro file and use Q_SLOTS instead of slots, etc
# endif
#ifdef WT_API
// Argh, copycat polluters!
#  error Include <xapian.h> before Wt headers, or define WT_NO_SLOT_MACROS to stop Wt from defining the macros 'slots' and 'SLOT()'
# endif
#endif

// Define so that deprecation warnings are given to API users, but not
// while building the library.
#define XAPIAN_IN_XAPIAN_H

#include <xapian/version.h> // Set defines for library version and check C++ ABI versions match.
#include <xapian/types.h> // Types
#include <xapian/attributes.h> // Function attributes
#include <xapian/constants.h> // Constants
#include <xapian/error.h> // Exceptions
#include <xapian/intrusive_ptr.h>
#include <xapian/derefwrapper.h>
// Access to databases, documents, etc.
#include <xapian/database.h>
#include <xapian/dbfactory.h>
#include <xapian/document.h>
#include <xapian/positioniterator.h>
#include <xapian/postingiterator.h>
#include <xapian/termiterator.h>
#include <xapian/valueiterator.h>
#include <xapian/termgenerator.h> // Indexing
#include <xapian/visibility.h> // ELF visibility annotations for GCC.
#include <xapian/constinfo.h> // Mechanism for accessing a struct of constant information
// Searching
#include <xapian/stem.h> // Stemming
#include <xapian/eset.h>
#include <xapian/mset.h>
#include <xapian/enquire.h>
#include <xapian/expanddecider.h>
#include <xapian/keymaker.h>
#include <xapian/matchdecider.h>
#include <xapian/matchspy.h>
#include <xapian/postingsource.h>
#include <xapian/query.h>
#include <xapian/queryparser.h>
#include <xapian/rset.h>
#include <xapian/valuesetmatchdecider.h>
#include <xapian/weight.h>
#include <xapian/cluster.h> // Clustering
#include <xapian/diversify.h> // Diversification
#include <xapian/registry.h> // Subclass registry
#include <xapian/unicode.h> // Unicode support
#include <xapian/geospatial.h> // Geospatial
#include <xapian/compactor.h> // Database compaction and merging

/// The Xapian namespace contains public interfaces for the Xapian library.
namespace Xapian {
	// Functions returning library version:

	/** Report the version string of the library which the program is linked with.
	 *
	 * This may be different to the version compiled against (given by
	 * XAPIAN_VERSION) if shared libraries are being used.
	 */
	inline const char* version_string() { return Internal::get_constinfo_()->str; }

	/** Report the major version of the library which the program is linked with.
	 *
	 * This may be different to the version compiled against (given by
	 * XAPIAN_MAJOR_VERSION) if shared libraries are being used.
	 */
	inline int major_version() { return Internal::get_constinfo_()->major; }

	/** Report the minor version of the library which the program is linked with.
	 *
	 * This may be different to the version compiled against (given by
	 * XAPIAN_MINOR_VERSION) if shared libraries are being used.
	 */
	inline int minor_version() { return Internal::get_constinfo_()->minor; }

	/** Report the revision of the library which the program is linked with.
	 *
	 * This may be different to the version compiled against (given by
	 * XAPIAN_REVISION) if shared libraries are being used.
	 */
	inline int revision() { return Internal::get_constinfo_()->revision; }
}

#undef XAPIAN_IN_XAPIAN_H

#endif /* XAPIAN_INCLUDED_XAPIAN_H */
