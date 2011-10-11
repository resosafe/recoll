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
#ifndef _SEARCHDATA_H_INCLUDED_
#define _SEARCHDATA_H_INCLUDED_

/** 
 * Structures to hold data coming almost directly from the gui
 * and handle its translation to Xapian queries.
 * This is not generic code, it reflects the choices made for the user 
 * interface, and it also knows some specific of recoll's usage of Xapian 
 * (ie: term prefixes)
 */
#include <string>
#include <vector>

#include "rcldb.h"
#include "refcntr.h"
#include "smallut.h"

class RclConfig;

#ifndef NO_NAMESPACES
using std::vector;
using std::string;
namespace Rcl {
#endif // NO_NAMESPACES

/** Search clause types */
enum SClType {
    SCLT_AND, 
    SCLT_OR, SCLT_EXCL, SCLT_FILENAME, SCLT_PHRASE, SCLT_NEAR,
    SCLT_SUB
};

class SearchDataClause;

/** 
  Data structure representing a Recoll user query, for translation
  into a Xapian query tree. This could probably better called a 'question'.

  This is a list of search clauses combined through either OR or AND.

  Clauses either reflect user entry in a query field: some text, a
  clause type (AND/OR/NEAR etc.), possibly a distance, or points to
  another SearchData representing a subquery.

  The content of each clause when added may not be fully parsed yet
  (may come directly from a gui field). It will be parsed and may be
  translated to several queries in the Xapian sense, for exemple
  several terms and phrases as would result from 
  ["this is a phrase"  term1 term2] . 

  This is why the clauses also have an AND/OR/... type. 

  A phrase clause could be added either explicitly or using double quotes:
  {SCLT_PHRASE, [this is a phrase]} or as {SCLT_XXX, ["this is a phrase"]}

 */
class SearchData {
public:
    SearchData(SClType tp) 
        : m_tp(tp), m_topdirexcl(false), m_topdirweight(1.0), 
	  m_haveDates(false), m_haveWildCards(false) 
    {
	if (m_tp != SCLT_OR && m_tp != SCLT_AND) 
	    m_tp = SCLT_OR;
    }
    ~SearchData() {erase();}

    /** Make pristine */
    void erase();

    /** Is there anything but a file name search in here ? */
    bool fileNameOnly();

    /** Do we have wildcards anywhere apart from filename searches ? */
    bool haveWildCards() {return m_haveWildCards;}

    /** Translate to Xapian query. rcldb knows about the void*  */
    bool toNativeQuery(Rcl::Db &db, void *);

    /** We become the owner of cl and will delete it */
    bool addClause(SearchDataClause *cl);

    /** If this is a simple query (one field only, no distance clauses),
	add phrase made of query terms to query, so that docs containing the
	user terms in order will have higher relevance. This must be called 
	before toNativeQuery().
    */
    bool maybeAddAutoPhrase();

    /** Set/get top subdirectory for filtering results */
    void setTopdir(const string& t, bool excl = false, float w = 1.0) 
    {
	m_topdir = t;
	m_topdirexcl = excl;
	m_topdirweight = w;
    }

    /** Set date span for filtering results */
    void setDateSpan(DateInterval *dip) {m_dates = *dip; m_haveDates = true;}

    /** Add file type for filtering results */
    void addFiletype(const string& ft) {m_filetypes.push_back(ft);}
    /** Add file type to not wanted list */
    void remFiletype(const string& ft) {m_nfiletypes.push_back(ft);}

    void setStemlang(const string& lang = "english") {m_stemlang = lang;}

    /** Retrieve error description */
    string getReason() {return m_reason;}

    /** Get terms and phrase/near groups. Used in the GUI for highlighting 
     * The groups and gslks vectors are parallel and hold the phrases/near
     * string groups and their associated slacks (distance in excess of group
     * size)
     */
    bool getTerms(vector<string>& terms, 
		  vector<vector<string> >& groups, vector<int>& gslks) const;
    /** Get user-input terms (before expansion etc.) */
    void getUTerms(vector<string>& terms) const;

    /** 
     * Get/set the description field which is retrieved from xapian after
     * initializing the query. It is stored here for usage in the GUI.
     */
    string getDescription() {return m_description;}
    void setDescription(const string& d) {m_description = d;}

private:
    SClType                   m_tp; // Only SCLT_AND or SCLT_OR here
    vector<SearchDataClause*> m_query;
    vector<string>            m_filetypes; // Restrict to filetypes if set.
    vector<string>            m_nfiletypes; // Unwanted file types
    string                    m_topdir; // Restrict to subtree.
    bool                      m_topdirexcl; // Invert meaning
    float                     m_topdirweight; // affect weight instead of filter
    bool                      m_haveDates;
    DateInterval              m_dates; // Restrict to date interval
    // Printable expanded version of the complete query, retrieved/set
    // from rcldb after the Xapian::setQuery() call
    string m_description; 
    string m_reason;
    bool   m_haveWildCards;
    string m_stemlang;
    bool expandFileTypes(RclConfig *cfg, vector<string>& exptps);
    /* Copyconst and assignment private and forbidden */
    SearchData(const SearchData &) {}
    SearchData& operator=(const SearchData&) {return *this;};
};

class SearchDataClause {
public:
    enum Modifier {SDCM_NONE=0, SDCM_NOSTEMMING=1, SDCM_ANCHORSTART=2,
		   SDCM_ANCHOREND=4};

    SearchDataClause(SClType tp) 
	: m_tp(tp), m_parentSearch(0), m_haveWildCards(0), 
	  m_modifiers(SDCM_NONE), m_weight(1.0)
    {}
    virtual ~SearchDataClause() {}
    virtual bool toNativeQuery(Rcl::Db &db, void *, const string&) = 0;
    bool isFileName() const {return m_tp == SCLT_FILENAME ? true: false;}
    virtual string getReason() const {return m_reason;}
    virtual bool getTerms(vector<string>&, vector<vector<string> >&,
			  vector<int>&) const = 0;
    virtual void getUTerms(vector<string>&) const = 0;

    SClType getTp() {return m_tp;}
    void setParent(SearchData *p) {m_parentSearch = p;}
    virtual void setModifiers(Modifier mod) {m_modifiers = mod;}
    virtual int getModifiers() {return m_modifiers;}
    virtual void addModifier(Modifier mod) {
	int imod = getModifiers();
	imod |= mod;
	setModifiers(Modifier(imod));
    }
    virtual void setWeight(float w) {m_weight = w;}
    friend class SearchData;

protected:
    string      m_reason;
    SClType     m_tp;
    SearchData *m_parentSearch;
    bool        m_haveWildCards;
    Modifier    m_modifiers;
    float       m_weight;
private:
    SearchDataClause(const SearchDataClause&) {}
    SearchDataClause& operator=(const SearchDataClause&) {
	return *this;
    }
};
    
/**
 * "Simple" data clause with user-entered query text. This can include 
 * multiple phrases and words, but no specified distance.
 */
class SearchDataClauseSimple : public SearchDataClause {
public:
    SearchDataClauseSimple(SClType tp, const string& txt, 
			   const string& fld = string())
	: SearchDataClause(tp), m_text(txt), m_field(fld), m_slack(0) {
	m_haveWildCards = (txt.find_first_of("*?[") != string::npos);
    }

    virtual ~SearchDataClauseSimple() {}

    /** Translate to Xapian query */
    virtual bool toNativeQuery(Rcl::Db &db, void *, const string& stemlang);

    /** Retrieve query terms and term groups. This is used for highlighting */
    virtual bool getTerms(vector<string>& terms, /* Single terms */
			  vector<vector<string> >& groups, /* Prox grps */
			  vector<int>& gslks) const        /* Prox slacks */
    {
	terms.insert(terms.end(), m_terms.begin(), m_terms.end());
	groups.insert(groups.end(), m_groups.begin(), m_groups.end());
	gslks.insert(gslks.end(), m_groups.size(), m_slack);
	return true;
    }
    virtual void getUTerms(vector<string>& terms) const
    {
	terms.insert(terms.end(), m_uterms.begin(), m_uterms.end());
    }
    virtual const string& gettext() {return m_text;}
    virtual const string& getfield() {return m_field;}
protected:
    string  m_text;  // Raw user entry text.
    string  m_field; // Field specification if any
    // Single terms and phrases resulting from breaking up m_text;
    // valid after toNativeQuery() call
    vector<string>          m_terms;
    vector<vector<string> > m_groups;
    // User terms before expansion
    vector<string>          m_uterms;
    // Declare m_slack here. Always 0, but allows getTerms to work for
    // SearchDataClauseDist
    int m_slack;
};

/** Filename search clause. This is special because term expansion is only
 * performed against the XSFN terms (it's performed against the main index
 * for all other fields). Else we could just use a "filename:" field
 * This doesn't really make sense (either). I think we could either expand
 * filenames against all terms and then select the XSFN ones, or always perform
 * expansion only against the field's terms ? Anyway this doesn't hurt
 * much either. 
 *
 * There is a big advantage though in expanding only against the
 * field, especially for file names, because this makes searches for
 * "*xx" much faster (no need to scan the whole main index).
 */
class SearchDataClauseFilename : public SearchDataClauseSimple {
public:
    SearchDataClauseFilename(const string& txt)
	: SearchDataClauseSimple(SCLT_FILENAME, txt) {
	// File name searches don't count when looking for wild cards.
	m_haveWildCards = false;
    }
    virtual ~SearchDataClauseFilename() {}
    virtual bool toNativeQuery(Rcl::Db &db, void *, const string& stemlang);
};

/** 
 * A clause coming from a NEAR or PHRASE entry field. There is only one 
 * string group, and a specified distance, which applies to it.
 */
class SearchDataClauseDist : public SearchDataClauseSimple {
public:
    SearchDataClauseDist(SClType tp, const string& txt, int slack, 
			 const string& fld = string())
	: SearchDataClauseSimple(tp, txt, fld) {m_slack = slack;}
    virtual ~SearchDataClauseDist() {}

    virtual bool toNativeQuery(Rcl::Db &db, void *, const string& stemlang);

    // m_slack is declared in SearchDataClauseSimple
};

/** Subquery */
class SearchDataClauseSub : public SearchDataClause {
public:
    // We take charge of the SearchData * and will delete it.
    SearchDataClauseSub(SClType tp, RefCntr<SearchData> sub) 
	: SearchDataClause(tp), m_sub(sub) {}
    virtual ~SearchDataClauseSub() {}
    virtual bool toNativeQuery(Rcl::Db &db, void *, const string& stemlang);
    virtual bool getTerms(vector<string>&, vector<vector<string> >&,
			  vector<int>&) const;
    virtual void getUTerms(vector<string>&) const;
protected:
    RefCntr<SearchData> m_sub;
};

} // Namespace Rcl
#endif /* _SEARCHDATA_H_INCLUDED_ */
