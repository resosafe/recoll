/* Copyright (C) 2005 J.F.Dockes
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
#ifndef _ADVSHIST_H_INCLUDED_
#define _ADVSHIST_H_INCLUDED_

#include <vector>

#include "recoll.h"
#include "refcntr.h"
#include "searchdata.h"

class AdvSearchHist {
public:
    AdvSearchHist();
    ~AdvSearchHist();
    RefCntr<Rcl::SearchData> getolder();
    RefCntr<Rcl::SearchData> getnewer();
    bool push(RefCntr<Rcl::SearchData>);
    void clear();

private:
    bool read();

    int m_current;
    std::vector<RefCntr<Rcl::SearchData> > m_entries;
};


#endif // _ADVSHIST_H_INCLUDED_
