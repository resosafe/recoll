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
#include "smallut.h"
#include "plaintorich.h"
#include "cancelcheck.h"
#include "unacpp.h"

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
class TextSplitPTR : public TextSplit {
 public:

    // Out: begin and end byte positions of query terms/groups in text
    vector<pair<int, int> > tboffs;  

    TextSplitPTR(const vector<string>& its, 
                 const vector<vector<string> >&groups, 
                 const vector<int>& slacks) 
	:  m_wcount(0), m_groups(groups), m_slacks(slacks)
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
	if (!unacmaybefold(term, dumb, "UTF-8", true)) {
	    LOGINFO(("PlainToRich::splitter::takeword: unac failed for [%s]\n",
                     term.c_str()));
	    return true;
	}
	//LOGDEB2(("Input dumbbed term: '%s' %d %d %d\n", dumb.c_str(), 
	// pos, bts, bte));

	// If this word is a search term, remember its byte-offset span. 
	if (m_terms.find(dumb) != m_terms.end()) {
	    tboffs.push_back(pair<int, int>(bts, bte));
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

// Recursively check that each term is inside the window (which is
// readjusted as the successive terms are found). i is the index for
// the next position list to use (initially 1)
static bool do_proximity_test(int window, vector<vector<int>* >& plists, 
			      unsigned int i, int min, int max, 
			      int *sp, int *ep)
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
bool TextSplitPTR::matchGroup(const vector<string>& terms, int window)
{
    LOGDEB0(("TextSplitPTR::matchGroup:d %d: %s\n", window,
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
	    LOGDEB0(("TextSplitPTR::matchGroup: [%s] not found in m_plists\n",
		    (*it).c_str()));
	    continue;
	}
	plists.push_back(&(pl->second));
	plistToTerm[&(pl->second)] = *it;
	realgroup.push_back(*it);
    }
    LOGDEB0(("TextSplitPTR::matchGroup:d %d:real group after expansion %s\n", 
	     window, vecStringToString(realgroup).c_str()));
    if (plists.size() < 2) {
	LOGDEB0(("TextSplitPTR::matchGroup: no actual groups found\n"));
	return false;
    }
    // Sort the positions lists so that the shorter is first
    std::sort(plists.begin(), plists.end(), VecIntCmpShorter());

    { // Debug
	map<vector<int>*, string>::iterator it;
	it =  plistToTerm.find(plists[0]);
	if (it == plistToTerm.end()) {
	    // SuperWeird
	    LOGERR(("matchGroup: term for first list not found !?!\n"));
	    return false;
	}
	LOGDEB0(("matchGroup: walking the shortest plist. Term [%s], len %d\n",
		it->second.c_str(), plists[0]->size()));
    }

    // Walk the shortest plist and look for matches
    for (vector<int>::iterator it = plists[0]->begin(); 
	 it != plists[0]->end(); it++) {
	int pos = *it;
	int sta = int(10E9), sto = 0;
	LOGDEB0(("MatchGroup: Testing at pos %d\n", pos));
	if (do_proximity_test(window, plists, 1, pos, pos, &sta, &sto)) {
	    LOGDEB0(("TextSplitPTR::matchGroup: MATCH termpos [%d,%d]\n", 
		     sta, sto)); 
	    // Maybe extend the window by 1st term position, this was not
	    // done by do_prox..
	    SETMINMAX(pos, sta, sto);
	    // Translate the position window into a byte offset window
	    int bs = 0;
	    map<int, pair<int, int> >::iterator i1 =  m_gpostobytes.find(sta);
	    map<int, pair<int, int> >::iterator i2 =  m_gpostobytes.find(sto);
	    if (i1 != m_gpostobytes.end() && i2 != m_gpostobytes.end()) {
		LOGDEB0(("TextSplitPTR::matchGroup: pushing bpos %d %d\n",
			i1->second.first, i2->second.second));
		tboffs.push_back(pair<int, int>(i1->second.first, 
						i2->second.second));
		bs = i1->second.first;
	    } else {
		LOGDEB(("matchGroup: no bpos found for %d or %d\n", sta, sto));
	    }
	}
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
bool TextSplitPTR::matchGroups()
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


// Fix result text for display inside the gui text window.
//
// To compute the term character positions in the output text, we used
// to emulate how qt's textedit counts chars (ignoring tags and
// duplicate whitespace etc...). This was tricky business, dependant
// on qtextedit internals, and we don't do it any more, so we finally
// don't know the term par/car positions in the editor text.  
// Instead, we now mark the search term positions with html anchors
//
// We output the result in chunks, arranging not to cut in the middle of
// a tag, which would confuse qtextedit.
bool PlainToRich::plaintorich(const string& in, 
			      list<string>& out, // Output chunk list
			      const HiliteData& hdata,
			      int chunksize)
{
    Chrono chron;
    const vector<string>& terms(hdata.terms);
    const vector<vector<string> >& groups(hdata.groups);
    const vector<int>& slacks(hdata.gslks);

    if (0 && DebugLog::getdbl()->getlevel() >= DEBDEB0) {
	LOGDEB0(("plaintorich: terms: \n"));
	string sterms = vecStringToString(terms);
	LOGDEB0(("  %s\n", sterms.c_str()));
	sterms = "\n";
	LOGDEB0(("plaintorich: groups: \n"));
	for (vector<vector<string> >::const_iterator vit = groups.begin(); 
	     vit != groups.end(); vit++) {
	    sterms += "GROUP: ";
	    sterms += vecStringToString(*vit);
	    sterms += "\n";
	}
	LOGDEB0(("  %s", sterms.c_str()));
        LOGDEB2(("  TEXT:[%s]\n", in.c_str()));
    }

    // Compute the positions for the query terms.  We use the text
    // splitter to break the text into words, and compare the words to
    // the search terms,
    TextSplitPTR splitter(terms, groups, slacks);
    // Note: the splitter returns the term locations in byte, not
    // character, offsets.
    splitter.text_to_words(in);
    LOGDEB2(("plaintorich: split done %d mS\n", chron.millis()));

    // Compute the positions for NEAR and PHRASE groups.
    splitter.matchGroups();

    out.clear();
    out.push_back("");
    list<string>::iterator olit = out.begin();

    // Rich text output
    *olit = header();

    // Iterator for the list of input term positions. We use it to
    // output highlight tags and to compute term positions in the
    // output text
    vector<pair<int, int> >::iterator tPosIt = splitter.tboffs.begin();
    vector<pair<int, int> >::iterator tPosEnd = splitter.tboffs.end();

#if 0
    for (vector<pair<int, int> >::const_iterator it = splitter.tboffs.begin();
	 it != splitter.tboffs.end(); it++) {
	LOGDEB2(("plaintorich: region: %d %d\n", it->first, it->second));
    }
#endif

    // Input character iterator
    Utf8Iter chariter(in);

    // State variable used to limit the number of consecutive empty lines,
    // and convert all eol to '\n'
    int eol = 0;
    int hadcr = 0;

    // Value for numbered anchors at each term match
    int anchoridx = 1;
    // HTML state
    bool intag = false, inparamvalue = false;
    // My tag state
    int inrcltag = 0;

    string::size_type headend = 0;
    if (m_inputhtml) {
	headend = in.find("</head>");
	if (headend == string::npos)
	    headend = in.find("</HEAD>");
	if (headend != string::npos)
	    headend += 7;
    }

    for (string::size_type pos = 0; pos != string::npos; pos = chariter++) {
	// Check from time to time if we need to stop
	if ((pos & 0xfff) == 0) {
	    CancelCheck::instance().checkCancel();
	}

	// If we still have terms positions, check (byte) position. If
	// we are at or after a term match, mark.
	if (tPosIt != tPosEnd) {
	    int ibyteidx = chariter.getBpos();
	    if (ibyteidx == tPosIt->first) {
		if (!intag && ibyteidx > (int)headend) {
		    *olit += startAnchor(anchoridx);
		    *olit += startMatch();
		}
		anchoridx++;
                inrcltag = 1;
	    } else if (ibyteidx == tPosIt->second) {
		// Output end of match region tags
		if (!intag && ibyteidx > (int)headend) {
		    *olit += endMatch();
		    *olit += endAnchor();
		}
		// Skip all highlight areas that would overlap this one
		int crend = tPosIt->second;
		while (tPosIt != splitter.tboffs.end() && tPosIt->first < crend)
		    tPosIt++;
                inrcltag = 0;
	    }
	}
        
        unsigned int car = *chariter;

        if (car == '\n') {
            if (!hadcr)
                eol++;
            hadcr = 0;
            continue;
        } else if (car == '\r') {
            hadcr++;
            eol++;
            continue;
        } else if (eol) {
            // Do line break;
            hadcr = 0;
            if (eol > 2)
                eol = 2;
            while (eol) {
		if (!m_inputhtml && m_eolbr)
		    *olit += "<br>";
                *olit += "\n";
                eol--;
            }
            // Maybe end this chunk, begin next. Don't do it on html
            // there is just no way to do it right (qtextedit cant grok
            // chunks cut in the middle of <a></a> for example).
            if (!m_inputhtml && !inrcltag && 
                olit->size() > (unsigned int)chunksize) {
                out.push_back(string(startChunk()));
                olit++;
            }
        }

        switch (car) {
        case '<':
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = true;
                chariter.appendchartostring(*olit);    
            } else {
                *olit += "&lt;";
            }
            break;
        case '>':
            if (m_inputhtml) {
                if (!inparamvalue)
                    intag = false;
            }
            chariter.appendchartostring(*olit);    
            break;
        case '&':
            if (m_inputhtml) {
                chariter.appendchartostring(*olit);
            } else {
                *olit += "&amp;";
            }
            break;
        case '"':
            if (m_inputhtml && intag) {
                inparamvalue = !inparamvalue;
            }
            chariter.appendchartostring(*olit);
            break;
        default:
            chariter.appendchartostring(*olit);
        }

    } // End chariter loop

#if 0
    {
	FILE *fp = fopen("/tmp/debugplaintorich", "a");
	fprintf(fp, "BEGINOFPLAINTORICHOUTPUT\n");
	for (list<string>::iterator it = out.begin();
	     it != out.end(); it++) {
	    fprintf(fp, "BEGINOFPLAINTORICHCHUNK\n");
	    fprintf(fp, "%s", it->c_str());
	    fprintf(fp, "ENDOFPLAINTORICHCHUNK\n");
	}
	fprintf(fp, "ENDOFPLAINTORICHOUTPUT\n");
	fclose(fp);
    }
#endif
    LOGDEB2(("plaintorich: done %d mS\n", chron.millis()));
    return true;
}
