/*
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
#ifndef _PLAINTORICH_H_INCLUDED_
#define _PLAINTORICH_H_INCLUDED_
/* @(#$Id: plaintorich.h,v 1.10.2.1 2006-11-27 19:04:16 dockes Exp $  (C) 2004 J.F.Dockes */

#include <string>

#include "searchdata.h"

/**
 * Transform plain text into qt rich text for the preview window.
 *
 * We escape characters like < or &, and add qt rich text tags to 
 * colorize the query terms. The latter is a quite complicated matter because
 * of phrase/near searches. We treat all such searches as "near", not "phrase"
 * 
 * @param in          raw text out of internfile.
 * @param out         rich text output
 * @param terms       list of query terms. These are out of Rcl::Db and dumb
 * @param firstTerm   out: value of the first search term in text.
 * @param frsttocc    out: occurrence of 1st term to look for
 * @param noHeader    if true don't output header (<qt><title>...)
 */
extern bool plaintorich(const string &in, string &out,
			RefCntr<Rcl::SearchData> sdata,
			bool noHeader = false,
			bool fft = false);

extern const char *firstTermAnchorName;

#define QT_SCROLL_TO_ANCHOR_BUG
#ifdef QT_SCROLL_TO_ANCHOR_BUG
// For some reason, can't get scrollToAnchor() to work. We use a string made
// of a few rare utf8 chars as a beacon for the match area.
extern const char *firstTermBeacon;
#endif

#endif /* _PLAINTORICH_H_INCLUDED_ */
