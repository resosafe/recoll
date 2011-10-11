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
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <fnmatch.h>
#include <regex.h>
#include <math.h>
#include <time.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "xapian.h"

#include "rclconfig.h"
#include "rcldb.h"
#include "rcldb_p.h"
#include "stemdb.h"
#include "textsplit.h"
#include "transcode.h"
#include "unacpp.h"
#include "conftree.h"
#include "debuglog.h"
#include "pathut.h"
#include "smallut.h"
#include "utf8iter.h"
#include "searchdata.h"
#include "rclquery.h"
#include "rclquery_p.h"
#include "md5.h"
#include "rclversion.h"
#include "cancelcheck.h"
#include "ptmutex.h"

#ifndef MAX
#define MAX(A,B) (A>B?A:B)
#endif
#ifndef MIN
#define MIN(A,B) (A<B?A:B)
#endif

// Recoll index format version is stored in user metadata. When this change,
// we can't open the db and will have to reindex.
static const string cstr_RCL_IDX_VERSION_KEY("RCL_IDX_VERSION_KEY");
static const string cstr_RCL_IDX_VERSION("1");

// This is the word position offset at which we index the body text
// (abstract, keywords, etc.. are stored before this)
static const unsigned int baseTextPosition = 100000;

#ifndef NO_NAMESPACES
namespace Rcl {
#endif

const string pathelt_prefix = "XP";
const string start_of_field_term = "XXST";
const string end_of_field_term = "XXND";

// This is used as a marker inside the abstract frag lists, but
// normally doesn't remain in final output (which is built with a
// custom sep. by our caller).
static const string cstr_ellipsis("...");

string version_string(){
    return string("Recoll ") + string(rclversionstr) + string(" + Xapian ") +
        string(Xapian::version_string());
}

// Synthetic abstract marker (to discriminate from abstract actually
// found in document)
static const string cstr_syntAbs("?!#@");

// Only ONE field name inside the index data record differs from the
// Rcl::Doc ones: caption<->title, for a remnant of compatibility with
// omega

// Static/Default table for field->prefix/weight translation. 
// This is logically const after initialization. Can't use a
// static object to init this as the static std::string objects may
// not be ready.
//
// This map is searched if a match is not found in the dynamic
// "fields" configuration (cf: Db::fieldToTraits()), meaning that the
// entries can be overriden in the configuration, but not
// suppressed. 

static map<string, FieldTraits> fldToTraits;
static PTMutexInit o_fldToTraits_mutex;

static void initFldToTraits() 
{
    PTMutexLocker locker(o_fldToTraits_mutex);
    // As we perform non-locked testing of initialization, check again with
    // the lock held
    if (fldToTraits.size())
	return;

    // Can't remember why "abstract" is indexed without a prefix
    // (result: it's indexed twice actually). Maybe I'll dare change
    // this one day
    fldToTraits[Doc::keyabs] = FieldTraits();

    fldToTraits["ext"] = FieldTraits("XE");
    fldToTraits[Doc::keyfn] = FieldTraits("XSFN");

    fldToTraits[cstr_caption] = FieldTraits("S");
    fldToTraits[Doc::keytt] = FieldTraits("S");
    fldToTraits["subject"] = FieldTraits("S");

    fldToTraits[Doc::keyau] = FieldTraits("A");
    fldToTraits["creator"] = FieldTraits("A");
    fldToTraits["from"] = FieldTraits("A");

    fldToTraits[Doc::keykw] = FieldTraits("K");
    fldToTraits["keyword"] = FieldTraits("K");
    fldToTraits["tag"] = FieldTraits("K");
    fldToTraits["tags"] = FieldTraits("K");

    fldToTraits["xapyear"] = FieldTraits("Y");
    fldToTraits["xapyearmon"] = FieldTraits("M");
    fldToTraits["xapdate"] = FieldTraits("D");
}

// Compute the unique term used to link documents to their origin. 
// "Q" + external udi
static inline string make_uniterm(const string& udi)
{
    string uniterm("Q");
    uniterm.append(udi);
    return uniterm;
}
// Compute parent term used to link documents to their parent document (if any)
// "F" + parent external udi
static inline string make_parentterm(const string& udi)
{
    // I prefer to be in possible conflict with omega than with
    // user-defined fields (Xxxx) that we also allow. "F" is currently
    // not used by omega (2008-07)
    string pterm("F");
    pterm.append(udi);
    return pterm;
}

/* See comment in class declaration: return all subdocuments of a
 * document given by its unique id. 
*/
bool Db::Native::subDocs(const string &udi, vector<Xapian::docid>& docids) 
{
    LOGDEB2(("subDocs: [%s]\n", uniterm.c_str()));
    string pterm = make_parentterm(udi);

    XAPTRY(docids.clear();
           docids.insert(docids.begin(), xrdb.postlist_begin(pterm), 
                         xrdb.postlist_end(pterm)),
           xrdb, m_rcldb->m_reason);

    if (!m_rcldb->m_reason.empty()) {
        LOGERR(("Rcl::Db::subDocs: %s\n", m_rcldb->m_reason.c_str()));
        return false;
    } else {
        LOGDEB0(("Db::Native::subDocs: returning %d ids\n", docids.size()));
        return true;
    }
}

// Turn data record from db into document fields
bool Db::Native::dbDataToRclDoc(Xapian::docid docid, std::string &data, 
				Doc &doc)
{
    LOGDEB2(("Db::dbDataToRclDoc: data:\n%s\n", data.c_str()));
    ConfSimple parms(data);
    if (!parms.ok())
	return false;
    parms.get(Doc::keyurl, doc.url);
    parms.get(Doc::keytp, doc.mimetype);
    parms.get(Doc::keyfmt, doc.fmtime);
    parms.get(Doc::keydmt, doc.dmtime);
    parms.get(Doc::keyoc, doc.origcharset);
    parms.get(cstr_caption, doc.meta[Doc::keytt]);
    parms.get(Doc::keykw, doc.meta[Doc::keykw]);
    parms.get(Doc::keyabs, doc.meta[Doc::keyabs]);
    // Possibly remove synthetic abstract indicator (if it's there, we
    // used to index the beginning of the text as abstract).
    doc.syntabs = false;
    if (doc.meta[Doc::keyabs].find(cstr_syntAbs) == 0) {
	doc.meta[Doc::keyabs] = doc.meta[Doc::keyabs].substr(cstr_syntAbs.length());
	doc.syntabs = true;
    }
    parms.get(Doc::keyipt, doc.ipath);
    parms.get(Doc::keyfs, doc.fbytes);
    parms.get(Doc::keyds, doc.dbytes);
    parms.get(Doc::keysig, doc.sig);
    doc.xdocid = docid;

    // Other, not predefined meta fields:
    list<string> keys = parms.getNames(string());
    for (list<string>::const_iterator it = keys.begin(); 
	 it != keys.end(); it++) {
	if (doc.meta.find(*it) == doc.meta.end()) 
	    parms.get(*it, doc.meta[*it]);
    }
    doc.meta[Doc::keymt] = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    return true;
}

// Remove prefixes (caps) from a list of terms.
static void noPrefixList(const list<string>& in, list<string>& out) 
{
    for (list<string>::const_iterator qit = in.begin(); 
	 qit != in.end(); qit++) {
	if ('A' <= qit->at(0) && qit->at(0) <= 'Z') {
	    string term = *qit;
	    while (term.length() && 'A' <= term.at(0) && term.at(0) <= 'Z')
		term.erase(0, 1);
	    if (term.length())
		out.push_back(term);
	    continue;
	} else {
	    out.push_back(*qit);
	}
    }
}

//#define DEBUGABSTRACT  1
#ifdef DEBUGABSTRACT
#define LOGABS LOGDEB
#else
#define LOGABS LOGDEB2
#endif
#if 0
static void listList(const string& what, const list<string>&l)
{
    string a;
    for (list<string>::const_iterator it = l.begin(); it != l.end(); it++) {
        a = a + *it + " ";
    }
    LOGDEB(("%s: %s\n", what.c_str(), a.c_str()));
}
#endif

// Build a document abstract by extracting text chunks around the query terms
// This uses the db termlists, not the original document.
//
// DatabaseModified and other general exceptions are catched and
// possibly retried by our caller
vector<string> Db::Native::makeAbstract(Xapian::docid docid, Query *query)
{
    Chrono chron;
    LOGDEB2(("makeAbstract:%d: maxlen %d wWidth %d\n", chron.ms(),
	     m_rcldb->m_synthAbsLen, m_rcldb->m_synthAbsWordCtxLen));

    list<string> terms;

    {
        list<string> iterms;
        query->getMatchTerms(docid, iterms);
        noPrefixList(iterms, terms);
        if (terms.empty()) {
            LOGDEB(("makeAbstract::Empty term list\n"));
            return vector<string>();
        }
    }
//    listList("Match terms: ", terms);

    // Retrieve db-wide frequencies for the query terms (we do this once per
    // query, using all the query terms, not only the document match terms)
    if (query->m_nq->termfreqs.empty()) {
        list<string> iqterms, qterms;
        query->getQueryTerms(iqterms);
        noPrefixList(iqterms, qterms);
//        listList("Query terms: ", qterms);
	double doccnt = xrdb.get_doccount();
	if (doccnt == 0) doccnt = 1;
	for (list<string>::const_iterator qit = qterms.begin(); 
	     qit != qterms.end(); qit++) {
	    query->m_nq->termfreqs[*qit] = xrdb.get_termfreq(*qit) / doccnt;
	    LOGABS(("makeAbstract: [%s] db freq %.1e\n", qit->c_str(), 
		    query->m_nq->termfreqs[*qit]));
	}
	LOGABS(("makeAbstract:%d: got termfreqs\n", chron.ms()));
    }

    // Compute a term quality coefficient by retrieving the term
    // Within Document Frequencies and multiplying by overal term
    // frequency, then using log-based thresholds. We are going to try
    // and show text around the less common search terms.
    map<string, double> termQcoefs;
    double totalweight = 0;
    double doclen = xrdb.get_doclength(docid);
    if (doclen == 0) doclen = 1;
    for (list<string>::const_iterator qit = terms.begin(); 
	 qit != terms.end(); qit++) {
	Xapian::TermIterator term = xrdb.termlist_begin(docid);
	term.skip_to(*qit);
	if (term != xrdb.termlist_end(docid) && *term == *qit) {
	    double q = (term.get_wdf() / doclen) * query->m_nq->termfreqs[*qit];
	    q = -log10(q);
	    if (q < 3) {
		q = 0.05;
	    } else if (q < 4) {
		q = 0.3;
	    } else if (q < 5) {
		q = 0.7;
	    } else if (q < 6) {
		q = 0.8;
	    } else {
		q = 1;
	    }
	    termQcoefs[*qit] = q;
	    totalweight += q;
	}
    }    
    LOGABS(("makeAbstract:%d: computed Qcoefs.\n", chron.ms()));

    // Build a sorted by quality term list.
    multimap<double, string> byQ;
    for (list<string>::const_iterator qit = terms.begin(); 
	 qit != terms.end(); qit++) {
	if (termQcoefs.find(*qit) != termQcoefs.end())
	    byQ.insert(pair<double,string>(termQcoefs[*qit], *qit));
    }

#ifdef DEBUGABSTRACT
    for (multimap<double, string>::reverse_iterator qit = byQ.rbegin(); 
	 qit != byQ.rend(); qit++) {
	LOGDEB(("%.1e->[%s]\n", qit->first, qit->second.c_str()));
    }
#endif


    // For each of the query terms, ask xapian for its positions list
    // in the document. For each position entry, remember it in
    // qtermposs and insert it and its neighbours in the set of
    // 'interesting' positions

    // The terms 'array' that we partially populate with the document
    // terms, at their positions around the search terms positions:
    map<unsigned int, string> sparseDoc;

    // All the chosen query term positions. 
    vector<unsigned int> qtermposs; 

    // Limit the total number of slots we populate. The 7 is taken as
    // average word size. It was a mistake to have the user max
    // abstract size parameter in characters, we basically only deal
    // with words. We used to limit the character size at the end, but
    // this damaged our careful selection of terms
    const unsigned int maxtotaloccs = 
	m_rcldb->m_synthAbsLen /(7 * (m_rcldb->m_synthAbsWordCtxLen+1));
    LOGABS(("makeAbstract:%d: mxttloccs %d\n", chron.ms(), maxtotaloccs));
    // This can't happen, but would crash us
    if (totalweight == 0.0) {
	LOGERR(("makeAbstract: 0 totalweight!\n"));
	return vector<string>();
    }

    // This is used to mark positions overlapped by a multi-word match term
    const string occupiedmarker("?");

    // Let's go populate
    for (multimap<double, string>::reverse_iterator qit = byQ.rbegin(); 
	 qit != byQ.rend(); qit++) {
	string qterm = qit->second;
	unsigned int maxoccs;
	if (byQ.size() == 1) {
	    maxoccs = maxtotaloccs;
	} else {
	    // We give more slots to the better terms
	    float q = qit->first / totalweight;
	    maxoccs = int(ceil(maxtotaloccs * q));
	    LOGABS(("makeAbstract: [%s] %d max occs (coef %.2f)\n", 
		    qterm.c_str(), maxoccs, q));
	}

	// The match term may span several words
	int qtrmwrdcnt = TextSplit::countWords(qterm, TextSplit::TXTS_NOSPANS);

	Xapian::PositionIterator pos;
	// There may be query terms not in this doc. This raises an
	// exception when requesting the position list, we catch it.
	string emptys;
	try {
	    unsigned int occurrences = 0;
	    for (pos = xrdb.positionlist_begin(docid, qterm); 
		 pos != xrdb.positionlist_end(docid, qterm); pos++) {
		int ipos = *pos;
		if (ipos < int(baseTextPosition)) // Not in text body
		    continue;
		LOGABS(("makeAbstract: [%s] at %d occurrences %d maxoccs %d\n",
			qterm.c_str(), ipos, occurrences, maxoccs));
		// Remember the term position
		qtermposs.push_back(ipos);

		// Add adjacent slots to the set to populate at next
		// step by inserting empty strings. Special provisions
		// for adding ellipsis and for positions overlapped by
		// the match term.
		unsigned int sta = MAX(0, ipos-m_rcldb->m_synthAbsWordCtxLen);
		unsigned int sto = ipos + qtrmwrdcnt-1 + 
		    m_rcldb->m_synthAbsWordCtxLen;
		for (unsigned int ii = sta; ii <= sto;  ii++) {
		    if (ii == (unsigned int)ipos) {
			sparseDoc[ii] = qterm;
		    } else if (ii > (unsigned int)ipos && 
			       ii < (unsigned int)ipos + qtrmwrdcnt) {
			sparseDoc[ii] = occupiedmarker;
		    } else if (!sparseDoc[ii].compare(cstr_ellipsis)) {
			// For an empty slot, the test has a side
			// effect of inserting an empty string which
			// is what we want
			sparseDoc[ii] = emptys;
		    }
		}
		// Add ellipsis at the end. This may be replaced later by
		// an overlapping extract. Take care not to replace an
		// empty string here, we really want an empty slot,
		// use find()
		if (sparseDoc.find(sto+1) == sparseDoc.end()) {
		    sparseDoc[sto+1] = cstr_ellipsis;
		}

		// Limit to allocated occurences and total size
		if (++occurrences >= maxoccs || 
		    qtermposs.size() >= maxtotaloccs)
		    break;
	    }
	} catch (...) {
	    // Term does not occur. No problem.
	}
	if (qtermposs.size() >= maxtotaloccs)
	    break;
    }
    LOGABS(("makeAbstract:%d:chosen number of positions %d\n", 
	    chron.millis(), qtermposs.size()));

    // This can happen if there are term occurences in the keywords
    // etc. but not elsewhere ?
    if (qtermposs.size() == 0) {
	LOGDEB1(("makeAbstract: no occurrences\n"));
	return vector<string>();
    }

    // Walk all document's terms position lists and populate slots
    // around the query terms. We arbitrarily truncate the list to
    // avoid taking forever. If we do cutoff, the abstract may be
    // inconsistant (missing words, potentially altering meaning),
    // which is bad.
    { 
	Xapian::TermIterator term;
	int cutoff = 500 * 1000;

	for (term = xrdb.termlist_begin(docid);
	     term != xrdb.termlist_end(docid); term++) {
	    // Ignore prefixed terms
	    if ('A' <= (*term).at(0) && (*term).at(0) <= 'Z')
		continue;
	    if (cutoff-- < 0) {
		LOGDEB0(("makeAbstract: max term count cutoff\n"));
		break;
	    }

	    Xapian::PositionIterator pos;
	    for (pos = xrdb.positionlist_begin(docid, *term); 
		 pos != xrdb.positionlist_end(docid, *term); pos++) {
		if (cutoff-- < 0) {
		    LOGDEB0(("makeAbstract: max term count cutoff\n"));
		    break;
		}
		map<unsigned int, string>::iterator vit;
		if ((vit=sparseDoc.find(*pos)) != sparseDoc.end()) {
		    // Don't replace a term: the terms list is in
		    // alphabetic order, and we may have several terms
		    // at the same position, we want to keep only the
		    // first one (ie: dockes and dockes@wanadoo.fr)
		    if (vit->second.empty()) {
			LOGABS(("makeAbstract: populating: [%s] at %d\n", 
				(*term).c_str(), *pos));
			sparseDoc[*pos] = *term;
		    }
		}
	    }
	}
    }

#if 0
    // Debug only: output the full term[position] vector
    bool epty = false;
    int ipos = 0;
    for (map<unsigned int, string>::iterator it = sparseDoc.begin(); 
	 it != sparseDoc.end();
	 it++, ipos++) {
	if (it->empty()) {
	    if (!epty)
		LOGDEB(("makeAbstract:vec[%d]: [%s]\n", ipos, it->c_str()));
	    epty=true;
	} else {
	    epty = false;
	    LOGDEB(("makeAbstract:vec[%d]: [%s]\n", ipos, it->c_str()));
	}
    }
#endif

    LOGABS(("makeAbstract:%d: extracting\n", chron.millis()));

    // Finally build the abstract by walking the map (in order of position)
    vector<string> vabs;
    string chunk;
    bool incjk = false;
    for (map<unsigned int, string>::const_iterator it = sparseDoc.begin();
	 it != sparseDoc.end(); it++) {
	LOGDEB2(("Abtract:output %u -> [%s]\n", it->first,it->second.c_str()));
	if (!occupiedmarker.compare(it->second))
	    continue;
	Utf8Iter uit(it->second);
	bool newcjk = false;
	if (TextSplit::isCJK(*uit))
	    newcjk = true;
	if (!incjk || (incjk && !newcjk))
	    chunk += " ";
	incjk = newcjk;
	if (it->second == cstr_ellipsis) {
	    vabs.push_back(chunk);
	    chunk.clear();
	} else {
	    chunk += it->second;
	}
    }
    if (!chunk.empty())
	vabs.push_back(chunk);

    LOGDEB2(("makeAbtract: done in %d mS\n", chron.millis()));
    return vabs;
}

/* Rcl::Db methods ///////////////////////////////// */

Db::Db(RclConfig *cfp)
    : m_ndb(0), m_config(cfp), m_idxAbsTruncLen(250), m_synthAbsLen(250),
      m_synthAbsWordCtxLen(4), m_flushMb(-1), 
      m_curtxtsz(0), m_flushtxtsz(0), m_occtxtsz(0), m_occFirstCheck(1),
      m_maxFsOccupPc(0), m_mode(Db::DbRO)
{
    if (!fldToTraits.size())
	initFldToTraits();

    m_ndb = new Native(this);
    if (m_config) {
	m_config->getConfParam("maxfsoccuppc", &m_maxFsOccupPc);
	m_config->getConfParam("idxflushmb", &m_flushMb);
    }
}

Db::~Db()
{
    LOGDEB2(("Db::~Db\n"));
    if (m_ndb == 0)
	return;
    LOGDEB(("Db::~Db: isopen %d m_iswritable %d\n", m_ndb->m_isopen,
	    m_ndb->m_iswritable));
    i_close(true);
}

list<string> Db::getStemmerNames()
{
    list<string> res;
    stringToStrings(Xapian::Stem::get_available_languages(), res);
    return res;
}

bool Db::open(OpenMode mode, OpenError *error)
{
    if (error)
	*error = DbOpenMainDb;

    if (m_ndb == 0 || m_config == 0) {
	m_reason = "Null configuration or Xapian Db";
	return false;
    }
    LOGDEB(("Db::open: m_isopen %d m_iswritable %d\n", m_ndb->m_isopen, 
	    m_ndb->m_iswritable));

    if (m_ndb->m_isopen) {
	// We used to return an error here but I see no reason to
	if (!close())
	    return false;
    }
    if (!m_config->getStopfile().empty())
	m_stops.setFile(m_config->getStopfile());
    string dir = m_config->getDbDir();
    string ermsg;
    try {
	switch (mode) {
	case DbUpd:
	case DbTrunc: 
	    {
		int action = (mode == DbUpd) ? Xapian::DB_CREATE_OR_OPEN :
		    Xapian::DB_CREATE_OR_OVERWRITE;
		m_ndb->xwdb = Xapian::WritableDatabase(dir, action);
                // If db is empty, write the data format version at once
                // to avoid stupid error messages:
                if (m_ndb->xwdb.get_doccount() == 0)
                    m_ndb->xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, 
                                             cstr_RCL_IDX_VERSION);
		m_ndb->m_iswritable = true;
		// We open a readonly object in all cases (possibly in
		// addition to the r/w one) because some operations
		// are faster when performed through a Database: no
		// forced flushes on allterms_begin(), ie, used in
		// subDocs()
		m_ndb->xrdb = Xapian::Database(dir);
		LOGDEB(("Db::open: lastdocid: %d\n", 
			m_ndb->xwdb.get_lastdocid()));
                LOGDEB2(("Db::open: resetting updated\n"));
                updated.resize(m_ndb->xwdb.get_lastdocid() + 1);
                for (unsigned int i = 0; i < updated.size(); i++)
                    updated[i] = false;
	    }
	    break;
	case DbRO:
	default:
	    m_ndb->m_iswritable = false;
	    m_ndb->xrdb = Xapian::Database(dir);
	    for (list<string>::iterator it = m_extraDbs.begin();
		 it != m_extraDbs.end(); it++) {
		if (error)
		    *error = DbOpenExtraDb;
		LOGDEB(("Db::Open: adding query db [%s]\n", it->c_str()));
                // An error here used to be non-fatal (1.13 and older)
                // but I can't see why
                m_ndb->xrdb.add_database(Xapian::Database(*it));
	    }
	    break;
	}
	if (error)
	    *error = DbOpenMainDb;

	// Check index format version. Must not try to check a just created or
	// truncated db
	if (mode != DbTrunc && m_ndb->xdb().get_doccount() > 0) {
	    string version = m_ndb->xdb().get_metadata(cstr_RCL_IDX_VERSION_KEY);
	    if (version.compare(cstr_RCL_IDX_VERSION)) {
		m_ndb->m_noversionwrite = true;
		LOGERR(("Rcl::Db::open: file index [%s], software [%s]\n",
			version.c_str(), cstr_RCL_IDX_VERSION.c_str()));
		throw Xapian::DatabaseError("Recoll index version mismatch",
					    "", "");
	    }
	}
	m_mode = mode;
	m_ndb->m_isopen = true;
	m_basedir = dir;
	if (error)
	    *error = DbOpenNoError;
	return true;
    } XCATCHERROR(ermsg);

    m_reason = ermsg;
    LOGERR(("Db::open: exception while opening [%s]: %s\n", 
	    dir.c_str(), ermsg.c_str()));
    return false;
}

// Note: xapian has no close call, we delete and recreate the db
bool Db::close()
{
    LOGDEB2(("Db::close()\n"));
    return i_close(false);
}
bool Db::i_close(bool final)
{
    if (m_ndb == 0)
	return false;
    LOGDEB(("Db::i_close(%d): m_isopen %d m_iswritable %d\n", final,
	    m_ndb->m_isopen, m_ndb->m_iswritable));
    if (m_ndb->m_isopen == false && !final) 
	return true;

    string ermsg;
    try {
	bool w = m_ndb->m_iswritable;
	if (w) {
	    if (!m_ndb->m_noversionwrite)
		m_ndb->xwdb.set_metadata(cstr_RCL_IDX_VERSION_KEY, cstr_RCL_IDX_VERSION);
	    LOGDEB(("Rcl::Db:close: xapian will close. May take some time\n"));
	}
	// Used to do a flush here. Cant see why it should be necessary.
	deleteZ(m_ndb);
	if (w)
	    LOGDEB(("Rcl::Db:close() xapian close done.\n"));
	if (final) {
	    return true;
	}
	m_ndb = new Native(this);
	if (m_ndb) {
	    return true;
	}
	LOGERR(("Rcl::Db::close(): cant recreate db object\n"));
	return false;
    } XCATCHERROR(ermsg);
    LOGERR(("Db:close: exception while deleting db: %s\n", ermsg.c_str()));
    return false;
}

// Reopen the db with a changed list of additional dbs
bool Db::adjustdbs()
{
    if (m_mode != DbRO) {
        LOGERR(("Db::adjustdbs: mode not RO\n"));
        return false;
    }
    if (m_ndb && m_ndb->m_isopen) {
	if (!close())
	    return false;
	if (!open(m_mode)) {
	    return false;
	}
    }
    return true;
}

int Db::docCnt()
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    XAPTRY(res = m_ndb->xdb().get_doccount(), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR(("Db::docCnt: got error: %s\n", m_reason.c_str()));
        return -1;
    }
    return res;
}

int Db::termDocCnt(const string& _term)
{
    int res = -1;
    if (!m_ndb || !m_ndb->m_isopen)
        return -1;

    string term;
    if (!unacmaybefold(_term, term, "UTF-8", true)) {
	LOGINFO(("Db::termDocCnt: unac failed for [%s]\n", _term.c_str()));
	return 0;
    }

    if (m_stops.isStop(term)) {
	LOGDEB1(("Db::termDocCnt [%s] in stop list\n", term.c_str()));
	return 0;
    }

    XAPTRY(res = m_ndb->xdb().get_termfreq(term), m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
        LOGERR(("Db::termDocCnt: got error: %s\n", m_reason.c_str()));
        return -1;
    }
    return res;
}

bool Db::addQueryDb(const string &dir) 
{
    LOGDEB(("Db::addQueryDb: ndb %p iswritable %d db [%s]\n", m_ndb,
	      (m_ndb)?m_ndb->m_iswritable:0, dir.c_str()));
    if (!m_ndb)
	return false;
    if (m_ndb->m_iswritable)
	return false;
    if (find(m_extraDbs.begin(), m_extraDbs.end(), dir) == m_extraDbs.end()) {
	m_extraDbs.push_back(dir);
    }
    return adjustdbs();
}

bool Db::rmQueryDb(const string &dir)
{
    if (!m_ndb)
	return false;
    if (m_ndb->m_iswritable)
	return false;
    if (dir.empty()) {
	m_extraDbs.clear();
    } else {
	list<string>::iterator it = find(m_extraDbs.begin(), 
					 m_extraDbs.end(), dir);
	if (it != m_extraDbs.end()) {
	    m_extraDbs.erase(it);
	}
    }
    return adjustdbs();
}

bool Db::testDbDir(const string &dir)
{
    string aerr;
    LOGDEB(("Db::testDbDir: [%s]\n", dir.c_str()));
    try {
	Xapian::Database db(dir);
    } XCATCHERROR(aerr);
    if (!aerr.empty()) {
	LOGERR(("Db::Open: error while trying to open database "
		"from [%s]: %s\n", dir.c_str(), aerr.c_str()));
	return false;
    }
    return true;
}

bool Db::isopen()
{
    if (m_ndb == 0)
	return false;
    return m_ndb->m_isopen;
}

// Try to translate field specification into field prefix.  We have a
// default table used if translations are not in the config for some
// reason (old config not updated ?). We use it only if the config
// translation fails. Also we add in there fields which should be
// indexed with no prefix (ie: abstract)
bool Db::fieldToTraits(const string& fld, const FieldTraits **ftpp)
{
    if (m_config && m_config->getFieldTraits(fld, ftpp))
	return true;

    // No data in rclconfig? Check default values
    map<string, FieldTraits>::const_iterator it = fldToTraits.find(fld);
    if (it != fldToTraits.end()) {
	*ftpp = &it->second;
	return true;
    }
    *ftpp = 0;
    return false;
}


// The splitter breaks text into words and adds postings to the Xapian
// document. We use a single object to split all of the document
// fields and position jumps to separate fields
class TextSplitDb : public TextSplit {
 public:
    Xapian::WritableDatabase db;
    Xapian::Document &doc;   // Xapian document 
    // Base for document section. Gets large increment when we change
    // sections, to avoid cross-section proximity matches.
    Xapian::termpos basepos;
    // Current relative position. This is the remembered value from
    // the splitter callback. The term position is reset for each call
    // to text_to_words(), so that the last value of curpos is the
    // section size (last relative term position), and this is what
    // gets added to basepos in addition to the inter-section increment
    // to compute the first position of the next section.
    Xapian::termpos curpos;

    StopList &stops;
    TextSplitDb(Xapian::WritableDatabase idb, 
		Xapian::Document &d, StopList &_stops) 
	: db(idb), doc(d), basepos(1), curpos(0), stops(_stops), wdfinc(1)
    {}
    // Reimplement text_to_words to add start and end special terms
    virtual bool text_to_words(const string &in);
    bool takeword(const std::string &term, int pos, int, int);
    void setprefix(const string& pref) {prefix = pref;}
    void setwdfinc(int i) {wdfinc = i;}

private:
    // If prefix is set, we also add a posting for the prefixed terms
    // (ie: for titles, add postings for both "term" and "Sterm")
    string  prefix; 
    // Some fields have more weight
    int wdfinc;
};


bool TextSplitDb::text_to_words(const string &in) 
{
    LOGDEB2(("TextSplitDb::text_to_words\n"));
    string ermsg;
    try {
	// Index the possibly prefixed start term.
	doc.add_posting(prefix + start_of_field_term, basepos, wdfinc);
	++basepos;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db: xapian add_posting error %s\n", ermsg.c_str()));
	basepos += curpos + 100;
	return false;
    }

    if (!TextSplit::text_to_words(in)) {
	LOGDEB(("TextSplitDb: TextSplit::text_to_words failed\n"));
	basepos += curpos + 100;
	return false;
    }

    try {
	// Index the possibly prefixed end term.
	doc.add_posting(prefix + end_of_field_term, basepos+curpos+1, wdfinc);
	++basepos;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db: xapian add_posting error %s\n", ermsg.c_str()));
	basepos += curpos + 100;
	return false;
    }
    basepos += curpos + 100;
    return true;
}

// Get one term from the doc, remove accents and lowercase, then add posting
bool TextSplitDb::takeword(const std::string &_term, int pos, int, int)
{
    LOGDEB2(("TextSplitDb::takeword: [%s]\n", _term.c_str()));

    string term;
    if (!unacmaybefold(_term, term, "UTF-8", true)) {
	LOGINFO(("Db::splitter::takeword: unac failed for [%s]\n", 
                 _term.c_str()));
	term.clear();
	// We don't generate a fatal error because of a bad term
	return true;
    }

    if (stops.isStop(term)) {
	LOGDEB1(("Db: takeword [%s] in stop list\n", term.c_str()));
	return true;
    }

    // Compute absolute position (pos is relative to current segment),
    // and remember relative.
    curpos = pos;
    pos += basepos;
    string ermsg;
    try {
	// Index without prefix, using the field-specific weighting
	doc.add_posting(term, pos, wdfinc);
#ifdef TESTING_XAPIAN_SPELL
	if (Db::isSpellingCandidate(term)) {
	    db.add_spelling(term);
	}
#endif
	// Index the prefixed term.
	if (!prefix.empty()) {
	    doc.add_posting(prefix + term, pos, wdfinc);
	}
	return true;
    } XCATCHERROR(ermsg);
    LOGERR(("Db: xapian add_posting error %s\n", ermsg.c_str()));
    return false;
}

#ifdef TESTING_XAPIAN_SPELL
string Db::getSpellingSuggestion(const string& word)
{
    if (m_ndb == 0)
	return string();
    string term;
    if (!unacmaybefold(word, term, "UTF-8", true)) {
	LOGINFO(("Db::getSpelling: unac failed for [%s]\n", word.c_str()));
	return string();
    }
    if (!isSpellingCandidate(term))
	return string();
    return m_ndb->xrdb.get_spelling_suggestion(term);
}
#endif

// Let our user set the parameters for abstract processing
void Db::setAbstractParams(int idxtrunc, int syntlen, int syntctxlen)
{
    LOGDEB1(("Db::setAbstractParams: trunc %d syntlen %d ctxlen %d\n",
	    idxtrunc, syntlen, syntctxlen));
    if (idxtrunc > 0)
	m_idxAbsTruncLen = idxtrunc;
    if (syntlen > 0)
	m_synthAbsLen = syntlen;
    if (syntctxlen > 0)
	m_synthAbsWordCtxLen = syntctxlen;
}

static const int MB = 1024 * 1024;
static const string cstr_nc("\n\r\x0c");

#define RECORD_APPEND(R, NM, VAL) {R += NM + "=" + VAL + "\n";}

// Add document in internal form to the database: index the terms in
// the title abstract and body and add special terms for file name,
// date, mime type etc. , create the document data record (more
// metadata), and update database
bool Db::addOrUpdate(const string &udi, const string &parent_udi,
		     const Doc &idoc)
{
    LOGDEB(("Db::add: udi [%s] parent [%s]\n", 
	     udi.c_str(), parent_udi.c_str()));
    if (m_ndb == 0)
	return false;
    // Check file system full every mbyte of indexed text.
    if (m_maxFsOccupPc > 0 && 
	(m_occFirstCheck || (m_curtxtsz - m_occtxtsz) / MB >= 1)) {
	LOGDEB(("Db::add: checking file system usage\n"));
	int pc;
	m_occFirstCheck = 0;
	if (fsocc(m_basedir, &pc) && pc >= m_maxFsOccupPc) {
	    LOGERR(("Db::add: stop indexing: file system "
		     "%d%% full > max %d%%\n", pc, m_maxFsOccupPc));
	    return false;
	}
	m_occtxtsz = m_curtxtsz;
    }

    Doc doc = idoc;

    Xapian::Document newdocument;
    TextSplitDb splitter(m_ndb->xwdb, newdocument, m_stops);

    // Split and index file name as document term(s)
    LOGDEB2(("Db::add: split file name [%s]\n", fn.c_str()));
    if (!splitter.text_to_words(doc.utf8fn))
        LOGDEB(("Db::addOrUpdate: split failed for file name\n"));

    // If the ipath is like a path, index the last element. This is
    // for compound documents like zip and chm for which the filter
    // uses the file path as ipath. 
    if (!doc.ipath.empty() && 
	doc.ipath.find_first_not_of("0123456789") != string::npos) {
	string utf8ipathlast;
	// There is no way in hell we could have an idea of the
	// charset here, so let's hope it's ascii or utf-8. We call
	// transcode to strip the bad chars and pray
	if (transcode(path_getsimple(doc.ipath), utf8ipathlast,
		      "UTF-8", "UTF-8")) {
	    splitter.text_to_words(utf8ipathlast);
	}
    }

    // Split and index the path from the url for path-based filtering
    {
	string path = url_gpath(doc.url);
	vector<string> vpath;
	stringToTokens(path, vpath, "/");
	splitter.curpos = 0;
	newdocument.add_posting(pathelt_prefix, 
				splitter.basepos + splitter.curpos++);
	for (vector<string>::iterator it = vpath.begin(); 
	     it != vpath.end(); it++){
	    if (it->length() > 230) {
		// Just truncate it. May still be useful because of wildcards
		*it = it->substr(0, 230);
	    }
	    newdocument.add_posting(pathelt_prefix + *it, 
				    splitter.basepos + splitter.curpos++);
	}
    }

    // Index textual metadata.  These are all indexed as text with
    // positions, as we may want to do phrase searches with them (this
    // makes no sense for keywords by the way).
    //
    // The order has no importance, and we set a position gap of 100
    // between fields to avoid false proximity matches.
    map<string, string>::iterator meta_it;
    for (meta_it = doc.meta.begin(); meta_it != doc.meta.end(); meta_it++) {
	if (!meta_it->second.empty()) {
	    const FieldTraits *ftp;
	    // We don't test for an empty prefix here. Some fields are part
	    // of the internal conf with an empty prefix (ie: abstract).
	    if (!fieldToTraits(meta_it->first, &ftp)) {
		LOGDEB0(("Db::add: no prefix for field [%s], no indexing\n",
			 meta_it->first.c_str()));
		continue;
	    }
	    LOGDEB0(("Db::add: field [%s] pfx [%s] inc %d: [%s]\n", 
		     meta_it->first.c_str(), ftp->pfx.c_str(), ftp->wdfinc,
		     meta_it->second.c_str()));
	    splitter.setprefix(ftp->pfx); // Subject
	    splitter.setwdfinc(ftp->wdfinc);
	    if (!splitter.text_to_words(meta_it->second))
                LOGDEB(("Db::addOrUpdate: split failed for %s\n", 
                        meta_it->first.c_str()));
	}
    }
    splitter.setprefix(string());
    splitter.setwdfinc(1);

    if (splitter.curpos < baseTextPosition)
	splitter.basepos = baseTextPosition;

    // Split and index body text
    LOGDEB2(("Db::add: split body: [%s]\n", doc.text.c_str()));
    if (!splitter.text_to_words(doc.text))
        LOGDEB(("Db::addOrUpdate: split failed for main text\n"));

    ////// Special terms for other metadata. No positions for these.
    // Mime type
    newdocument.add_term("T" + doc.mimetype);

    // Simple file name indexed for file name searches with a term prefix
    // We also add a term for the filename extension if any.
    if (!doc.utf8fn.empty()) {
	string fn;
	if (unacmaybefold(doc.utf8fn, fn, "UTF-8", true)) {
	    // We should truncate after extracting the extension, but this is
	    // a pathological case anyway
	    if (fn.size() > 230)
		utf8truncate(fn, 230);
	    string::size_type pos = fn.rfind('.');
	    if (pos != string::npos && pos != fn.length() - 1) {
		newdocument.add_term(string("XE") + fn.substr(pos + 1));
	    }
	    fn = string("XSFN") + fn;
	    newdocument.add_term(fn);
	}
        // Store utf8fn inside the metadata array as keyfn
        // (="filename") so that it can be accessed by the "stored"
        // processing below, without special-casing it. We only do it
        // if keyfn is currently empty, because there could be a value
        // already (ie for a mail attachment with a file name
        // attribute)
	if (doc.meta[Doc::keyfn].empty()) {
            doc.meta[Doc::keyfn] = doc.utf8fn;
	}
    }

    // Udi unique term: this is used for file existence/uptodate
    // checks, and unique id for the replace_document() call.
    string uniterm = make_uniterm(udi);
    newdocument.add_term(uniterm);
    // Parent term. This is used to find all descendents, mostly to delete them 
    // when the parent goes away
    if (!parent_udi.empty()) {
	newdocument.add_term(make_parentterm(parent_udi));
    }
    // Dates etc.
    time_t mtime = atol(doc.dmtime.empty() ? doc.fmtime.c_str() : 
			doc.dmtime.c_str());
    struct tm *tm = localtime(&mtime);
    char buf[9];
    snprintf(buf, 9, "%04d%02d%02d",
	    tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday);
    newdocument.add_term("D" + string(buf)); // Date (YYYYMMDD)
    buf[6] = '\0';
    newdocument.add_term("M" + string(buf)); // Month (YYYYMM)
    buf[4] = '\0';
    newdocument.add_term("Y" + string(buf)); // Year (YYYY)


    //////////////////////////////////////////////////////////////////
    // Document data record. omindex has the following nl separated fields:
    // - url
    // - sample
    // - caption (title limited to 100 chars)
    // - mime type 
    //
    // The title, author, abstract and keywords fields are special,
    // they always get stored in the document data
    // record. Configurable other fields can be, too.
    //
    // We truncate stored fields abstract, title and keywords to
    // reasonable lengths and suppress newlines (so that the data
    // record can keep a simple syntax)

    string record;
    RECORD_APPEND(record, Doc::keyurl, doc.url);
    RECORD_APPEND(record, Doc::keytp, doc.mimetype);
    // We left-zero-pad the times so that they are lexico-sortable
    leftzeropad(doc.fmtime, 11);
    RECORD_APPEND(record, Doc::keyfmt, doc.fmtime);
    if (!doc.dmtime.empty()) {
	leftzeropad(doc.dmtime, 11);
	RECORD_APPEND(record, Doc::keydmt, doc.dmtime);
    }
    RECORD_APPEND(record, Doc::keyoc, doc.origcharset);

    if (!doc.fbytes.empty())
	RECORD_APPEND(record, Doc::keyfs, doc.fbytes);
    // Note that we add the signature both as a value and in the data record
    if (!doc.sig.empty())
	RECORD_APPEND(record, Doc::keysig, doc.sig);
    newdocument.add_value(VALUE_SIG, doc.sig);

    char sizebuf[30]; 
    sprintf(sizebuf, "%u", (unsigned int)doc.text.length());
    RECORD_APPEND(record, Doc::keyds, sizebuf);

    if (!doc.ipath.empty())
	RECORD_APPEND(record, Doc::keyipt, doc.ipath);

    if (doc.meta[Doc::keytt].empty())
	doc.meta[Doc::keytt] = doc.utf8fn;
    doc.meta[Doc::keytt] = 
	neutchars(truncate_to_word(doc.meta[Doc::keytt], 150), cstr_nc);
    if (!doc.meta[Doc::keytt].empty())
	RECORD_APPEND(record, cstr_caption, doc.meta[Doc::keytt]);

    trimstring(doc.meta[Doc::keykw], " \t\r\n");
    doc.meta[Doc::keykw] = 
	neutchars(truncate_to_word(doc.meta[Doc::keykw], 300), cstr_nc);
    if (!doc.meta[Doc::keykw].empty())
	RECORD_APPEND(record, Doc::keykw, doc.meta[Doc::keykw]);

    // If abstract is empty, we make up one with the beginning of the
    // document. This is then not indexed, but part of the doc data so
    // that we can return it to a query without having to decode the
    // original file.
    bool syntabs = false;
    // Note that the map accesses by operator[] create empty entries if they
    // don't exist yet.
    trimstring(doc.meta[Doc::keyabs], " \t\r\n");
    if (doc.meta[Doc::keyabs].empty()) {
	syntabs = true;
	if (!doc.text.empty())
	    doc.meta[Doc::keyabs] = cstr_syntAbs + 
		neutchars(truncate_to_word(doc.text, m_idxAbsTruncLen), cstr_nc);
    } else {
	doc.meta[Doc::keyabs] = 
	    neutchars(truncate_to_word(doc.meta[Doc::keyabs], m_idxAbsTruncLen),
		      cstr_nc);
    }
    if (!doc.meta[Doc::keyabs].empty())
	RECORD_APPEND(record, Doc::keyabs, doc.meta[Doc::keyabs]);

    const set<string>& stored = m_config->getStoredFields();
    for (set<string>::const_iterator it = stored.begin();
	 it != stored.end(); it++) {
	string nm = m_config->fieldCanon(*it);
	if (!doc.meta[*it].empty()) {
	    string value = 
		neutchars(truncate_to_word(doc.meta[*it], 150), cstr_nc);
	    RECORD_APPEND(record, nm, value);
	}
    }

    // If the file's md5 was computed, add value. This is optionally
    // used for query result duplicate elimination.
    string& md5 = doc.meta[Doc::keymd5];
    if (!md5.empty()) {
	string digest;
	MD5HexScan(md5, digest);
	newdocument.add_value(VALUE_MD5, digest);
    }

    LOGDEB0(("Rcl::Db::add: new doc record:\n%s\n", record.c_str()));
    newdocument.set_data(record);

    const char *fnc = udi.c_str();
    string ermsg;

    // Add db entry or update existing entry:
    try {
	Xapian::docid did = 
	    m_ndb->xwdb.replace_document(uniterm, newdocument);
	if (did < updated.size()) {
	    updated[did] = true;
	    LOGINFO(("Db::add: docid %d updated [%s]\n", did, fnc));
	} else {
	    LOGINFO(("Db::add: docid %d added [%s]\n", did, fnc));
	}
    } XCATCHERROR(ermsg);

    if (!ermsg.empty()) {
	LOGERR(("Db::add: replace_document failed: %s\n", ermsg.c_str()));
	ermsg.erase();
	// FIXME: is this ever actually needed?
	try {
	    m_ndb->xwdb.add_document(newdocument);
	    LOGDEB(("Db::add: %s added (failed re-seek for duplicate)\n", 
		    fnc));
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR(("Db::add: add_document failed: %s\n", ermsg.c_str()));
	    return false;
	}
    }

    // Test if we're over the flush threshold (limit memory usage):
    maybeflush(doc.text.length());
    return true;
}

// Flush when idxflushmbs is reached
bool Db::maybeflush(off_t moretext)
{
    if (m_flushMb > 0) {
	m_curtxtsz += moretext;
	if ((m_curtxtsz - m_flushtxtsz) / MB >= m_flushMb) {
	    LOGDEB(("Db::add/delete: txt size >= %d Mb, flushing\n", 
		    m_flushMb));
	    string ermsg;
	    try {
		m_ndb->xwdb.flush();
	    } XCATCHERROR(ermsg);
	    if (!ermsg.empty()) {
		LOGERR(("Db::add: flush() failed: %s\n", ermsg.c_str()));
		return false;
	    }
	    m_flushtxtsz = m_curtxtsz;
	}
    }
    return true;
}

// Test if doc given by udi has changed since last indexed (test sigs)
bool Db::needUpdate(const string &udi, const string& sig)
{
    if (m_ndb == 0)
	return false;

    string uniterm = make_uniterm(udi);
    string ermsg;

    // We look up the document indexed by the uniterm. This is either
    // the actual document file, or, for a multi-document file, the
    // pseudo-doc we create to stand for the file itself.

    // We try twice in case database needs to be reopened.
    for (int tries = 0; tries < 2; tries++) {
	try {
	    // Get the doc or pseudo-doc
	    Xapian::PostingIterator docid =m_ndb->xrdb.postlist_begin(uniterm);
	    if (docid == m_ndb->xrdb.postlist_end(uniterm)) {
		// If no document exist with this path, we do need update
		LOGDEB(("Db::needUpdate:yes (new): [%s]\n", uniterm.c_str()));
		return true;
	    }
	    Xapian::Document doc = m_ndb->xrdb.get_document(*docid);

	    // Retrieve old file/doc signature from value
	    string osig = doc.get_value(VALUE_SIG);
	    LOGDEB2(("Db::needUpdate: oldsig [%s] new [%s]\n",
		     osig.c_str(), sig.c_str()));
	    // Compare new/old sig
	    if (sig != osig) {
		LOGDEB(("Db::needUpdate:yes: olsig [%s] new [%s] [%s]\n",
			osig.c_str(), sig.c_str(), uniterm.c_str()));
		// Db is not up to date. Let's index the file
		return true;
	    }

	    LOGDEB(("Db::needUpdate:no: [%s]\n", uniterm.c_str()));

	    // Up to date. 

	    // Set the uptodate flag for doc / pseudo doc
	    updated[*docid] = true;

	    // Set the existence flag for all the subdocs (if any)
	    vector<Xapian::docid> docids;
	    if (!m_ndb->subDocs(udi, docids)) {
		LOGERR(("Rcl::Db::needUpdate: can't get subdocs list\n"));
		return true;
	    }
	    for (vector<Xapian::docid>::iterator it = docids.begin();
		 it != docids.end(); it++) {
		if (*it < updated.size()) {
		    LOGDEB2(("Db::needUpdate: set flag for docid %d\n", *it));
		    updated[*it] = true;
		}
	    }
	    return false;
	} catch (const Xapian::DatabaseModifiedError &e) {
	    LOGDEB(("Db::needUpdate: got modified error. reopen/retry\n"));
            m_reason = e.get_msg();
	    m_ndb->xrdb.reopen();
            continue;
	} XCATCHERROR(m_reason);
        break;
    }
    LOGERR(("Db::needUpdate: error while checking existence: %s\n", 
	    m_reason.c_str()));
    return true;
}


// Return list of existing stem db languages
list<string> Db::getStemLangs()
{
    LOGDEB(("Db::getStemLang\n"));
    list<string> dirs;
    if (m_ndb == 0 || m_ndb->m_isopen == false)
	return dirs;
    dirs = StemDb::getLangs(m_basedir);
    return dirs;
}

/**
 * Delete stem db for given language
 */
bool Db::deleteStemDb(const string& lang)
{
    LOGDEB(("Db::deleteStemDb(%s)\n", lang.c_str()));
    if (m_ndb == 0 || m_ndb->m_isopen == false)
	return false;
    return StemDb::deleteDb(m_basedir, lang);
}

/**
 * Create database of stem to parents associations for a given language.
 * We walk the list of all terms, stem them, and create another Xapian db
 * with documents indexed by a single term (the stem), and with the list of
 * parent terms in the document data.
 */
bool Db::createStemDb(const string& lang)
{
    LOGDEB(("Db::createStemDb(%s)\n", lang.c_str()));
    if (m_ndb == 0 || m_ndb->m_isopen == false)
	return false;

    return StemDb::createDb(m_ndb->xdb(), m_basedir, lang);
}

/**
 * This is called at the end of an indexing session, to delete the
 * documents for files that are no longer there. This can ONLY be called
 * after a full file-system tree walk, else the file existence flags will 
 * be wrong.
 */
bool Db::purge()
{
    LOGDEB(("Db::purge\n"));
    if (m_ndb == 0)
	return false;
    LOGDEB(("Db::purge: m_isopen %d m_iswritable %d\n", m_ndb->m_isopen, 
	    m_ndb->m_iswritable));
    if (m_ndb->m_isopen == false || m_ndb->m_iswritable == false) 
	return false;

    // For xapian versions up to 1.0.1, deleting a non-existant
    // document would trigger an exception that would discard any
    // pending update. This could lose both previous added documents
    // or deletions. Adding the flush before the delete pass ensured
    // that any added document would go to the index. Kept here
    // because it doesn't really hurt.
    try {
	m_ndb->xwdb.flush();
    } catch (...) {
	LOGERR(("Db::purge: 1st flush failed\n"));

    }

    // Walk the document array and delete any xapian document whose
    // flag is not set (we did not see its source during indexing).
    int purgecount = 0;
    for (Xapian::docid docid = 1; docid < updated.size(); ++docid) {
	if (!updated[docid]) {
	    if ((purgecount+1) % 100 == 0) {
		try {
		    CancelCheck::instance().checkCancel();
		} catch(CancelExcept) {
		    LOGINFO(("Db::purge: partially cancelled\n"));
		    break;
		}
	    }

	    try {
		if (m_flushMb > 0) {
		    // We use an average term length of 5 for
		    // estimating the doc sizes which is probably not
		    // accurate but gives rough consistency with what
		    // we do for add/update. I should fetch the doc
		    // size from the data record, but this would be
		    // bad for performance.
		    Xapian::termcount trms = m_ndb->xwdb.get_doclength(docid);
		    maybeflush(trms * 5);
		}
		m_ndb->xwdb.delete_document(docid);
		LOGDEB(("Db::purge: deleted document #%d\n", docid));
	    } catch (const Xapian::DocNotFoundError &) {
		LOGDEB0(("Db::purge: document #%d not found\n", docid));
	    } catch (const Xapian::Error &e) {
		LOGERR(("Db::purge: document #%d: %s\n", docid, e.get_msg().c_str()));
	    } catch (...) {
		LOGERR(("Db::purge: document #%d: unknown error\n", docid));
	    }
	    purgecount++;
	}
    }

    try {
	m_ndb->xwdb.flush();
    } catch (...) {
	LOGERR(("Db::purge: 2nd flush failed\n"));
    }
    return true;
}

/* Delete document(s) for given unique identifier (doc and descendents) */
bool Db::purgeFile(const string &udi, bool *existed)
{
    LOGDEB(("Db:purgeFile: [%s]\n", udi.c_str()));
    if (m_ndb == 0 || !m_ndb->m_iswritable)
	return false;
    Xapian::WritableDatabase db = m_ndb->xwdb;
    string uniterm = make_uniterm(udi);
    string ermsg;
    try {
	Xapian::PostingIterator docid = db.postlist_begin(uniterm);
	if (docid == db.postlist_end(uniterm)) {
            if (existed)
                *existed = false;
	    return true;
        }
        *existed = true;
	LOGDEB(("purgeFile: delete docid %d\n", *docid));
	if (m_flushMb > 0) {
	    Xapian::termcount trms = m_ndb->xwdb.get_doclength(*docid);
	    maybeflush(trms * 5);
	}
	db.delete_document(*docid);
	vector<Xapian::docid> docids;
	m_ndb->subDocs(udi, docids);
	LOGDEB(("purgeFile: subdocs cnt %d\n", docids.size()));
	for (vector<Xapian::docid>::iterator it = docids.begin();
	     it != docids.end(); it++) {
	    LOGDEB(("Db::purgeFile: delete subdoc %d\n", *it));
	    if (m_flushMb > 0) {
		Xapian::termcount trms = m_ndb->xwdb.get_doclength(*it);
		maybeflush(trms * 5);
	    }
	    db.delete_document(*it);
	}
	return true;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db::purgeFile: %s\n", ermsg.c_str()));
    }
    return false;
}

// File name wild card expansion. This is a specialisation ot termMatch
bool Db::filenameWildExp(const string& fnexp, list<string>& names)
{
    string pattern = fnexp;
    names.clear();

    // If pattern is not capitalized, not quoted (quoted pattern can't
    // get here currently anyway), and has no wildcards, we add * at
    // each end: match any substring
    if (pattern[0] == '"' && pattern[pattern.size()-1] == '"') {
	pattern = pattern.substr(1, pattern.size() -2);
    } else if (pattern.find_first_of(cstr_minwilds) == string::npos && 
	       !unaciscapital(pattern)) {
	pattern = "*" + pattern + "*";
    } // else let it be

    LOGDEB(("Rcl::Db::filenameWildExp: pattern: [%s]\n", pattern.c_str()));

    TermMatchResult result;
    if (!termMatch(ET_WILD, string(), pattern, result, 1000, Doc::keyfn))
	return false;
    for (list<TermMatchEntry>::const_iterator it = result.entries.begin();
	 it != result.entries.end(); it++) 
	names.push_back(it->term);

    if (names.empty()) {
	// Build an impossible query: we know its impossible because we
	// control the prefixes!
	names.push_back("XNONENoMatchingTerms");
    }
    return true;
}

// Walk the Y terms and return min/max
bool Db::maxYearSpan(int *minyear, int *maxyear)
{
    *minyear = 1000000; 
    *maxyear = -1000000;
    TermMatchResult result;
    if (!termMatch(ET_WILD, string(), "*", result, 5000, "xapyear"))
	return false;
    for (list<TermMatchEntry>::const_iterator it = result.entries.begin();
	 it != result.entries.end(); it++) {
        if (!it->term.empty()) {
            int year = atoi(it->term.c_str()+1);
            if (year < *minyear)
                *minyear = year;
            if (year > *maxyear)
                *maxyear = year;
        }
    }
    return true;
}


class TermMatchCmpByWcf {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
	return r.wcf - l.wcf < 0;
    }
};
class TermMatchCmpByTerm {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
	return l.term.compare(r.term) > 0;
    }
};
class TermMatchTermEqual {
public:
    int operator()(const TermMatchEntry& l, const TermMatchEntry& r) {
	return !l.term.compare(r.term);
    }
};

bool Db::stemExpand(const string &lang, const string &term, 
		    TermMatchResult& result, int max)
{
    list<string> dirs = m_extraDbs;
    dirs.push_front(m_basedir);
    for (list<string>::iterator it = dirs.begin(); it != dirs.end(); it++) {
	list<string> more;
	StemDb::stemExpand(*it, lang, term, more);
	LOGDEB1(("Db::stemExpand: Got %d from %s\n", 
		 more.size(), it->c_str()));
	result.entries.insert(result.entries.end(), more.begin(), more.end());
	if (result.entries.size() >= (unsigned int)max)
	    break;
    }
    LOGDEB1(("Db:::stemExpand: final count %d \n", result.size()));
    return true;
}

/** Add prefix to all strings in list */
static void addPrefix(list<TermMatchEntry>& terms, const string& prefix)
{
    if (prefix.empty())
	return;
    for (list<TermMatchEntry>::iterator it = terms.begin(); 
         it != terms.end(); it++)
	it->term.insert(0, prefix);
}

// Characters that can begin a wildcard or regexp expression. We use skipto
// to begin the allterms search with terms that begin with the portion of
// the input string prior to these chars.
const string cstr_wildSpecChars = "*?[";
const string cstr_regSpecChars = "(.[{";

// Find all index terms that match a wildcard or regular expression
bool Db::termMatch(MatchType typ, const string &lang,
		   const string &root, 
		   TermMatchResult& res,
		   int max, 
		   const string& field,
                   string *prefixp
    )
{
    if (!m_ndb || !m_ndb->m_isopen)
	return false;
    Xapian::Database xdb = m_ndb->xdb();

    res.clear();
    XAPTRY(res.dbdoccount = xdb.get_doccount();
           res.dbavgdoclen = xdb.get_avlength(), xdb, m_reason);
    if (!m_reason.empty())
        return false;

    // Get rid of capitals and accents
    string droot;
    if (!unacmaybefold(root, droot, "UTF-8", true)) {
	LOGERR(("Db::termMatch: unac failed for [%s]\n", root.c_str()));
	return false;
    }
    string nochars = typ == ET_WILD ? cstr_wildSpecChars : cstr_regSpecChars;

    string prefix;
    if (!field.empty()) {
	const FieldTraits *ftp = 0;
	if (!fieldToTraits(field, &ftp) || ftp->pfx.empty()) {
            LOGDEB(("Db::termMatch: field is not indexed (no prefix): [%s]\n", 
                    field.c_str()));
        } else {
	    prefix = ftp->pfx;
	}
        if (prefixp)
            *prefixp = prefix;
    }

    if (typ == ET_STEM) {
	if (!stemExpand(lang, root, res, max))
	    return false;
	res.entries.sort();
	res.entries.unique();
	for (list<TermMatchEntry>::iterator it = res.entries.begin(); 
	     it != res.entries.end(); it++) {
	    XAPTRY(it->wcf = xdb.get_collection_freq(it->term);
                   it->docs = xdb.get_termfreq(it->term),
                   xdb, m_reason);
            if (!m_reason.empty())
                return false;
	    LOGDEB1(("termMatch: %d [%s]\n", it->wcf, it->term.c_str()));
	}
        if (!prefix.empty())
            addPrefix(res.entries, prefix);
    } else {
	regex_t reg;
	int errcode;
	if (typ == ET_REGEXP) {
	    string mroot = droot;
	    if ((errcode = regcomp(&reg, mroot.c_str(), 
				   REG_EXTENDED|REG_NOSUB))) {
		char errbuf[200];
		regerror(errcode, &reg, errbuf, 199);
		LOGERR(("termMatch: regcomp failed: %s\n", errbuf));
		res.entries.push_back(string(errbuf));
		regfree(&reg);
		return false;
	    }
	}

	// Find the initial section before any special char
	string::size_type es = droot.find_first_of(nochars);
	string is;
	switch (es) {
	case string::npos: is = prefix + droot; break;
	case 0: is = prefix; break;
	default: is = prefix + droot.substr(0, es); break;
	}
	LOGDEB(("termMatch: initsec: [%s]\n", is.c_str()));

        for (int tries = 0; tries < 2; tries++) { 
            try {
                Xapian::TermIterator it = xdb.allterms_begin(); 
                if (!is.empty())
                    it.skip_to(is.c_str());
                for (int n = 0; it != xdb.allterms_end(); it++) {
                    // If we're beyond the terms matching the initial
                    // string, end
                    if (!is.empty() && (*it).find(is) != 0)
                        break;
                    string term;
                    if (!prefix.empty())
                        term = (*it).substr(prefix.length());
                    else
                        term = *it;
                    if (typ == ET_WILD) {
                        if (fnmatch(droot.c_str(), term.c_str(), 0) == 
                            FNM_NOMATCH)
                            continue;
                    } else {
                        if (regexec(&reg, term.c_str(), 0, 0, 0))
                            continue;
                    }
                    // Do we want stem expansion here? We don't do it for now
                    res.entries.push_back(TermMatchEntry(*it, 
                                                   xdb.get_collection_freq(*it),
                                                   it.get_termfreq()));
                    ++n;
                }
                m_reason.erase();
                break;
            } catch (const Xapian::DatabaseModifiedError &e) {
                m_reason = e.get_msg();
                xdb.reopen();
                continue;
            } XCATCHERROR(m_reason);
            break;
        }
	if (!m_reason.empty()) {
	    LOGERR(("termMatch: %s\n", m_reason.c_str()));
	    return false;
	}

	if (typ == ET_REGEXP) {
	    regfree(&reg);
	}

    }

    TermMatchCmpByTerm tcmp;
    res.entries.sort(tcmp);
    TermMatchTermEqual teq;
    res.entries.unique(teq);
    TermMatchCmpByWcf wcmp;
    res.entries.sort(wcmp);
    if (max > 0) {
	res.entries.resize(MIN(res.entries.size(), (unsigned int)max));
    }
    return true;
}

/** Term list walking. */
class TermIter {
public:
    Xapian::TermIterator it;
    Xapian::Database db;
};
TermIter *Db::termWalkOpen()
{
    if (!m_ndb || !m_ndb->m_isopen)
	return 0;
    TermIter *tit = new TermIter;
    if (tit) {
	tit->db = m_ndb->xdb();
        XAPTRY(tit->it = tit->db.allterms_begin(), tit->db, m_reason);
	if (!m_reason.empty()) {
	    LOGERR(("Db::termWalkOpen: xapian error: %s\n", m_reason.c_str()));
	    return 0;
	}
    }
    return tit;
}
bool Db::termWalkNext(TermIter *tit, string &term)
{
    XAPTRY(
	if (tit && tit->it != tit->db.allterms_end()) {
	    term = *(tit->it)++;
	    return true;
	}
        , tit->db, m_reason);

    if (!m_reason.empty()) {
	LOGERR(("Db::termWalkOpen: xapian error: %s\n", m_reason.c_str()));
    }
    return false;
}
void Db::termWalkClose(TermIter *tit)
{
    try {
	delete tit;
    } catch (...) {}
}

bool Db::termExists(const string& word)
{
    if (!m_ndb || !m_ndb->m_isopen)
	return 0;

    XAPTRY(if (!m_ndb->xdb().term_exists(word)) return false,
           m_ndb->xrdb, m_reason);

    if (!m_reason.empty()) {
	LOGERR(("Db::termWalkOpen: xapian error: %s\n", m_reason.c_str()));
	return false;
    }
    return true;
}


bool Db::stemDiffers(const string& lang, const string& word, 
		     const string& base)
{
    Xapian::Stem stemmer(lang);
    if (!stemmer(word).compare(stemmer(base))) {
	LOGDEB2(("Rcl::Db::stemDiffers: same for %s and %s\n", 
		word.c_str(), base.c_str()));
	return false;
    }
    return true;
}

bool Db::makeDocAbstract(Doc &doc, Query *query, vector<string>& abstract)
{
    LOGDEB1(("Db::makeDocAbstract: exti %d\n", exti));
    if (!m_ndb || !m_ndb->m_isopen) {
	LOGERR(("Db::makeDocAbstract: no db\n"));
	return false;
    }
    XAPTRY(abstract = m_ndb->makeAbstract(doc.xdocid, query),
           m_ndb->xrdb, m_reason);
    return m_reason.empty() ? true : false;
}

bool Db::makeDocAbstract(Doc &doc, Query *query, string& abstract)
{
    LOGDEB1(("Db::makeDocAbstract: exti %d\n", exti));
    if (!m_ndb || !m_ndb->m_isopen) {
	LOGERR(("Db::makeDocAbstract: no db\n"));
	return false;
    }
    vector<string> vab;
    XAPTRY(vab = m_ndb->makeAbstract(doc.xdocid, query),
           m_ndb->xrdb, m_reason);
    for (vector<string>::const_iterator it = vab.begin(); 
	 it != vab.end(); it++) {
	abstract.append(*it);
	abstract.append(cstr_ellipsis);
    }
    return m_reason.empty() ? true : false;
}

// Retrieve document defined by Unique doc identifier. This is mainly used
// by the GUI history feature
bool Db::getDoc(const string &udi, Doc &doc)
{
    LOGDEB(("Db:getDoc: [%s]\n", udi.c_str()));
    if (m_ndb == 0)
	return false;

    // Initialize what we can in any case. If this is history, caller
    // will make partial display in case of error
    doc.meta[Rcl::Doc::keyrr] = "100%";
    doc.pc = 100;

    string uniterm = make_uniterm(udi);
    for (int tries = 0; tries < 2; tries++) {
	try {
            if (!m_ndb->xrdb.term_exists(uniterm)) {
                // Document found in history no longer in the
                // database.  We return true (because their might be
                // other ok docs further) but indicate the error with
                // pc = -1
                doc.pc = -1;
                LOGINFO(("Db:getDoc: no such doc in index: [%s] (len %d)\n",
                         uniterm.c_str(), uniterm.length()));
                return true;
            }
            Xapian::PostingIterator docid = 
                m_ndb->xrdb.postlist_begin(uniterm);
            Xapian::Document xdoc = m_ndb->xrdb.get_document(*docid);
            string data = xdoc.get_data();
            doc.meta[Rcl::Doc::keyudi] = udi;
            return m_ndb->dbDataToRclDoc(*docid, data, doc);
	} catch (const Xapian::DatabaseModifiedError &e) {
            m_reason = e.get_msg();
	    m_ndb->xrdb.reopen();
            continue;
	} XCATCHERROR(m_reason);
        break;
    }

    LOGERR(("Db::getDoc: %s\n", m_reason.c_str()));
    return false;
}

#ifndef NO_NAMESPACES
}
#endif
