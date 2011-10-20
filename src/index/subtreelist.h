/* Copyright (C) 2007 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _SUBTREELIST_H_INCLUDED_
#define _SUBTREELIST_H_INCLUDED_

#include <list>
using std::list;

class RclConfig;

// This queries the database with a pure directory-filter query, to
// retrieve all the entries beside the specified path. This is used by
// the real time indexer to purge entries when a top directory is
// renamed. This is really convoluted, I'd like a better way.
extern bool subtreelist(RclConfig *config, const string& top, 
			list<string>& paths); 

#endif /* _SUBTREELIST_H_INCLUDED_ */