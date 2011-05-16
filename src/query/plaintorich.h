/* Copyright (C) 2004 J.F.Dockes
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

#include <string>
#include <list>
using std::list;
using std::string;

/// Holder for plaintorich() input data: words and groups of words to
/// be highlighted
struct HiliteData {
    // Single terms
    vector<string> terms;
    // NEAR and PHRASE elements
    vector<vector<string> > groups;
    // Group slacks (number of permitted non-matched words). 
    // Parallel vector to the above 'groups'
    vector<int> gslks; 
    void reset() 
    {
	terms.clear();
	groups.clear();
	gslks.clear();
    }
};

/** 
 * A class for highlighting search results. Overridable methods allow
 * for different styles. We can handle plain text or html input. In the latter
 * case, we may fail to highligt term groups if they are mixed with html tags.
 */
class PlainToRich {
public:
    PlainToRich() : m_inputhtml(false) {}
    virtual ~PlainToRich() {}
    void set_inputhtml(bool v) {m_inputhtml = v;}

    /**
     * Transform plain text for highlighting search terms, ie in the
     * preview window or result list entries.
     *
     * The actual tags used for highlighting and anchoring are
     * determined by deriving from this class which handles the searching for
     * terms and groups, but there is an assumption that the output will be
     * html-like: we escape characters like < or &
     * 
     * Finding the search terms is relatively complicated because of
     * phrase/near searches, which need group highlights. As a matter
     * of simplification, we handle "phrase" as "near", not filtering
     * on word order.
     *
     * @param in    raw text out of internfile.
     * @param out   rich text output, divided in chunks (to help our caller
     *   avoid inserting half tags into textedit which doesnt like it)
     * @param hdata terms and groups to be highlighted. These are
     *   lowercase and unaccented.
     * @param chunksize max size of chunks in output list
     */
    virtual bool plaintorich(const string &in, list<string> &out,
			     const HiliteData& hdata,
			     int chunksize = 50000
			     );

    /* Methods to ouput headers, highlighting and marking tags */
    virtual string header() {return snull;}
    virtual string startMatch() {return snull;}
    virtual string endMatch() {return snull;}
    virtual string startAnchor(int) {return snull;}
    virtual string endAnchor() {return snull;}
    virtual string startChunk() {return snull;}

protected:
    const string snull;
    bool m_inputhtml;
    // Use <br> to break plain text lines (else caller has used a <pre> tag)
    bool m_eolbr; 
};

#endif /* _PLAINTORICH_H_INCLUDED_ */
