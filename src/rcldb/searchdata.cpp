#ifndef lint
static char rcsid[] = "@(#$Id: searchdata.cpp,v 1.26.2.1 2008-12-05 11:17:51 dockes Exp $ (C) 2006 J.F.Dockes";
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

// Handle translation from rcl's SearchData structures to Xapian Queries

#include <string>
#include <vector>

#include "xapian.h"

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

bool SearchData::toNativeQuery(Rcl::Db &db, void *d)
{
    Xapian::Query xq;
    m_reason.erase();

    if (m_query.size() < 1) {
	m_reason = "empty query";
	return false;
    }

    // It's not allowed to have a pure negative query and also it
    // seems that Xapian doesn't like the first element to be AND_NOT
    qlist_it_t itnotneg = m_query.end();
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++) {
	if ((*it)->m_tp != SCLT_EXCL) {
	    itnotneg = it;
	    break;
	}
    }
    if (itnotneg == m_query.end()) {
	LOGERR(("SearchData::toNativeQuery: can't have all negative clauses"));
	m_reason = "Can't have only negative clauses";
	return false;
    }
    if ((*m_query.begin())->m_tp == SCLT_EXCL) 
	iter_swap(m_query.begin(), itnotneg);

    // Walk the clause list translating each in turn and building the 
    // Xapian query tree
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++) {
	Xapian::Query nq;
	if (!(*it)->toNativeQuery(db, &nq, m_stemlang)) {
	    LOGERR(("SearchData::toNativeQuery: failed\n"));
	    m_reason = (*it)->getReason();
	    return false;
	}	    

	// If this structure is an AND list, must use AND_NOT for excl clauses.
	// Else this is an OR list, and there can't be excl clauses (checked by
	// addClause())
	Xapian::Query::op op;
	if (m_tp == SCLT_AND) {
	    op = (*it)->m_tp == SCLT_EXCL ? 
		Xapian::Query::OP_AND_NOT: Xapian::Query::OP_AND;
	} else {
	    op = Xapian::Query::OP_OR;
	}
	xq = xq.empty() ? nq : Xapian::Query(op, xq, nq);
    }

    // Add the file type filtering clause if any
    if (!m_filetypes.empty()) {
	vector<string> exptps;
	exptps.reserve(m_filetypes.size());
	// Expand categories
	RclConfig *cfg = RclConfig::getMainConfig();
	for (vector<string>::iterator it = m_filetypes.begin(); 
	     it != m_filetypes.end(); it++) {
	    if (cfg && cfg->isMimeCategory(*it)) {
		list<string>tps;
		cfg->getMimeCatTypes(*it, tps);
		exptps.insert(exptps.end(), tps.begin(), tps.end());
	    } else {
		exptps.push_back(*it);
	    }
	}
	    
	list<Xapian::Query> pqueries;
	Xapian::Query tq;
	for (vector<string>::iterator it = exptps.begin(); 
	     it != exptps.end(); it++) {
	    string term = "T" + *it;
	    LOGDEB(("Adding file type term: [%s]\n", term.c_str()));
	    tq = tq.empty() ? Xapian::Query(term) : 
		Xapian::Query(Xapian::Query::OP_OR, tq, Xapian::Query(term));
	}
	xq = xq.empty() ? tq : Xapian::Query(Xapian::Query::OP_FILTER, xq, tq);
    }

    *((Xapian::Query *)d) = xq;
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
    LOGDEB(("SearchData::erase\n"));
    m_tp = SCLT_AND;
    for (qlist_it_t it = m_query.begin(); it != m_query.end(); it++)
	delete *it;
    m_query.clear();
    m_filetypes.clear();
    m_topdir.erase();
    m_description.erase();
    m_reason.erase();
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

// Splitter callback for breaking a user query string into simple
// terms and phrases. 
class wsQData : public TextSplitCB {
 public:
    wsQData(const StopList &_stops) 
	: stops(_stops), alltermcount(0)
    {}
    vector<string> terms;
    bool takeword(const std::string &term, int , int, int) {
	alltermcount++;
	LOGDEB1(("wsQData::takeword: %s\n", term.c_str()));
	if (stops.hasStops() && stops.isStop(term)) {
	    LOGDEB1(("wsQData::takeword [%s] in stop list\n", term.c_str()));
	    return true;
	}
	terms.push_back(term);
	return true;
    }
    const StopList &stops;
    int alltermcount; // Count of terms including stopwords: this is
		      // for adjusting phrase/near slack
};

/** 
 * Translate a user compound string as may be entered in recoll's
 * search entry fields, ex: [term1 "a phrase" term3] into a xapian
 * query tree.
 * The object keeps track of the query terms and term groups while 
 * translating.
 */
class StringToXapianQ {
public:
    StringToXapianQ(Db& db, const string &stmlng, bool boostUser)
	: m_db(db), m_stemlang(stmlng), m_doBoostUserTerms(boostUser)
    { }

    bool processUserString(const string &iq,
			   const string &prefix,
			   string &ermsg,
			   list<Xapian::Query> &pqueries, 
			   const StopList &stops,
			   int slack = 0, bool useNear = false);

    bool getTerms(vector<string>& terms, 
		  vector<vector<string> >& groups) 
    {
	terms.insert(terms.end(), m_terms.begin(), m_terms.end());
	groups.insert(groups.end(), m_groups.begin(), m_groups.end());
	return true;
    }

private:
    void stripExpandTerm(bool dont, const string& term, list<string>& exp, 
		      string& sterm);

    Db&           m_db;
    const string& m_stemlang;
    bool          m_doBoostUserTerms;
    // Single terms and phrases resulting from breaking up text;
    vector<string>          m_terms;
    vector<vector<string> > m_groups; 
};

/** Unaccent and lowercase term, possibly expand stem and wildcards
 *
 * @param nostemexp don't perform stem expansion. This is mainly used to
 *   prevent stem expansion inside phrases (because the user probably
 *   does not expect it). This does NOT prevent wild card expansion.
 *   Other factors than nostemexp can prevent stem expansion: 
 *   a null stemlang, resulting from a global user preference, a
 *   capitalized term, or wildcard(s)
 * @param term input single word
 * @param exp output expansion list
 * @param sterm output lower-cased+unaccented version of the input term 
 *              (only for stem expansion, not wildcards)
 */
void StringToXapianQ::stripExpandTerm(bool nostemexp, 
				      const string& term, 
				      list<string>& exp,
				      string &sterm)
{
    LOGDEB2(("stripExpandTerm: term [%s] stemlang [%s] nostemexp %d\n", 
	     term.c_str(), m_stemlang.c_str(), nostemexp));
    sterm.erase();
    exp.clear();
    if (term.empty()) {
	return;
    }
    // term1 is lowercase and without diacritics
    string term1;
    dumb_string(term, term1);

    bool haswild = term.find_first_of("*?[") != string::npos;

    // No stemming if there are wildcards or prevented globally.
    if (haswild || m_stemlang.empty())
	nostemexp = true;

    if (!nostemexp) {
	// Check if the first letter is a majuscule in which
	// case we do not want to do stem expansion. Note that
	// the test is convoluted and possibly problematic

	string noacterm, noaclowterm;
	if (unacmaybefold(term, noacterm, "UTF-8", false) &&
	    unacmaybefold(noacterm, noaclowterm, "UTF-8", true)) {
	    Utf8Iter it1(noacterm);
	    Utf8Iter it2(noaclowterm);
	    if (*it1 != *it2)
		nostemexp = true;
	}
    }

    if (nostemexp && !haswild) {
	// Neither stemming nor wildcard expansion: just the word
	sterm = term1;
	exp.push_front(term1);
	exp.resize(1);
    } else {
	list<TermMatchEntry> l;
	if (haswild) {
	    m_db.termMatch(Rcl::Db::ET_WILD, m_stemlang, term1, l);
	} else {
	    sterm = term1;
	    m_db.termMatch(Rcl::Db::ET_STEM, m_stemlang, term1, l);
	}
	for (list<TermMatchEntry>::const_iterator it = l.begin(); 
	     it != l.end(); it++) {
	    exp.push_back(it->term);
	}
    }
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

/** Add prefix to all strings in list */
static void addPrefix(list<string>& terms, const string& prefix)
{
    if (prefix.empty())
	return;
    for (list<string>::iterator it = terms.begin(); it != terms.end(); it++)
	it->insert(0, prefix);
}

/** 
 * Turn string into list of xapian queries. There is little
 * interpretation done on the string (no +term -term or filename:term
 * stuff). We just separate words and phrases, and interpret
 * capitalized terms as wanting no stem expansion. 
 * The final list contains one query for each term or phrase
 *   - Elements corresponding to a stem-expanded part are an OP_OR
 *     composition of the stem-expanded terms (or a single term query).
 *   - Elements corresponding to a phrase are an OP_PHRASE composition of the
 *     phrase terms (no stem expansion in this case)
 * @return the subquery count (either or'd stem-expanded terms or phrase word
 *   count)
 */
bool StringToXapianQ::processUserString(const string &iq,
					const string &prefix,
					string &ermsg,
					list<Xapian::Query> &pqueries,
					const StopList& stops,
					int slack, bool useNear
					)
{
    LOGDEB(("StringToXapianQ:: query string: [%s]\n", iq.c_str()));
    ermsg.erase();
    m_terms.clear();
    m_groups.clear();

    // Split input into user-level words and double-quoted phrases:
    // word1 word2 "this is a phrase". The text splitter may still
    // decide that the resulting "words" are really phrases, this
    // depends on separators: [paul@dom.net] would still be a word
    // (span), but [about:me] will probably be handled as a phrase.
    list<string> phrases;
    TextSplit::stringToStrings(iq, phrases);

    // Process each element: textsplit into terms, handle stem/wildcard 
    // expansion and transform into an appropriate Xapian::Query
    try {
	for (list<string>::iterator it = phrases.begin(); 
	     it != phrases.end(); it++) {
	    LOGDEB(("strToXapianQ: phrase or word: [%s]\n", it->c_str()));

	    // If there are multiple spans in this element, including
	    // at least one composite, we have to do something
	    // else a phrase query including a span would fail. 
	    // Ex: "term0@term1 term2" is onlyspans-split as:
	    //   0 term0@term1             0   12
	    //   2 term2                  13   18
	    // The position of term1 is 2, not 1, so a phrase search
	    // would fail.
	    // We used to do  word split, searching for 
	    // "term0 term01 term1" instead, which may have worse 
	    // performance, but will succeed.
	    // We now adjust the phrase/near slack by the term count
	    // difference (this is mainly better for cjk where this is a very
	    // common occurrence because of the ngrams thing.
	    wsQData splitDataS(stops), splitDataW(stops);
	    TextSplit splitterS(&splitDataS, 
				TextSplit::Flags(TextSplit::TXTS_ONLYSPANS | 
						 TextSplit::TXTS_KEEPWILD));
	    splitterS.text_to_words(*it);
	    TextSplit splitterW(&splitDataW, 
				TextSplit::Flags(TextSplit::TXTS_NOSPANS | 
						 TextSplit::TXTS_KEEPWILD));
	    splitterW.text_to_words(*it);
	    wsQData *splitData = &splitDataS;
	    if (splitDataS.terms.size() > 1 && 
		splitDataS.terms.size() != splitDataW.terms.size()) {
		slack += splitDataW.terms.size() - splitDataS.terms.size();
		// used to: splitData = &splitDataW;
	    }

	    LOGDEB(("strToXapianQ: splitter term count: %d\n", 
		     splitData->terms.size()));
	    switch (splitData->terms.size()) {
	    case 0: continue;// ??
	    case 1: 
		// Just a term. Still may be expanded (by stem or
		// wildcard) to an OR list.
		{
		    string term = splitData->terms.front();
		    list<string> exp;  
		    string sterm; // dumb version of user term
		    stripExpandTerm(false, term, exp, sterm);
		    m_terms.insert(m_terms.end(), exp.begin(), exp.end());
		    // Push either term or OR of stem-expanded set
		    addPrefix(exp, prefix);
		    Xapian::Query xq(Xapian::Query::OP_OR, 
				     exp.begin(), exp.end());

		    // If sterm is not null, give a relevance boost to
		    // the original term. We do this even if no
		    // expansion occurred (else the non-expanded terms
		    // in a term list would end-up with even less
		    // wqf). This does not happen if there are
		    // wildcards anywhere in the search.
		    if (m_doBoostUserTerms && !sterm.empty()) {
			xq = Xapian::Query(Xapian::Query::OP_OR, xq, 
					   Xapian::Query(prefix+sterm, 10));
		    }
		    pqueries.push_back(xq);
		}
		break;

	    default:
		// Element had several terms: transform into a PHRASE
		// or NEAR xapian query, the elements of which can
		// themselves be OR queries if the terms get expanded
		// by stemming or wildcards (we don't do stemming for
		// PHRASE though)
		Xapian::Query::op op = useNear ? Xapian::Query::OP_NEAR : 
		Xapian::Query::OP_PHRASE;
		list<Xapian::Query> orqueries;
		bool hadmultiple = false;
		vector<vector<string> >groups;
		for (vector<string>::iterator it = splitData->terms.begin();
		     it != splitData->terms.end(); it++) {
		    // Some version of xapian will accept only one OR clause
		    // inside NEAR, all others must be leafs
		    bool nostemexp = 
			op == Xapian::Query::OP_PHRASE || hadmultiple;

		    string sterm;
		    list<string>exp;
		    stripExpandTerm(nostemexp, *it, exp, sterm);
		    groups.push_back(vector<string>(exp.begin(), exp.end()));
		    addPrefix(exp, prefix);
		    orqueries.push_back(Xapian::Query(Xapian::Query::OP_OR, 
						      exp.begin(), exp.end()));
#ifdef XAPIAN_NEAR_EXPAND_SINGLE_BUF
		    if (exp.size() > 1) 
			hadmultiple = true;
#endif
		}

		pqueries.push_back(Xapian::Query(op,
						 orqueries.begin(),
						 orqueries.end(),
						 splitData->alltermcount 
						 + slack));
		// Add NEAR/PHRASE groups to the highlighting data. Must
		// push all combinations
		vector<vector<string> > allcombs;
		vector<string> comb;
		multiply_groups(groups.begin(), groups.end(), comb, allcombs);
		m_groups.insert(m_groups.end(), allcombs.begin(), 
				allcombs.end());
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

static const string nullstemlang;

// Translate a simple OR, AND, or EXCL search clause. 
bool SearchDataClauseSimple::toNativeQuery(Rcl::Db &db, void *p, 
					   const string& stemlang)
{
    const string& l_stemlang = (m_modifiers&SDCM_NOSTEMMING)? nullstemlang:
	stemlang;

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
    string prefix;
    if (!m_field.empty())
	db.fieldToPrefix(m_field, prefix);
    list<Xapian::Query> pqueries;

    // We normally boost the original term in the stem expansion list. Don't
    // do it if there are wildcards anywhere, this would skew the results.
    bool doBoostUserTerm = 
	(m_parentSearch && !m_parentSearch->haveWildCards()) || 
	(m_parentSearch == 0 && !m_haveWildCards);

    StringToXapianQ tr(db, l_stemlang, doBoostUserTerm);
    if (!tr.processUserString(m_text, prefix, m_reason, pqueries, 
			      db.getStopList()))
	return false;
    if (pqueries.empty()) {
	LOGERR(("SearchDataClauseSimple: resolved to null query\n"));
	return false;
    }
    tr.getTerms(m_terms, m_groups);
    *qp = Xapian::Query(op, pqueries.begin(), pqueries.end());
    return true;
}

// Translate a FILENAME search clause. 
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
	names.splice(names.end(), more);
    }
    // Build a query out of the matching file name terms.
    *qp = Xapian::Query(Xapian::Query::OP_OR, names.begin(), names.end());
    return true;
}

// Translate NEAR or PHRASE clause. 
bool SearchDataClauseDist::toNativeQuery(Rcl::Db &db, void *p, 
					 const string& stemlang)
{
    const string& l_stemlang = (m_modifiers&SDCM_NOSTEMMING)? nullstemlang:
	stemlang;
    LOGDEB(("SearchDataClauseDist::toNativeQuery\n"));
    m_terms.clear();
    m_groups.clear();

    Xapian::Query *qp = (Xapian::Query *)p;
    *qp = Xapian::Query();

    list<Xapian::Query> pqueries;
    Xapian::Query nq;

    string prefix;
    if (!m_field.empty())
	db.fieldToPrefix(m_field, prefix);

    // We normally boost the original term in the stem expansion list. Don't
    // do it if there are wildcards anywhere, this would skew the results.
    bool doBoostUserTerm = 
	(m_parentSearch && !m_parentSearch->haveWildCards()) || 
	(m_parentSearch == 0 && !m_haveWildCards);

    // We produce a single phrase out of the user entry (there should be
    // no dquotes in there), then use stringToXapianQueries() to
    // lowercase and simplify the phrase terms etc. This will result
    // into a single (complex) Xapian::Query.
    if (m_text.find_first_of("\"") != string::npos) {
	LOGDEB(("Double quotes inside phrase/near field\n"));
	return false;
    }
    string s = string("\"") + m_text + string("\"");
    bool useNear = (m_tp == SCLT_NEAR);
    StringToXapianQ tr(db, l_stemlang, doBoostUserTerm);
    if (!tr.processUserString(s, prefix, m_reason, pqueries, db.getStopList(),
			      m_slack, useNear))
	return false;
    if (pqueries.empty()) {
	LOGERR(("SearchDataClauseDist: resolved to null query\n"));
	return true;
    }
    tr.getTerms(m_terms, m_groups);
    *qp = *pqueries.begin();
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

} // Namespace Rcl
