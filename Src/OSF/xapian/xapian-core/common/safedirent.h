/** @file
 *  @brief #include <dirent.h>, with alternative implementation for windows.
 */
/* Copyright (C) 2008 Lemur Consulting Ltd
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA
 */

#ifndef XAPIAN_INCLUDED_SAFEDIRENT_H
#define XAPIAN_INCLUDED_SAFEDIRENT_H

#include <dirent.h> // @sobolev SLIB has own dirent.h
//#ifdef __WIN32__
//#include "msvc_dirent.h"
//#else
//#include <dirent.h>
//#endif

#endif // XAPIAN_INCLUDED_SAFEDIRENT_H
