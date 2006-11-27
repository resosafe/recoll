#ifndef lint
static char rcsid[] = "@(#$Id: plaintorich.cpp,v 1.17.2.2 2006-11-27 19:04:16 dockes Exp $ (C) 2005 J.F.Dockes";
#endif
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


#include <string>
#include <utility>
#include <list>
#include <set>
#include <vector>
#include <map>
#include <algorithm>

#ifndef NO_NAMESPACES
using std::vector;
using std::list;
using std::pair;
using std::set;
#endif /* NO_NAMESPACES */

#include "rcldb.h"
#include "rclconfig.h"
#include "debuglog.h"
#include "textsplit.h"
#include "utf8iter.h"
#include "transcode.h"
#include "smallut.h"
#include "plaintorich.h"
#include "cancelcheck.h"

// For debug printing
static string vecStringToString(const vector<string>& t)
{
    string sterms;
    for (vector<string>::const_iterator it = t.begin(); it != t.end(); it++) {
	sterms += "[" + *it + "] ";
    }
    return sterms;
}

// Text splitter callback used to take note of the position of query terms 
// inside the result text. This is then used to insert highlight tags. 
class myTextSplitCB : public TextSplitCB {
 public:

    // Out: first query term found in text
    string firstTerm;
    int    firstTermOcc;
    int m_firstTermPos;
    int m_firstTermBPos;

    // Out: begin and end byte positions of query terms/groups in text
    vector<pair<int, int> > tboffs;  

    myTextSplitCB(const vector<string>& its, vector<vector<string> >&groups, 
		  vector<int>& slacks) 
	:  firstTermOcc(1), m_wcount(0), m_groups(groups), m_slacks(slacks)
    {
	for (vector<string>::const_iterator it = its.begin(); 
	     it != its.end(); it++) {
	    m_terms.insert(*it);
	}
	for (vector<vector<string> >::const_iterator vit = m_groups.begin(); 
	     vit != m_groups.end(); vit++) {
	    for (vector<string>::const_iterator it = (*vit).begin(); 
		 it != (*vit).end(); it++) {
		m_gterms.insert(*it);
	    }
	}
    }

    // Callback called by the text-to-words breaker for each word
    virtual bool takeword(const std::string& term, int pos, int bts, int bte) {
	string dumb;
	Rcl::dumb_string(term, dumb);
	//LOGDEB2(("Input dumbbed term: '%s' %d %d %d\n", dumb.c_str(), 
	// pos, bts, bte));

	// If this word is a search term, remember its byte-offset span. 
	if (m_terms.find(dumb) != m_terms.end()) {
	    tboffs.push_back(pair<int, int>(bts, bte));
	    if (firstTerm.empty()) {
		firstTerm = term;
		m_firstTermPos = pos;
		m_firstTermBPos = bts;
	    }
	}
	
	if (m_gterms.find(dumb) != m_gterms.end()) {
	    // Term group (phrase/near) handling
	    m_plists[dumb].push_back(pos);
	    m_gpostobytes[pos] = pair<int,int>(bts, bte);
	    //LOGDEB2(("Recorded bpos for %d: %d %d\n", pos, bts, bte));
	}
	if ((m_wcount++ & 0xfff) == 0)
	    CancelCheck::instance().checkCancel();
	return true;
    }

    // Must be called after the split to find the phrase/near match positions
    virtual bool matchGroups();

private:
    virtual bool matchGroup(const vector<string>& terms, int dist);

    int m_wcount;

    // In: user query terms
    set<string>    m_terms; 

    // In: user query groups, for near/phrase searches.
    const vector<vector<string> >& m_groups;
    const vector<int>&             m_slacks;
    set<string>                    m_gterms;

    // group/near terms word positions.
    map<string, vector<int> > m_plists;
    map<int, pair<int, int> > m_gpostobytes;
};


/** Sort by shorter comparison class */
class VecIntCmpShorter {
    public:
	/** Return true if and only if a is strictly shorter than b.
	 */
        bool operator()(const vector<int> *a, const vector<int> *b) {
            return a->size() < b->size();
        }
};

#define SETMINMAX(POS, STA, STO)  {if ((POS) < (STA)) (STA) = (POS); \
	if ((POS) > (STO)) (STO) = (POS);}

// Recursively check that each term is inside the window (which is readjusted
// as the successive terms are found)
static bool do_proximity_test(int window, vector<vector<int>* >& plists, 
		    unsigned int i, int min, int max, int *sp, int *ep)
{
    int tmp = max + 1;
    // take care to avoid underflow
    if (window <= tmp) 
	tmp -= window; 
    else 
	tmp = 0;
    vector<int>::iterator it = plists[i]->begin();

    // Find 1st position bigger than window start
    while (it != plists[i]->end() && *it < tmp)
	it++;

    // Try each position inside window in turn for match with other lists
    while (it != plists[i]->end()) {
	int pos = *it;
	if (pos > min + window - 1) 
	    return false;
	if (i + 1 == plists.size()) {
	    SETMINMAX(pos, *sp, *ep);
	    return true;
	}
	if (pos < min) {
	    min = pos;
	} else if (pos > max) {
	    max = pos;
	}
	if (do_proximity_test(window, plists, i + 1, min, max, sp, ep)) {
	    SETMINMAX(pos, *sp, *ep);
	    return true;
	}
	it++;
    }
    return false;
}

// Check if there is a NEAR match for the group of terms
bool myTextSplitCB::matchGroup(const vector<string>& terms, int window)
{
    LOGDEB0(("myTextSplitCB::matchGroup:d %d: %s\n", window,
	    vecStringToString(terms).c_str()));

    // The position lists we are going to work with. We extract them from the 
    // (string->plist) map
    vector<vector<int>* > plists;
    // A revert plist->term map. This is so that we can find who is who after
    // sorting the plists by length.
    map<vector<int>*, string> plistToTerm;
    // For traces
    vector<string> realgroup;

    // Find the position list for each term in the group. Not all
    // necessarily exist (esp for NEAR where terms have been
    // stem-expanded: we don't know which matched)
    for (vector<string>::const_iterator it = terms.begin(); 
	 it != terms.end(); it++) {
	map<string, vector<int> >::iterator pl = m_plists.find(*it);
	if (pl == m_plists.end()) {
	    LOGDEB1(("myTextSplitCB::matchGroup: [%s] not found in m_plists\n",
		    (*it).c_str()));
	    continue;
	}
	plists.push_back(&(pl->second));
	plistToTerm[&(pl->second)] = *it;
	realgroup.push_back(*it);
    }
    LOGDEB0(("myTextSplitCB::matchGroup:d %d:real group %s\n", window,
	     vecStringToString(realgroup).c_str()));
    if (plists.size() < 2)
	return false;
    // Sort the positions lists so that the shorter is first
    std::sort(plists.begin(), plists.end(), VecIntCmpShorter());

    // Walk the shortest plist and look for matches
    int sta = int(10E9), sto = 0;
    int pos;
    // Occurrences are from 1->N
    firstTermOcc = 0;
    vector<int>::iterator it = plists[0]->begin();
    do {
	if (it == plists[0]->end())
	    return false;
	pos = *it++;
	firstTermOcc++;
    } while (!do_proximity_test(window, plists, 1, pos, pos, &sta, &sto));
    SETMINMAX(pos, sta, sto);

    LOGDEB0(("myTextSplitCB::matchGroup: MATCH [%d,%d]\n", sta, sto)); 

    // Translate the position window into a byte offset window
    int bs = 0;
    map<int, pair<int, int> >::iterator i1 =  m_gpostobytes.find(sta);
    map<int, pair<int, int> >::iterator i2 =  m_gpostobytes.find(sto);
    if (i1 != m_gpostobytes.end() && i2 != m_gpostobytes.end()) {
	LOGDEB1(("myTextSplitCB::matchGroup: pushing %d %d\n",
		 i1->second.first, i2->second.second));
	tboffs.push_back(pair<int, int>(i1->second.first, i2->second.second));
	bs = i1->second.first;
    } else {
	LOGDEB(("myTextSplitCB::matchGroup: no bpos found for %d or %d\n", 
		sta, sto));
    }

    if (firstTerm.empty() || m_firstTermPos > sta) {
	// firsTerm is used to try an position the preview window over
	// the match. As it's difficult to divine byte/word positions
	// in qtextedit, we use a string search. Use the
	// shortest plist for this, which hopefully gives a better
	// chance for the group to be found (it's hopeless to try and
	// match the whole group)
	map<vector<int>*, string>::iterator it = 
	    plistToTerm.find(plists.front());
	if (it != plistToTerm.end())
	    firstTerm = it->second;
	LOGDEB(("myTextSplitCB:: best group term %s, firstTermOcc %d\n",
		firstTerm.c_str(), firstTermOcc));
	m_firstTermPos = sta;
	m_firstTermBPos = bs;
    }

    return true;
}

/** Sort integer pairs by increasing first value and decreasing width */
class PairIntCmpFirst {
public:
    bool operator()(pair<int,int> a, pair<int, int>b) {
	if (a.first != b.first)
	    return a.first < b.first;
	return a.second > b.second;
    }
};

// Do the phrase match thing, then merge the highlight lists
bool myTextSplitCB::matchGroups()
{
    vector<vector<string> >::const_iterator vit = m_groups.begin();
    vector<int>::const_iterator sit = m_slacks.begin();
    for (; vit != m_groups.end() && sit != m_slacks.end(); vit++, sit++) {
	matchGroup(*vit, *sit + (*vit).size());
    }

    // Sort by start and end offsets. The merging of overlapping entries
    // will be handled during output.
    std::sort(tboffs.begin(), tboffs.end(), PairIntCmpFirst());
    return true;
}

const char *firstTermAnchorName = "FIRSTTERM";

#ifdef QT_SCROLL_TO_ANCHOR_BUG
const char *firstTermBeacon = "\xe2\xa0\x91\xe2\x96\x9f\x20\x01\x9a";
#endif

// Fix result text for display inside the gui text window.
//
// To compute the term character positions in the output text, we used
// to emulate how qt's textedit counts chars (ignoring tags and
// duplicate whitespace etc...). This was tricky business, dependant
// on qtextedit internals, and we don't do it any more, so we finally
// don't know the term par/car positions in the editor text.  Instead,
// we return the first term encountered, and the caller will use the
// editor's find() function to position on it
bool plaintorich(const string& in, string& out, 
		 RefCntr<Rcl::SearchData> sdata,
		 bool noHeader, bool fft)
{
    Chrono chron;
    out.erase();
    vector<string> terms;
    vector<vector<string> > groups;
    vector<int> slacks;

    sdata->getTerms(terms, groups, slacks);

    if (DebugLog::getdbl()->getlevel() >= DEBDEB0) {
	LOGDEB0(("plaintorich: terms: \n"));
	string sterms = vecStringToString(terms);
	LOGDEB0(("  %s\n", sterms.c_str()));
	sterms = "\n";
	LOGDEB0(("plaintorich: groups: \n"));
	for (vector<vector<string> >::iterator vit = groups.begin(); 
	     vit != groups.end(); vit++) {
	    sterms += vecStringToString(*vit);
	    sterms += "\n";
	}
	LOGDEB0(("  %s", sterms.c_str()));
    }

    // We first use the text splitter to break the text into words,
    // and compare the words to the search terms, which yields the
    // query terms positions inside the text
    myTextSplitCB cb(terms, groups, slacks);
    TextSplit splitter(&cb, TextSplit::TXTS_ONLYSPANS);
    // Note that splitter returns the term locations in byte, not
    // character offset
    splitter.text_to_words(in);
    LOGDEB(("plaintorich: split done %d mS\n", chron.millis()));

    cb.matchGroups();

    // Rich text output
    if (noHeader)
	out = "";
    else 
	out = "<qt><head><title></title></head><body><p>";

    // Iterator for the list of input term positions. We use it to
    // output highlight tags and to compute term positions in the
    // output text
    vector<pair<int, int> >::iterator tPosIt = cb.tboffs.begin();
    vector<pair<int, int> >::iterator tboffsend = cb.tboffs.end();

#if 0
    for (vector<pair<int, int> >::const_iterator it = cb.tboffs.begin();
	 it != cb.tboffs.end(); it++) {
	LOGDEB2(("plaintorich: region: %d %d\n", it->first, it->second));
    }
#endif

    // Input character iterator
    Utf8Iter chariter(in);
    // State variable used to limitate the number of consecutive empty lines 
    int ateol = 0;
    // State variable to update the char pos only for the first of
    // consecutive blank chars
    int atblank = 0;
    for (string::size_type pos = 0; pos != string::npos; pos = chariter++) {
	if ((pos & 0xfff) == 0) {
	    CancelCheck::instance().checkCancel();
	}
	// If we still have terms positions, check (byte) position
	if (tPosIt != tboffsend) {
	    int ibyteidx = chariter.getBpos();

	    if (fft && ibyteidx == cb.m_firstTermBPos) {
		out += string("<a name=\"") + firstTermAnchorName + "\"> "
#ifdef QT_SCROLL_TO_ANCHOR_BUG
		    + "<font color=\"white\"> " + firstTermBeacon + " </font> "
#endif
		    + "</a>";
	    }

	    if (ibyteidx == tPosIt->first) {
		out += "<termtag>";
	    } else if (ibyteidx == tPosIt->second) {
		// Output end tag, then skip all highlight areas that
		// would overlap this one
		out += "</termtag>";
		int crend = tPosIt->second;
		while (tPosIt != cb.tboffs.end() && tPosIt->first < crend)
		    tPosIt++;
	    }
	}
	switch(*chariter) {
	case '\n':
	    if (ateol < 2) {
		out += "<br>\n";
		ateol++;
	    }
	    break;
	case '\r': 
	    break;
	case '<':
	    ateol = 0;
	    out += "&lt;";
	    break;
	case '&':
	    ateol = 0;
	    out += "&amp;";
	    break;
	default:
	    // We don't change the eol status for whitespace, want a real line
	    if (*chariter == ' ' || *chariter == '\t') {
		atblank = 1;
	    } else {
		ateol = 0;
		atblank = 0;
	    }
	    chariter.appendchartostring(out);
	}
    }
#if 0
    {
	FILE *fp = fopen("/tmp/debugplaintorich", "a");
	fprintf(fp, "%s\n", out.c_str());
	fclose(fp);
    }
#endif
    LOGDEB(("plaintorich: done %d mS\n", chron.millis()));
    return true;
}
