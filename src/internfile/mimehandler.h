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
#ifndef _MIMEHANDLER_H_INCLUDED_
#define _MIMEHANDLER_H_INCLUDED_
/* @(#$Id: mimehandler.h,v 1.16 2008-10-04 14:26:59 dockes Exp $  (C) 2004 J.F.Dockes */
#include "autoconfig.h"

#include <string>
#include <list>
using std::string;
using std::list;

#include <Filter.h>

class RclConfig;

class RecollFilter : public Dijon::Filter {
public:
    RecollFilter(const string& mtype)
	: Dijon::Filter(mtype), m_forPreview(false), m_havedoc(false)
    {}
    virtual ~RecollFilter() {}
    virtual bool set_property(Properties p, const string &v) {
	switch (p) {
	case DEFAULT_CHARSET: 
	    m_defcharset = v;
	    break;
	case OPERATING_MODE: 
	    if (!v.empty() && v[0] == 'v') 
		m_forPreview = true; 
	    else 
		m_forPreview = false;
	    break;
	}
	return true;
    }

    // We don't use this for now
    virtual bool set_document_uri(const std::string &) {return false;}

    /// This does nothing right now but should be called from the
    /// subclass method in case we need some common processing one day
    /// (was used for xattrs at some point).
    virtual bool set_document_file(const string &file_path) {return true;}

    // Default implementations
    virtual bool set_document_string(const std::string &) {return false;}
    virtual bool set_document_data(const char *cp, unsigned int sz) {
	return set_document_string(string(cp, sz));
    }

    virtual bool has_documents() const {return m_havedoc;}

    // Most doc types are single-doc
    virtual bool skip_to_document(const string& s) {
	if (s.empty())
	    return true;
	return false;
    }

    virtual bool is_data_input_ok(DataInput input) const {
	if (input == DOCUMENT_FILE_NAME)
	    return true;
	return false;
    }

    virtual string get_error() const {
	return m_reason;
    }

    virtual void clear() {
	Dijon::Filter::clear();
	m_forPreview = m_havedoc = false;
	m_defcharset.clear();
	m_reason.clear();
    }

protected:
    bool   m_forPreview;
    string m_defcharset;
    string m_reason;
    bool   m_havedoc;
};

/**
 * Return indexing handler object for the given mime type
 * returned pointer should be deleted by caller
 * @param mtyp input mime type, ie text/plain
 * @param cfg  the recoll config object to be used
 * @param filtertypes decide if we should restrict to types in 
 *     indexedmimetypes (if this is set at all).
 */
extern Dijon::Filter *getMimeHandler(const std::string &mtyp, RclConfig *cfg,
				     bool filtertypes=false);

/// Free up filter for reuse (you can also delete it)
extern void returnMimeHandler(Dijon::Filter *);

/// Can this mime type be interned ?
extern bool canIntern(const std::string mimetype, RclConfig *cfg);

#endif /* _MIMEHANDLER_H_INCLUDED_ */
