/* Copyright (C) 2006 J.F.Dockes
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

// Handle translation from rcl's SearchData structures to Xapian Queries
#include <stdio.h>
#include <fnmatch.h>

#include <string>
#include <vector>
#include <algorithm>

#include "xapian.h"

#include "cstr.h"
#include "rcldb.h"
#include "searchdata.h"
#include "debuglog.h"
#include "smallut.h"
#include "textsplit.h"
#include "unacpp.h"
#include "utf8iter.h"
#include "stoplist.h"
#include "rclconfig.h"

#ifndef NO_NAMESPACES
using namespace std;
namespace Rcl {
#endif

typedef  vector<SearchDataClause *>::iterator qlist_it_t;
typedef  vector<SearchDataClause *>::const_iterator qlist_cit_t;

static const int original_term_wqf_booster = 10;

/* The dates-to-query routine is is lifted quasi-verbatim but
 *  modified from xapian-omega:date.cc. Copyright info:
 *
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2001 James Aylett
 * Copyright 2001,2002 Ananova Ltd
 * Copyright 2002 Intercede 1749 Ltd
 * Copyright 2002,2003,2006 Olly Betts
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
static Xapian::Query
date_range_filter(int y1, int m1, int d1, int y2, int m2, int d2)
{
    // Xapian uses a smallbuf and snprintf. Can't be bothered, we're
    // only doing %d's !
    char buf[200];
    sprintf(buf, "D%04d%02d", y1, m1);
    vector<Xapian::Query> v;

    int d_last = monthdays(m1, y1);
    int d_end = d_last;
    if (y1 == y2 && m1 == m2 && d2 < d_last) {
	d_end = d2;
    }
    // Deal with any initial partial month
    if (d1 > 1 || d_end < d_last) {
    	for ( ; d1 <= d_end ; d1++) {
	    sprintf(buf + 7, "%02d", d1);
	    v.push_back(Xapian::Query(buf));
	}
    } else {
	buf[0] = 'M';
	v.push_back(Xapian::Query(buf));
    }
    
    if (y1 == y2 && m1 == m2) {
	return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
    }

    int m_last = (y1 < y2) ? 12 : m2 - 1;
    while (++m1 <= m_last) {
	sprintf(buf + 5, "%02d", m1);
	buf[0] = 'M';
	v.push_back(Xapian::Query(buf));
    }
	
    if (y1 < y2) {
	while (++y1 < y2) {
	    sprintf(buf + 1, "%04d", y1);
	    buf[0] = 'Y';
	    v.push_back(Xapian::Query(buf));
	}
	sprintf(buf + 1, "%04d", y2);
	buf[0] = 'M';
	for (m1 = 1; m1 < m2; m1++) {
	    sprintf(buf + 5, "%02d", m1);
	    v.push_back(Xapian::Query(buf));
	}
    }
	
    sprintf(buf + 5, "%02d", m2);

    // Deal with any final partial month
    if (d2 < monthdays(m2, y2)) {
	buf[0] = 'D';
    	for (d1 = 1 ; d1 <= d2; d1++) {
	    sprintf(buf + 7, "%02d", d1);
	    v.push_back(Xapian::Query(buf));
	}
    } else {
	buf[0] = 'M';
	v.push_back(Xapian::Query(buf));
    }

    return Xapian::Query(Xapian::Query::OP_OR, v.begin(), v.end());
}

// Expand categories and mime type wild card exps
bool SearchData::expandFileTypes(RclConfig *cfg, vector<string>& tps)
{
    if (!cfg) {
	LOGFATAL(("Db::expandFileTypes: null configuration!!\n"));
	return false;
    }
    vector<string> exptps;
    list<string> alltypes = cfg->getAllMimeTypes();

    for (vector<string>::iterator it = tps.begin(); it != tps.end(); it++) {
	if (cfg->isMimeCategory(*it)) {
	    list<string>tps;
	    cfg->getMimeCatTypes(*it, tps);
	    exptps.insert(exptps.end(), tps.begin(), tps.end());
	} else {
	    for (list<string>::const_iterator ait = alltypes.begin();
		 ait != alltypes.end(); ait++) {
		if (fnmatch(it->c_str(), ait->c_str(), FNM_CASEFOLD) 
		    != FNM_NOMATCH) {
		    exptps.push_back(*ait);
		}
	    }
	}
    }
    tps = exptps;
    return true;
}

bool SearchData::toNativeQuery(Rcl::Db &db, void *d)
{
    LOGDEB2(("SearchData::toNativeQuery: stemlang [%s]\n", 
	    m_stemlang.c_str()));
    Xapian::Query xq;
    m_reason.erase();

    // Walk the clause list translating each in turn and building the 
    // Xapian query tree
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++) {
	Xapian::Query nq;
	if (!(*it)->toNativeQuery(db, &nq, m_stemlang)) {
	    LOGERR(("SearchData::toNativeQuery: failed\n"));
	    m_reason = (*it)->getReason();
	    return false;
	}	    
        if (nq.empty()) {
            LOGDEB(("SearchData::toNativeQuery: skipping empty clause\n"));
            continue;
        }
	// If this structure is an AND list, must use AND_NOT for excl clauses.
	// Else this is an OR list, and there can't be excl clauses (checked by
	// addClause())
	Xapian::Query::op op;
	if (m_tp == SCLT_AND) {
            if ((*it)->m_tp == SCLT_EXCL) {
                op =  Xapian::Query::OP_AND_NOT;
            } else {
                op =  Xapian::Query::OP_AND;
            }
	} else {
	    op = Xapian::Query::OP_OR;
	}
        if (xq.empty()) {
            if (op == Xapian::Query::OP_AND_NOT)
                xq = Xapian::Query(op, Xapian::Query::MatchAll, nq);
            else 
                xq = nq;
        } else {
            xq = Xapian::Query(op, xq, nq);
        }
    }
    if (xq.empty())
	xq = Xapian::Query::MatchAll;

    if (m_haveDates) {
        // If one of the extremities is unset, compute db extremas
        if (m_dates.y1 == 0 || m_dates.y2 == 0) {
            int minyear = 1970, maxyear = 2100;
            if (!db.maxYearSpan(&minyear, &maxyear)) {
                LOGERR(("Can't retrieve index min/max dates\n"));
                //whatever, go on.
            }

            if (m_dates.y1 == 0) {
                m_dates.y1 = minyear;
                m_dates.m1 = 1;
                m_dates.d1 = 1;
            }
            if (m_dates.y2 == 0) {
                m_dates.y2 = maxyear;
                m_dates.m2 = 12;
                m_dates.d2 = 31;
            }
        }
        LOGDEB(("Db::toNativeQuery: date interval: %d-%d-%d/%d-%d-%d\n",
                m_dates.y1, m_dates.m1, m_dates.d1,
                m_dates.y2, m_dates.m2, m_dates.d2));
        Xapian::Query dq = date_range_filter(m_dates.y1, m_dates.m1, m_dates.d1,
                m_dates.y2, m_dates.m2, m_dates.d2);
        if (dq.empty()) {
            LOGINFO(("Db::toNativeQuery: date filter is empty\n"));
        }
        // If no probabilistic query is provided then promote the daterange
        // filter to be THE query instead of filtering an empty query.
        if (xq.empty()) {
            LOGINFO(("Db::toNativeQuery: proba query is empty\n"));
            xq = dq;
        } else {
            xq = Xapian::Query(Xapian::Query::OP_FILTER, xq, dq);
        }
    }

    // Add the file type filtering clause if any
    if (!m_filetypes.empty()) {
	expandFileTypes(db.getConf(), m_filetypes);
	    
	Xapian::Query tq;
	for (vector<string>::iterator it = m_filetypes.begin(); 
	     it != m_filetypes.end(); it++) {
	    string term = "T" + *it;
	    LOGDEB0(("Adding file type term: [%s]\n", term.c_str()));
	    tq = tq.empty() ? Xapian::Query(term) : 
		Xapian::Query(Xapian::Query::OP_OR, tq, Xapian::Query(term));
	}
	xq = xq.empty() ? tq : Xapian::Query(Xapian::Query::OP_FILTER, xq, tq);
    }

    // Add the neg file type filtering clause if any
    if (!m_nfiletypes.empty()) {
	expandFileTypes(db.getConf(), m_nfiletypes);
	    
	Xapian::Query tq;
	for (vector<string>::iterator it = m_nfiletypes.begin(); 
	     it != m_nfiletypes.end(); it++) {
	    string term = "T" + *it;
	    LOGDEB0(("Adding negative file type term: [%s]\n", term.c_str()));
	    tq = tq.empty() ? Xapian::Query(term) : 
		Xapian::Query(Xapian::Query::OP_OR, tq, Xapian::Query(term));
	}
	xq = xq.empty() ? tq : Xapian::Query(Xapian::Query::OP_AND_NOT, xq, tq);
    }

    // Add the directory filtering clause. This is a phrase of terms
    // prefixed with the pathelt prefix XP
    if (!m_topdir.empty()) {
	vector<string> vpath;
	stringToTokens(m_topdir, vpath, "/");
	vector<string> pvpath;
	pvpath.push_back(pathelt_prefix);
	for (vector<string>::const_iterator it = vpath.begin(); 
	     it != vpath.end(); it++){
	    pvpath.push_back(pathelt_prefix + *it);
	}
	Xapian::Query::op tdop;
	if (m_topdirweight == 1.0) {
	    tdop = m_topdirexcl ? 
		Xapian::Query::OP_AND_NOT : Xapian::Query::OP_FILTER;
	} else {
	    tdop = m_topdirexcl ? 
		Xapian::Query::OP_AND_NOT : Xapian::Query::OP_AND_MAYBE;
	}
	Xapian::Query tdq = Xapian::Query(Xapian::Query::OP_PHRASE, 
					  pvpath.begin(), pvpath.end());
	if (m_topdirweight != 1.0)
	    tdq = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, 
				tdq, m_topdirweight);

	xq = Xapian::Query(tdop, xq, tdq);
    }

    *((Xapian::Query *)d) = xq;
    return true;
}

// This is called by the GUI simple search if the option is set: add
// (OR) phrase to a query (if it is simple enough) so that results
// where the search terms are close and in order will come up on top.
// We remove very common terms from the query to avoid performance issues.
bool SearchData::maybeAddAutoPhrase(Rcl::Db& db, double freqThreshold)
{
    LOGDEB0(("SearchData::maybeAddAutoPhrase()\n"));
    if (!m_query.size()) {
	LOGDEB2(("SearchData::maybeAddAutoPhrase: empty query\n"));
	return false;
    }

    string field;
    list<string> words;
    // Walk the clause list. If we find any non simple clause or different
    // field names, bail out.
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++) {
	SClType tp = (*it)->m_tp;
	if (tp != SCLT_AND && tp != SCLT_OR) {
	    LOGDEB2(("SearchData::maybeAddAutoPhrase: rejected clause\n"));
	    return false;
	}
	SearchDataClauseSimple *clp = 
	    dynamic_cast<SearchDataClauseSimple*>(*it);
	if (clp == 0) {
	    LOGDEB2(("SearchData::maybeAddAutoPhrase: dyncast failed\n"));
	    return false;
	}
	if (it == m_query.begin()) {
	    field = clp->getfield();
	} else {
	    if (clp->getfield().compare(field)) {
		LOGDEB2(("SearchData::maybeAddAutoPhrase: diff. fields\n"));
		return false;
	    }
	}

	// If there are wildcards or quotes in there, bail out
	if (clp->gettext().find_first_of("\"*[?") != string::npos) { 
	    LOGDEB2(("SearchData::maybeAddAutoPhrase: wildcards\n"));
	    return false;
	}
        // Do a simple word-split here, don't bother with the full-blown
	// textsplit. The autophrase thing is just "best effort", it's
	// normal that it won't work in strange cases.
	vector<string> wl;
	stringToStrings(clp->gettext(), wl);
	words.insert(words.end(), wl.begin(), wl.end());
    }


    // Trim the word list by eliminating very frequent terms
    // (increasing the slack as we do it):
    int slack = 0;
    int doccnt = db.docCnt();
    if (!doccnt)
	doccnt = 1;
    string swords;
    for (list<string>::iterator it = words.begin(); 
	 it != words.end(); it++) {
	double freq = double(db.termDocCnt(*it)) / doccnt;
	if (freq < freqThreshold) {
	    if (!swords.empty())
		swords.append(1, ' ');
	    swords += *it;
	} else {
	    LOGDEB0(("Autophrase: [%s] too frequent (%.2f %%)\n", 
		    it->c_str(), 100 * freq));
	    slack++;
	}
    }
    
    // We can't make a phrase with a single word :)
    if (TextSplit::countWords(swords) <= 1) {
	LOGDEB2(("SearchData::maybeAddAutoPhrase: ended with 1 word\n"));
	return false;
    }
	
    SearchDataClauseDist *nclp = 
	new SearchDataClauseDist(SCLT_PHRASE, swords, slack, field);

    // If the toplevel conjunction is an OR, just OR the phrase, else 
    // deepen the tree.
    if (m_tp == SCLT_OR) {
	addClause(nclp);
    } else {
	// My type is AND. Change it to OR and insert two queries, one
	// being the original query as a subquery, the other the
	// phrase.
	SearchData *sd = new SearchData(m_tp);
	sd->m_query = m_query;
	sd->m_stemlang = m_stemlang;
	m_tp = SCLT_OR;
	m_query.clear();
	SearchDataClauseSub *oq = 
	    new SearchDataClauseSub(SCLT_OR, RefCntr<SearchData>(sd));
	addClause(oq);
	addClause(nclp);
    }
    return true;
}

// Add clause to current list. OR lists cant have EXCL clauses.
bool SearchData::addClause(SearchDataClause* cl)
{
    if (m_tp == SCLT_OR && (cl->m_tp == SCLT_EXCL)) {
	LOGERR(("SearchData::addClause: cant add EXCL to OR list\n"));
	m_reason = "No Negative (AND_NOT) clauses allowed in OR queries";
	return false;
    }
    cl->setParent(this);
    m_haveWildCards = m_haveWildCards || cl->m_haveWildCards;
    m_query.push_back(cl);
    return true;
}

// Make me all new
void SearchData::erase() {
    LOGDEB0(("SearchData::erase\n"));
    m_tp = SCLT_AND;
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++)
	delete *it;
    m_query.clear();
    m_filetypes.clear();
    m_topdir.erase();
    m_topdirexcl = false;
    m_description.erase();
    m_reason.erase();
    m_haveDates = false;
}

// Am I a file name only search ? This is to turn off term highlighting
bool SearchData::fileNameOnly() 
{
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++)
	if (!(*it)->isFileName())
	    return false;
    return true;
}

// Extract all terms and term groups
bool SearchData::getTerms(vector<string>& terms, 
			  vector<vector<string> >& groups,
			  vector<int>& gslks) const
{
    for (qlist_cit_t it = m_query.begin(); it != m_query.end(); it++)
	(*it)->getTerms(terms, groups, gslks);
    return true;
}
// Extract user terms
void SearchData::getUTerms(vector<string>& terms) const
{
    for (qlist_cit_t it = m_query.begin(); it != m_query.end(); it++)
	(*it)->getUTerms(terms);
    sort(terms.begin(), terms.end());
    vector<string>::iterator it = unique(terms.begin(), terms.end());
    terms.erase(it, terms.end());
}

// Splitter callback for breaking a user string into simple terms and
// phrases. This is for parts of the user entry which would appear as
// a single word because there is no white space inside, but are
// actually multiple terms to rcldb (ie term1,term2)
class TextSplitQ : public TextSplit {
 public:
    TextSplitQ(Flags flags, const StopList &_stops) 
	: TextSplit(flags), stops(_stops), alltermcount(0), lastpos(0)
    {}
    bool takeword(const std::string &interm, int pos, int, int) {
	alltermcount++;
        lastpos = pos
	LOGDEB1(("TextSplitQ::takeword: %s\n", interm.c_str()));

	// Check if the first letter is a majuscule in which
	// case we do not want to do stem expansion. 
	bool nostemexp = unaciscapital(interm);
	string noaclowterm;
	if (!unacmaybefold(interm, noaclowterm, "UTF-8", true)) {
	    LOGINFO(("SearchData::splitter::takeword: unac failed for [%s]\n", 
                     interm.c_str()));
	    return true;
	}

	if (stops.isStop(noaclowterm)) {
	    LOGDEB1(("TextSplitQ::takeword [%s] in stop list\n", 
                     noaclowterm.c_str()));
	    return true;
	}
	terms.push_back(noaclowterm);
	nostemexps.push_back(nostemexp);
	return true;
    }

    vector<string> terms;
    vector<bool>   nostemexps;
    const StopList &stops;
    // Count of terms including stopwords: this is for adjusting
    // phrase/near slack
    int alltermcount; 
    int lastpos;
};

// A class used to translate a user compound string (*not* a query
// language string) as may be entered in any_terms/all_terms search
// entry fields, ex: [term1 "a phrase" term3] into a xapian query
// tree.
// The object keeps track of the query terms and term groups while
// translating.
class StringToXapianQ {
public:
    StringToXapianQ(Db& db, const string& field, 
		    const string &stmlng, bool boostUser)
	: m_db(db), m_field(field), m_stemlang(stmlng), 
	  m_doBoostUserTerms(boostUser)
    { }

    bool processUserString(const string &iq,
			   string &ermsg,
			   list<Xapian::Query> &pqueries, 
			   const StopList &stops,
			   int slack = 0, bool useNear = false);
    // After processing the string: return search terms and term
    // groups (ie: for highlighting)
    bool getTerms(vector<string>& terms, vector<vector<string> >& groups) 
    {
	terms.insert(terms.end(), m_terms.begin(), m_terms.end());
	groups.insert(groups.end(), m_groups.begin(), m_groups.end());
	return true;
    }
    bool getUTerms(vector<string>& terms) 
    {
	terms.insert(terms.end(), m_uterms.begin(), m_uterms.end());
	return true;
    }

private:
    void expandTerm(bool dont, const string& term, list<string>& exp, 
                    string& sterm, const string& prefix);
    // After splitting entry on whitespace: process non-phrase element
    void processSimpleSpan(const string& span, bool nostemexp, list<Xapian::Query> &pqueries);
    // Process phrase/near element
    void processPhraseOrNear(TextSplitQ *splitData, 
			     list<Xapian::Query> &pqueries,
			     bool useNear, int slack, int mods);

    Db&           m_db;
    const string& m_field;
    const string& m_stemlang;
    bool          m_doBoostUserTerms;
    // Single terms and phrases resulting from breaking up text;
    vector<string>          m_uterms;
    vector<string>          m_terms;
    vector<vector<string> > m_groups; 
};

#if 0
static void listVector(const string& what, const vector<string>&l)
{
    string a;
    for (vector<string>::const_iterator it = l.begin(); it != l.end(); it++) {
        a = a + *it + " ";
    }
    LOGDEB(("%s: %s\n", what.c_str(), a.c_str()));
}
#endif

/** Expand stem and wildcards
 *
 * @param nostemexp don't perform stem expansion. This is mainly used to
 *   prevent stem expansion inside phrases (because the user probably
 *   does not expect it). This does NOT prevent wild card expansion.
 *   Other factors than nostemexp can prevent stem expansion: 
 *   a null stemlang, resulting from a global user preference, a
 *   capitalized term, or wildcard(s)
 * @param term input single word
 * @param exp output expansion list
 * @param sterm output original input term if there were no wildcards
 */
void StringToXapianQ::expandTerm(bool nostemexp, 
                                 const string& term, 
                                 list<string>& exp,
                                 string &sterm, const string& prefix)
{
    LOGDEB2(("expandTerm: field [%s] term [%s] stemlang [%s] nostemexp %d\n",
	     m_field.c_str(), term.c_str(), m_stemlang.c_str(), nostemexp));
    sterm.erase();
    exp.clear();
    if (term.empty()) {
	return;
    }

    bool haswild = term.find_first_of(cstr_minwilds) != string::npos;

    // No stemming if there are wildcards or prevented globally.
    if (haswild || m_stemlang.empty()) {
	LOGDEB2(("expandTerm: found wildcards or stemlang empty: no exp\n"));
	nostemexp = true;
    }

    if (nostemexp && !haswild) {
	sterm = term;
        m_uterms.push_back(sterm);
	exp.push_front(prefix + term);
	exp.resize(1);
    } else {
	TermMatchResult res;
	if (haswild) {
	    m_db.termMatch(Rcl::Db::ET_WILD, m_stemlang, term, res, -1, 
                           m_field);
	} else {
	    sterm = term;
            m_uterms.push_back(sterm);
	    m_db.termMatch(Rcl::Db::ET_STEM, m_stemlang, term, res, -1, 
			   m_field);
	}
	for (list<TermMatchEntry>::const_iterator it = res.entries.begin(); 
	     it != res.entries.end(); it++) {
	    exp.push_back(it->term);
	}
    }
    //listVector("ExpandTerm:uterms now: ", m_uterms);
}

// Do distribution of string vectors: a,b c,d -> a,c a,d b,c b,d
void multiply_groups(vector<vector<string> >::const_iterator vvit,
		     vector<vector<string> >::const_iterator vvend, 
		     vector<string>& comb,
		     vector<vector<string> >&allcombs)
{
    // Remember my string vector and compute next, for recursive calls.
    vector<vector<string> >::const_iterator myvit = vvit++;

    // Walk the string vector I'm called upon and, for each string,
    // add it to current result, an call myself recursively on the
    // next string vector. The last call (last element of the vector of
    // vectors), adds the elementary result to the output

    // Walk my string vector
    for (vector<string>::const_iterator strit = (*myvit).begin();
	 strit != (*myvit).end(); strit++) {

	// Add my current value to the string vector we're building
	comb.push_back(*strit);

	if (vvit == vvend) {
	    // Last call: store current result
	    allcombs.push_back(comb);
	} else {
	    // Call recursively on next string vector
	    multiply_groups(vvit, vvend, comb, allcombs);
	}
	// Pop the value I just added (make room for the next element in my
	// vector)
	comb.pop_back();
    }
}

void StringToXapianQ::processSimpleSpan(const string& span, bool nostemexp,
					list<Xapian::Query> &pqueries)
{
    LOGDEB2(("StringToXapianQ::processSimpleSpan: [%s] nostemexp %d\n",
	     span.c_str(), int(nostemexp)));
    list<string> exp;  
    string sterm; // dumb version of user term

    string prefix;
    const FieldTraits *ftp;
    if (!m_field.empty() && m_db.fieldToTraits(m_field, &ftp)) {
	prefix = ftp->pfx;
    }

    expandTerm(nostemexp, span, exp, sterm, prefix);

    // m_terms is used for highlighting, we don't want prefixes in there.
    for (list<string>::const_iterator it = exp.begin(); 
	 it != exp.end(); it++) {
	m_terms.push_back(it->substr(prefix.size()));
    }
    // Push either term or OR of stem-expanded set
    Xapian::Query xq(Xapian::Query::OP_OR, exp.begin(), exp.end());

    // If sterm (simplified original user term) is not null, give it a
    // relevance boost. We do this even if no expansion occurred (else
    // the non-expanded terms in a term list would end-up with even
    // less wqf). This does not happen if there are wildcards anywhere
    // in the search.
    if (m_doBoostUserTerms && !sterm.empty()) {
        xq = Xapian::Query(Xapian::Query::OP_OR, xq, 
			   Xapian::Query(prefix+sterm, 
					 original_term_wqf_booster));
    }
    pqueries.push_back(xq);
}

// User entry element had several terms: transform into a PHRASE or
// NEAR xapian query, the elements of which can themselves be OR
// queries if the terms get expanded by stemming or wildcards (we
// don't do stemming for PHRASE though)
void StringToXapianQ::processPhraseOrNear(TextSplitQ *splitData, 
					  list<Xapian::Query> &pqueries,
					  bool useNear, int slack, int mods)
{
    Xapian::Query::op op = useNear ? Xapian::Query::OP_NEAR : 
	Xapian::Query::OP_PHRASE;
    list<Xapian::Query> orqueries;
    bool hadmultiple = false;
    vector<vector<string> >groups;

    string prefix;
    const FieldTraits *ftp;
    if (!m_field.empty() && m_db.fieldToTraits(m_field, &ftp)) {
	prefix = ftp->pfx;
    }

    if (mods & Rcl::SearchDataClause::SDCM_ANCHORSTART) {
	orqueries.push_back(Xapian::Query(prefix + start_of_field_term));
	slack++;
    }

    // Go through the list and perform stem/wildcard expansion for each element
    vector<bool>::iterator nxit = splitData->nostemexps.begin();
    for (vector<string>::iterator it = splitData->terms.begin();
	 it != splitData->terms.end(); it++, nxit++) {
	// Adjust when we do stem expansion. Not inside phrases, and
	// some versions of xapian will accept only one OR clause
	// inside NEAR, all others must be leafs.
	bool nostemexp = *nxit || (op == Xapian::Query::OP_PHRASE) || hadmultiple;

	string sterm;
	list<string>exp;
	expandTerm(nostemexp, *it, exp, sterm, prefix);

	// groups is used for highlighting, we don't want prefixes in there.
	vector<string> noprefs;
	for (list<string>::const_iterator it = exp.begin(); 
	     it != exp.end(); it++) {
	    noprefs.push_back(it->substr(prefix.size()));
	}
	groups.push_back(noprefs);
	orqueries.push_back(Xapian::Query(Xapian::Query::OP_OR, 
					  exp.begin(), exp.end()));
#ifdef XAPIAN_NEAR_EXPAND_SINGLE_BUF
	if (exp.size() > 1) 
	    hadmultiple = true;
#endif
    }

    if (mods & Rcl::SearchDataClause::SDCM_ANCHOREND) {
	orqueries.push_back(Xapian::Query(prefix + end_of_field_term));
	slack++;
    }

    // Generate an appropriate PHRASE/NEAR query with adjusted slack
    // For phrases, give a relevance boost like we do for original terms
    LOGDEB2(("PHRASE/NEAR: alltermcount %d lastpos %d\n", 
             splitData->alltermcount, splitData->lastpos));
    Xapian::Query xq(op, orqueries.begin(), orqueries.end(),
		     splitData->lastpos + 1 + slack);
    if (op == Xapian::Query::OP_PHRASE)
	xq = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, xq, 
			   original_term_wqf_booster);
    pqueries.push_back(xq);

    // Add all combinations of NEAR/PHRASE groups to the highlighting data. 
    vector<vector<string> > allcombs;
    vector<string> comb;
    multiply_groups(groups.begin(), groups.end(), comb, allcombs);
    m_groups.insert(m_groups.end(), allcombs.begin(), allcombs.end());
}

// Trim string beginning with ^ or ending with $ and convert to flags
static int stringToMods(string& s)
{
    int mods = 0;
    // Check for an anchored search
    trimstring(s);
    if (s.length() > 0 && s[0] == '^') {
	mods |= Rcl::SearchDataClause::SDCM_ANCHORSTART;
	s.erase(0, 1);
    }
    if (s.length() > 0 && s[s.length()-1] == '$') {
	mods |= Rcl::SearchDataClause::SDCM_ANCHOREND;
	s.erase(s.length()-1);
    }
    return mods;
}

/** 
 * Turn user entry string (NOT query language) into a list of xapian queries.
 * We just separate words and phrases, and do wildcard and stem expansion,
 *
 * This is used to process data entered into an OR/AND/NEAR/PHRASE field of
 * the GUI.
 *
 * The final list contains one query for each term or phrase
 *   - Elements corresponding to a stem-expanded part are an OP_OR
 *     composition of the stem-expanded terms (or a single term query).
 *   - Elements corresponding to phrase/near are an OP_PHRASE/NEAR
 *     composition of the phrase terms (no stem expansion in this case)
 * @return the subquery count (either or'd stem-expanded terms or phrase word
 *   count)
 */
bool StringToXapianQ::processUserString(const string &iq,
					string &ermsg,
					list<Xapian::Query> &pqueries,
					const StopList& stops,
					int slack, 
					bool useNear
					)
{
    LOGDEB(("StringToXapianQ:: query string: [%s]\n", iq.c_str()));
    ermsg.erase();
    m_uterms.clear();
    m_terms.clear();
    m_groups.clear();

    // Simple whitespace-split input into user-level words and
    // double-quoted phrases: word1 word2 "this is a phrase". 
    //
    // The text splitter may further still decide that the resulting
    // "words" are really phrases, this depends on separators:
    // [paul@dom.net] would still be a word (span), but [about:me]
    // will probably be handled as a phrase.
    list<string> phrases;
    TextSplit::stringToStrings(iq, phrases);

    // Process each element: textsplit into terms, handle stem/wildcard 
    // expansion and transform into an appropriate Xapian::Query
    try {
	for (list<string>::iterator it = phrases.begin(); 
	     it != phrases.end(); it++) {
	    LOGDEB0(("strToXapianQ: phrase/word: [%s]\n", it->c_str()));
	    int mods = stringToMods(*it);
	    int terminc = mods != 0 ? 1 : 0;
	    // If there are multiple spans in this element, including
	    // at least one composite, we have to increase the slack
	    // else a phrase query including a span would fail. 
	    // Ex: "term0@term1 term2" is onlyspans-split as:
	    //   0 term0@term1             0   12
	    //   2 term2                  13   18
	    // The position of term2 is 2, not 1, so a phrase search
	    // would fail.
	    // We used to do  word split, searching for 
	    // "term0 term1 term2" instead, which may have worse 
	    // performance, but will succeed.
	    // We now adjust the phrase/near slack by the term count
	    // difference (this is mainly better for cjk where this is a very
	    // common occurrence because of the ngrams thing.
	    TextSplitQ splitterS(TextSplit::Flags(TextSplit::TXTS_ONLYSPANS | 
                                                  TextSplit::TXTS_KEEPWILD), 
                                 stops);
	    splitterS.text_to_words(*it);
	    TextSplitQ splitterW(TextSplit::Flags(TextSplit::TXTS_NOSPANS | 
                                                  TextSplit::TXTS_KEEPWILD),
                                 stops);
	    splitterW.text_to_words(*it);
	    TextSplitQ *splitter = &splitterS;
	    if (splitterS.terms.size() > 1 && 
		splitterS.terms.size() != splitterW.terms.size()) {
		slack += splitterW.terms.size() - splitterS.terms.size();
		// used to: splitData = &splitDataW;
	    }

	    LOGDEB0(("strToXapianQ: termcount: %d\n", splitter->terms.size()));
	    switch (splitter->terms.size() + terminc) {
	    case 0: 
		continue;// ??
	    case 1: 
		processSimpleSpan(splitter->terms.front(), 
                                  splitter->nostemexps.front(), pqueries);
		break;
	    default:
		processPhraseOrNear(splitter, pqueries, useNear, slack, mods);
	    }
	}
    } catch (const Xapian::Error &e) {
	ermsg = e.get_msg();
    } catch (const string &s) {
	ermsg = s;
    } catch (const char *s) {
	ermsg = s;
    } catch (...) {
	ermsg = "Caught unknown exception";
    }
    if (!ermsg.empty()) {
	LOGERR(("stringToXapianQueries: %s\n", ermsg.c_str()));
	return false;
    }
    return true;
}

// Translate a simple OR, AND, or EXCL search clause. 
bool SearchDataClauseSimple::toNativeQuery(Rcl::Db &db, void *p, 
					   const string& stemlang)
{
    const string& l_stemlang = (m_modifiers&SDCM_NOSTEMMING)? cstr_null:
	stemlang;
    LOGDEB2(("SearchDataClauseSimple::toNativeQuery: stemlang [%s]\n",
	     stemlang.c_str()));

    m_terms.clear();
    m_groups.clear();
    Xapian::Query *qp = (Xapian::Query *)p;
    *qp = Xapian::Query();

    Xapian::Query::op op;
    switch (m_tp) {
    case SCLT_AND: op = Xapian::Query::OP_AND; break;
	// EXCL will be set with AND_NOT in the list. So it's an OR list here
    case SCLT_OR: 
    case SCLT_EXCL: op = Xapian::Query::OP_OR; break;
    default:
	LOGERR(("SearchDataClauseSimple: bad m_tp %d\n", m_tp));
	return false;
    }
    list<Xapian::Query> pqueries;

    // We normally boost the original term in the stem expansion list. Don't
    // do it if there are wildcards anywhere, this would skew the results.
    bool doBoostUserTerm = 
	(m_parentSearch && !m_parentSearch->haveWildCards()) || 
	(m_parentSearch == 0 && !m_haveWildCards);

    StringToXapianQ tr(db, m_field, l_stemlang, doBoostUserTerm);
    if (!tr.processUserString(m_text, m_reason, pqueries, db.getStopList()))
	return false;
    if (pqueries.empty()) {
	LOGERR(("SearchDataClauseSimple: resolved to null query\n"));
	return true;
    }
    tr.getTerms(m_terms, m_groups);
    tr.getUTerms(m_uterms);
    //listVector("SearchDataClauseSimple: Uterms: ", m_uterms);
    *qp = Xapian::Query(op, pqueries.begin(), pqueries.end());
    if  (m_weight != 1.0) {
	*qp = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, *qp, m_weight);
    }
    return true;
}

// Translate a FILENAME search clause. This mostly (or always) comes
// from a "filename" search from the gui or recollq. A query language
// "filename:"-prefixed field will not go through here, but through
// the generic field-processing code.
//
// In the case of multiple space-separated fragments, we generate an
// AND of OR queries. Each OR query comes from the expansion of a
// fragment. We used to generate a single OR with all expanded terms,
// which did not make much sense.
bool SearchDataClauseFilename::toNativeQuery(Rcl::Db &db, void *p, 
					     const string&)
{
    Xapian::Query *qp = (Xapian::Query *)p;
    *qp = Xapian::Query();

    list<string> patterns;
    TextSplit::stringToStrings(m_text, patterns);
    list<string> names;
    for (list<string>::iterator it = patterns.begin();
	 it != patterns.end(); it++) {
	list<string> more;
	db.filenameWildExp(*it, more);
	Xapian::Query tq = Xapian::Query(Xapian::Query::OP_OR, more.begin(), 
					 more.end());
	*qp = qp->empty() ? tq : Xapian::Query(Xapian::Query::OP_AND, *qp, tq);
    }
    if (m_weight != 1.0) {
	*qp = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, *qp, m_weight);
    }
    return true;
}

// Translate NEAR or PHRASE clause. 
bool SearchDataClauseDist::toNativeQuery(Rcl::Db &db, void *p, 
					 const string& stemlang)
{
    const string& l_stemlang = (m_modifiers&SDCM_NOSTEMMING)? cstr_null:
	stemlang;
    LOGDEB(("SearchDataClauseDist::toNativeQuery\n"));
    m_terms.clear();
    m_groups.clear();

    Xapian::Query *qp = (Xapian::Query *)p;
    *qp = Xapian::Query();

    list<Xapian::Query> pqueries;
    Xapian::Query nq;

    // We normally boost the original term in the stem expansion list. Don't
    // do it if there are wildcards anywhere, this would skew the results.
    bool doBoostUserTerm = 
	(m_parentSearch && !m_parentSearch->haveWildCards()) || 
	(m_parentSearch == 0 && !m_haveWildCards);

    // We produce a single phrase out of the user entry then use
    // stringToXapianQueries() to lowercase and simplify the phrase
    // terms etc. This will result into a single (complex)
    // Xapian::Query.
    if (m_text.find('\"') != string::npos) {
	m_text = neutchars(m_text, "\"");
    }
    string s = cstr_dquote + m_text + cstr_dquote;
    bool useNear = (m_tp == SCLT_NEAR);
    StringToXapianQ tr(db, m_field, l_stemlang, doBoostUserTerm);
    if (!tr.processUserString(s, m_reason, pqueries, db.getStopList(),
			      m_slack, useNear))
	return false;
    if (pqueries.empty()) {
	LOGERR(("SearchDataClauseDist: resolved to null query\n"));
	return true;
    }
    tr.getTerms(m_terms, m_groups);
    tr.getUTerms(m_uterms);
    *qp = *pqueries.begin();
    if (m_weight != 1.0) {
	*qp = Xapian::Query(Xapian::Query::OP_SCALE_WEIGHT, *qp, m_weight);
    }
    return true;
}

// Translate subquery
bool SearchDataClauseSub::toNativeQuery(Rcl::Db &db, void *p, const string&)
{
    return m_sub->toNativeQuery(db, p);
}

bool SearchDataClauseSub::getTerms(vector<string>& terms, 
				   vector<vector<string> >& groups,
				   vector<int>& gslks) const
{
    return m_sub.getconstptr()->getTerms(terms, groups, gslks);
}
void SearchDataClauseSub::getUTerms(vector<string>& terms) const
{
    m_sub.getconstptr()->getUTerms(terms);
}

} // Namespace Rcl
