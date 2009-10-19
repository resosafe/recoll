#ifndef lint
static char rcsid[] = "@(#$Id: rcldb.cpp,v 1.154 2008-12-17 16:19:58 dockes Exp $ (C) 2004 J.F.Dockes";
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
#include <stdio.h>
#include <cstring>
#include <unistd.h>
#include <fnmatch.h>
#include <regex.h>
#include <math.h>

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "xapian/version.h"

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

#ifndef MAX
#define MAX(A,B) (A>B?A:B)
#endif
#ifndef MIN
#define MIN(A,B) (A<B?A:B)
#endif

// Recoll index format version is stored in user metadata. When this change,
// we can't open the db and will have to reindex.
static const string RCL_IDX_VERSION_KEY("RCL_IDX_VERSION_KEY");
static const string RCL_IDX_VERSION("1");

// This is the word position offset at which we index the body text
// (abstract, keywords, etc.. are stored before this)
static const unsigned int baseTextPosition = 100000;

#ifndef NO_NAMESPACES
namespace Rcl {
#endif

string version_string(){
    return string("Recoll ") + string(rclversionstr) + string(" + Xapian ") +
        string(Xapian::version_string());
}

// Synthetic abstract marker (to discriminate from abstract actually
// found in document)
const static string rclSyntAbs("?!#@");

// Compute the unique term used to link documents to their origin. 
// "Q" + external udi
static inline string make_uniterm(const string& udi)
{
    string uniterm("Q");
    uniterm.append(udi);
    return uniterm;
}
// Compute parent term used to link documents to their parent document (if any)
// "" + parent external udi
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
    string ermsg;
    string pterm = make_parentterm(udi);
    for (int tries = 0; tries < 2; tries++) {
	try {
	    Xapian::PostingIterator it = db.postlist_begin(pterm);
	    for (; it != db.postlist_end(pterm); it++) {
		docids.push_back(*it);
	    }
	    LOGDEB(("Db::Native::subDocs: returning %d ids\n", docids.size()));
	    return true;
	} catch (const Xapian::DatabaseModifiedError &e) {
	    LOGDEB(("Db::subDocs: got modified error. reopen/retry\n"));
	    // Can't use reOpen() here, I'm a Native:: method, this
	    // would delete my own object
	    db = Xapian::Database(m_db->m_basedir);
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) 
	    break;
    }
    LOGERR(("Rcl::Db::subDocs: %s\n", ermsg.c_str()));
    return false;
}

// Only ONE field name inside the index data record differs from the
// Rcl::Doc ones: caption<->title, for a remnant of compatibility with
// omega
static const string keycap("caption");

// Turn data record from db into document fields
bool Db::Native::dbDataToRclDoc(Xapian::docid docid, std::string &data, 
				Doc &doc, int percent)
{
    LOGDEB0(("Db::dbDataToRclDoc: data: %s\n", data.c_str()));
    ConfSimple parms(&data);
    if (!parms.ok())
	return false;
    parms.get(Doc::keyurl, doc.url);
    parms.get(Doc::keytp, doc.mimetype);
    parms.get(Doc::keyfmt, doc.fmtime);
    parms.get(Doc::keydmt, doc.dmtime);
    parms.get(Doc::keyoc, doc.origcharset);
    parms.get(keycap, doc.meta[Doc::keytt]);
    parms.get(Doc::keykw, doc.meta[Doc::keykw]);
    parms.get(Doc::keyabs, doc.meta[Doc::keyabs]);
    // Possibly remove synthetic abstract indicator (if it's there, we
    // used to index the beginning of the text as abstract).
    doc.syntabs = false;
    if (doc.meta[Doc::keyabs].find(rclSyntAbs) == 0) {
	doc.meta[Doc::keyabs] = doc.meta[Doc::keyabs].substr(rclSyntAbs.length());
	doc.syntabs = true;
    }
    char buf[20];
    sprintf(buf,"%.2f", float(percent) / 100.0);
    doc.pc = percent;
    doc.meta[Doc::keyrr] = buf;
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
    return true;
}

static list<string> noPrefixList(const list<string>& in) 
{
    list<string> out;
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
    return out;
}

    //#define DEBUGABSTRACT  1
#ifdef DEBUGABSTRACT
#define LOGABS LOGDEB
#else
#define LOGABS LOGDEB2
#endif

// Build a document abstract by extracting text chunks around the query terms
// This uses the db termlists, not the original document.
string Db::Native::makeAbstract(Xapian::docid docid, Query *query)
{
    Chrono chron;
    LOGDEB(("makeAbstract:%d: maxlen %d wWidth %d\n", chron.ms(),
	     m_db->m_synthAbsLen, m_db->m_synthAbsWordCtxLen));

    list<string> iterms;
    query->getQueryTerms(iterms);

    list<string> terms = noPrefixList(iterms);
    if (terms.empty()) {
	return string();
    }

    // Retrieve db-wide frequencies for the query terms
    if (query->m_nq->termfreqs.empty()) {
	double doccnt = db.get_doccount();
	if (doccnt == 0) doccnt = 1;
	for (list<string>::const_iterator qit = terms.begin(); 
	     qit != terms.end(); qit++) {
	    query->m_nq->termfreqs[*qit] = db.get_termfreq(*qit) / doccnt;
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
    double doclen = db.get_doclength(docid);
    if (doclen == 0) doclen = 1;
    for (list<string>::const_iterator qit = terms.begin(); 
	 qit != terms.end(); qit++) {
	Xapian::TermIterator term = db.termlist_begin(docid);
	term.skip_to(*qit);
	if (term != db.termlist_end(docid) && *term == *qit) {
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
	m_db->m_synthAbsLen /(7 * (m_db->m_synthAbsWordCtxLen+1));
    LOGABS(("makeAbstract:%d: mxttloccs %d\n", chron.ms(), maxtotaloccs));
    // This can't happen, but would crash us
    if (totalweight == 0.0) {
	LOGERR(("makeAbstract: 0 totalweight!\n"));
	return string();
    }

    // This is used to mark positions overlapped by a multi-word match term
    const string occupiedmarker("?");
    const string ellipsis("...");

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
	    for (pos = db.positionlist_begin(docid, qterm); 
		 pos != db.positionlist_end(docid, qterm); pos++) {
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
		unsigned int sta = MAX(0, ipos-m_db->m_synthAbsWordCtxLen);
		unsigned int sto = ipos + qtrmwrdcnt-1 + 
		    m_db->m_synthAbsWordCtxLen;
		for (unsigned int ii = sta; ii <= sto;  ii++) {
		    if (ii == (unsigned int)ipos) {
			sparseDoc[ii] = qterm;
		    } else if (ii > (unsigned int)ipos && 
			       ii < (unsigned int)ipos + qtrmwrdcnt) {
			sparseDoc[ii] = occupiedmarker;
		    } else if (!sparseDoc[ii].compare(ellipsis)) {
			// For an empty slot, the test has a side
			// effect of inserting an empty string which
			// is what we want
			sparseDoc[ii] = emptys;
		    }
		}
		// Add ... at the end. This may be replaced later by
		// an overlapping extract. Take care not to replace an
		// empty string here, we really want an empty slot,
		// use find()
		if (sparseDoc.find(sto+1) == sparseDoc.end()) {
		    sparseDoc[sto+1] = ellipsis;
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
    if (qtermposs.size() == 0) 
	return string();

    // Walk all document's terms position lists and populate slots
    // around the query terms. We arbitrarily truncate the list to
    // avoid taking forever. If we do cutoff, the abstract may be
    // inconsistant (missing words, potentially altering meaning),
    // which is bad...
    { 
	Xapian::TermIterator term;
	int cutoff = 500 * 1000;

	for (term = db.termlist_begin(docid);
	     term != db.termlist_end(docid); term++) {
	    // Ignore prefixed terms
	    if ('A' <= (*term).at(0) && (*term).at(0) <= 'Z')
		continue;
	    if (cutoff-- < 0) {
		LOGDEB(("makeAbstract: max term count cutoff\n"));
		break;
	    }

	    Xapian::PositionIterator pos;
	    for (pos = db.positionlist_begin(docid, *term); 
		 pos != db.positionlist_end(docid, *term); pos++) {
		if (cutoff-- < 0) {
		    LOGDEB(("makeAbstract: max term count cutoff\n"));
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

    LOGDEB(("makeAbstract:%d: extracting\n", chron.millis()));

    // Finally build the abstract by walking the map (in order of position)
    string abstract;
    abstract.reserve(sparseDoc.size() * 10);
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
	    abstract += " ";
	incjk = newcjk;
	abstract += it->second;
    }

    // This happens for docs with no terms (only filename) indexed? I'll fix 
    // one day (yeah)
    if (!abstract.compare("... "))
	abstract.clear();

    LOGDEB(("makeAbtract: done in %d mS\n", chron.millis()));
    return abstract;
}

/* Rcl::Db methods ///////////////////////////////// */

Db::Db(RclConfig *cfp)
    : m_ndb(0), m_config(cfp), m_idxAbsTruncLen(250), m_synthAbsLen(250),
      m_synthAbsWordCtxLen(4), m_flushMb(-1), 
      m_curtxtsz(0), m_flushtxtsz(0), m_occtxtsz(0),
      m_maxFsOccupPc(0), m_mode(Db::DbRO)
{
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

bool Db::open(OpenMode mode, bool keep_updated)
{
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
		m_ndb->wdb = Xapian::WritableDatabase(dir, action);
		m_ndb->m_iswritable = true;
		// We open a readonly object in all cases (possibly in
		// addition to the r/w one) because some operations
		// are faster when performed through a Database: no
		// forced flushes on allterms_begin(), ie, used in
		// subDocs()
		m_ndb->db = Xapian::Database(dir);
		LOGDEB(("Db::open: lastdocid: %d\n", 
			m_ndb->wdb.get_lastdocid()));
		if (!keep_updated) {
		    LOGDEB2(("Db::open: resetting updated\n"));
		    updated.resize(m_ndb->wdb.get_lastdocid() + 1);
		    for (unsigned int i = 0; i < updated.size(); i++)
			updated[i] = false;
		}
	    }
	    break;
	case DbRO:
	default:
	    m_ndb->m_iswritable = false;
	    m_ndb->db = Xapian::Database(dir);
	    for (list<string>::iterator it = m_extraDbs.begin();
		 it != m_extraDbs.end(); it++) {
		string aerr;
		LOGDEB(("Db::Open: adding query db [%s]\n", it->c_str()));
		aerr.erase();
		try {
		    // Make this non-fatal
		    m_ndb->db.add_database(Xapian::Database(*it));
		} XCATCHERROR(aerr);
		if (!aerr.empty())
		    LOGERR(("Db::Open: error while trying to add database "
			    "from [%s]: %s\n", it->c_str(), aerr.c_str()));
	    }
	    break;
	}
	// Check index format version. Must not try to check a just created or
	// truncated db
	if (mode != DbTrunc && m_ndb->db.get_doccount()>0) {
	    Xapian::Database cdb = m_ndb->m_iswritable ? m_ndb->wdb: m_ndb->db;
	    string version = cdb.get_metadata(RCL_IDX_VERSION_KEY);
	    if (version.compare(RCL_IDX_VERSION)) {
		m_ndb->m_noversionwrite = true;
		LOGERR(("Rcl::Db::open: file index [%s], software [%s]\n",
			version.c_str(), RCL_IDX_VERSION.c_str()));
		throw Xapian::DatabaseError("Recoll index version mismatch",
					    "", "");
	    }
	}
	m_mode = mode;
	m_ndb->m_isopen = true;
	m_basedir = dir;
	return true;
    } XCATCHERROR(ermsg);
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
		m_ndb->wdb.set_metadata(RCL_IDX_VERSION_KEY, RCL_IDX_VERSION);
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

bool Db::reOpen()
{
    if (m_ndb && m_ndb->m_isopen) {
	if (!close())
	    return false;
	if (!open(m_mode, true)) {
	    return false;
	}
    }
    return true;
}

int Db::docCnt()
{
    int res = -1;
    string ermsg;
    if (m_ndb && m_ndb->m_isopen) {
	try {
	    res = m_ndb->m_iswritable ? m_ndb->wdb.get_doccount() : 
		m_ndb->db.get_doccount();
	} catch (const Xapian::DatabaseModifiedError &e) {
	    LOGDEB(("Db::docCnt: got modified error. reopen/retry\n"));
	    reOpen();
	    res = m_ndb->m_iswritable ? m_ndb->wdb.get_doccount() : 
		m_ndb->db.get_doccount();
	} XCATCHERROR(ermsg);
	if (!ermsg.empty())
	    LOGERR(("Db::docCnt: got error: %s\n", ermsg.c_str()));
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
    return reOpen();
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
    return reOpen();
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
bool Db::fieldToPrefix(const string& fld, string &pfx)
{
    // This is the default table. We prefer the data from rclconfig if 
    // available
    static map<string, string> fldToPrefs;
    if (fldToPrefs.empty()) {
	fldToPrefs[Doc::keyabs] = string();
	fldToPrefs["ext"] = "XE";
	fldToPrefs[Doc::keyfn] = "XSFN";

	fldToPrefs[keycap] = "S";
	fldToPrefs[Doc::keytt] = "S";
	fldToPrefs["subject"] = "S";

	fldToPrefs[Doc::keyau] = "A";
	fldToPrefs["creator"] = "A";
	fldToPrefs["from"] = "A";

	fldToPrefs[Doc::keykw] = "K";
	fldToPrefs["keyword"] = "K";
	fldToPrefs["tag"] = "K";
	fldToPrefs["tags"] = "K";
    }

    if (m_config && m_config->getFieldPrefix(fld, pfx))
	return true;

    // No data in rclconfig? Check default values
    map<string, string>::const_iterator it = fldToPrefs.find(fld);
    if (it != fldToPrefs.end()) {
	pfx = it->second;
	return true;
    }
    return false;
}


// The text splitter callback class which receives words from the
// splitter and adds postings to the Xapian document.
class mySplitterCB : public TextSplitCB {
 public:
    Xapian::Document &doc;   // Xapian document 
    Xapian::termpos basepos; // Base for document section
    Xapian::termpos curpos;  // Current position. Used to set basepos for the
                             // following section
    StopList &stops;
    mySplitterCB(Xapian::Document &d, StopList &_stops) 
	: doc(d), basepos(1), curpos(0), stops(_stops)
    {}
    bool takeword(const std::string &term, int pos, int, int);
    void setprefix(const string& pref) {prefix = pref;}

private:
    // If prefix is set, we also add a posting for the prefixed terms
    // (ie: for titles, add postings for both "term" and "Sterm")
    string  prefix; 
};

// Callback for the document to word splitting class during indexation
bool mySplitterCB::takeword(const std::string &_term, int pos, int, int)
{
#if 0
    LOGDEB(("mySplitterCB::takeword:splitCb: [%s]\n", _term.c_str()));
#endif
    string term;
    if (!unacmaybefold(_term, term, "UTF-8", true)) {
	LOGINFO(("Db::splitter::takeword: unac failed for [%s]\n", _term.c_str()));
	term.clear();
	// We don't generate a fatal error because of a bad term
	return true;
    }

    if (stops.hasStops() && stops.isStop(term)) {
	LOGDEB1(("Db: takeword [%s] in stop list\n", term.c_str()));
	return true;
    }

    // Compute absolute position (pos is relative to current segment),
    // and remember relative.
    curpos = pos;
    pos += basepos;
    string ermsg;
    try {
	// Note: 1 is the within document frequency increment. It would 
	// be possible to assign different weigths to doc parts (ie title)
	// by using a higher value
	doc.add_posting(term, pos, 1);
	if (!prefix.empty()) {
	    doc.add_posting(prefix + term, pos, 1);
	}
	return true;
    } XCATCHERROR(ermsg);
    LOGERR(("Db: xapian add_posting error %s\n", ermsg.c_str()));
    return false;
}

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

static inline void leftzeropad(string& s, unsigned len)
{
    if (s.length() && s.length() < len)
	s = s.insert(0, len-s.length(), '0');
}

static const int MB = 1024 * 1024;
static const string nc("\n\r\x0c");

#define RECORD_APPEND(R, NM, VAL) {R += NM + "=" + VAL + "\n";}

// Add document in internal form to the database: index the terms in
// the title abstract and body and add special terms for file name,
// date, mime type ... , create the document data record (more
// metadata), and update database
bool Db::addOrUpdate(const string &udi, const string &parent_udi,
		     const Doc &idoc)
{
    LOGDEB(("Db::add: udi [%s] parent [%s]\n", 
	     udi.c_str(), parent_udi.c_str()));
    if (m_ndb == 0)
	return false;
    static int first = 1;
    // Check file system full every mbyte of indexed text.
    if (m_maxFsOccupPc > 0 && (first || (m_curtxtsz - m_occtxtsz) / MB >= 1)) {
	LOGDEB(("Db::add: checking file system usage\n"));
	int pc;
	first = 0;
	if (fsocc(m_basedir, &pc) && pc >= m_maxFsOccupPc) {
	    LOGERR(("Db::add: stop indexing: file system "
		     "%d%% full > max %d%%\n", pc, m_maxFsOccupPc));
	    return false;
	}
	m_occtxtsz = m_curtxtsz;
    }

    Doc doc = idoc;

    Xapian::Document newdocument;
    mySplitterCB splitData(newdocument, m_stops);
    TextSplit splitter(&splitData);

    // Split and index file name as document term(s)
    LOGDEB2(("Db::add: split file name [%s]\n", fn.c_str()));
    splitter.text_to_words(doc.utf8fn);
    splitData.basepos += splitData.curpos + 100;

    // Index textual metadata.  These are all indexed as text with
    // positions, as we may want to do phrase searches with them (this
    // makes no sense for keywords by the way).
    //
    // The order has no importance, and we set a position gap of 100
    // between fields to avoid false proximity matches.
    map<string,string>::iterator meta_it;
    string pfx;
    for (meta_it = doc.meta.begin(); meta_it != doc.meta.end(); meta_it++) {
	if (!meta_it->second.empty()) {
	    if (!fieldToPrefix(meta_it->first, pfx)) {
		LOGDEB0(("Db::add: no prefix for field [%s], no indexing\n",
			 meta_it->first.c_str()));
		continue;
	    }
	    LOGDEB0(("Db::add: field [%s] pfx [%s]: [%s]\n", 
		    meta_it->first.c_str(), pfx.c_str(), 
		    meta_it->second.c_str()));
	    splitData.setprefix(pfx); // Subject
	    splitter.text_to_words(meta_it->second);
	    splitData.setprefix(string());
	    splitData.basepos += splitData.curpos + 100;
	}
    }

    if (splitData.curpos < baseTextPosition)
	splitData.basepos = baseTextPosition;
    else
	splitData.basepos += splitData.curpos + 100;

    // Split and index body text
    LOGDEB2(("Db::add: split body\n"));
    splitter.text_to_words(doc.text);

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
    // Dates etc...
    time_t mtime = atol(doc.dmtime.empty() ? doc.fmtime.c_str() : 
			doc.dmtime.c_str());
    struct tm *tm = localtime(&mtime);
    char buf[9];
    sprintf(buf, "%04d%02d%02d",tm->tm_year+1900, tm->tm_mon + 1, tm->tm_mday);
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
	neutchars(truncate_to_word(doc.meta[Doc::keytt], 150), nc);
    if (!doc.meta[Doc::keytt].empty())
	RECORD_APPEND(record, keycap, doc.meta[Doc::keytt]);

    doc.meta[Doc::keykw] = 
	neutchars(truncate_to_word(doc.meta[Doc::keykw], 300), nc);
    if (!doc.meta[Doc::keykw].empty())
	RECORD_APPEND(record, Doc::keykw, doc.meta[Doc::keykw]);

    // If abstract is empty, we make up one with the beginning of the
    // document. This is then not indexed, but part of the doc data so
    // that we can return it to a query without having to decode the
    // original file.
    bool syntabs = false;
    // Note that the map accesses by operator[] create empty entries if they
    // don't exist yet.
    if (doc.meta[Doc::keyabs].empty()) {
	syntabs = true;
	if (!doc.text.empty())
	    doc.meta[Doc::keyabs] = rclSyntAbs + 
		neutchars(truncate_to_word(doc.text, m_idxAbsTruncLen), nc);
    } else {
	doc.meta[Doc::keyabs] = 
	    neutchars(truncate_to_word(doc.meta[Doc::keyabs], m_idxAbsTruncLen),
		      nc);
    }
    if (!doc.meta[Doc::keyabs].empty())
	RECORD_APPEND(record, Doc::keyabs, doc.meta[Doc::keyabs]);

    const set<string>& stored = m_config->getStoredFields();
    for (set<string>::const_iterator it = stored.begin();
	 it != stored.end(); it++) {
	string nm = m_config->fieldCanon(*it);
	if (!doc.meta[*it].empty()) {
	    string value = 
		neutchars(truncate_to_word(doc.meta[*it], 150), nc);
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
	    m_ndb->wdb.replace_document(uniterm, newdocument);
	if (did < updated.size()) {
	    updated[did] = true;
	    LOGDEB(("Db::add: docid %d updated [%s]\n", did, fnc));
	} else {
	    LOGDEB(("Db::add: docid %d added [%s]\n", did, fnc));
	}
    } XCATCHERROR(ermsg);

    if (!ermsg.empty()) {
	LOGERR(("Db::add: replace_document failed: %s\n", ermsg.c_str()));
	ermsg.erase();
	// FIXME: is this ever actually needed?
	try {
	    m_ndb->wdb.add_document(newdocument);
	    LOGDEB(("Db::add: %s added (failed re-seek for duplicate)\n", 
		    fnc));
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR(("Db::add: add_document failed: %s\n", ermsg.c_str()));
	    return false;
	}
    }

    // Test if we're over the flush threshold (limit memory usage):
    m_curtxtsz += doc.text.length();
    if (m_flushMb > 0) {
	if ((m_curtxtsz - m_flushtxtsz) / MB >= m_flushMb) {
	    ermsg.erase();
	    LOGDEB(("Db::add: text size >= %d Mb, flushing\n", m_flushMb));
	    try {
		m_ndb->wdb.flush();
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

    // We try twice in case database needs to be reopened. (Ulterior
    // note: this does not make sense as we are the sole writer!)
    for (int tries = 0; tries < 2; tries++) {
	try {
	    // Get the doc or pseudo-doc
	    Xapian::PostingIterator docid = m_ndb->db.postlist_begin(uniterm);
	    if (docid == m_ndb->db.postlist_end(uniterm)) {
		// If no document exist with this path, we do need update
		LOGDEB(("Db::needUpdate: no path: [%s]\n", uniterm.c_str()));
		return true;
	    }
	    Xapian::Document doc = m_ndb->db.get_document(*docid);

	    // Retrieve old file/doc signature from value
	    string osig = doc.get_value(VALUE_SIG);
	    LOGDEB2(("Db::needUpdate: oldsig [%s] new [%s]\n",
		     osig.c_str(), sig.c_str()));
	    // Compare new/old sig
	    if (sig != osig) {
		LOGDEB(("Db::needUpdate:yes: olsig [%s] new [%s]\n",
			osig.c_str(), sig.c_str()));
		// Db is not up to date. Let's index the file
		return true;
	    } 

	    LOGDEB(("Db::needUpdate: uptodate: [%s]\n", uniterm.c_str()));

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
	    reOpen();
	} XCATCHERROR(ermsg);
	if (!ermsg.empty())
	    break;
    }
    LOGERR(("Db::needUpdate: error while checking existence: %s\n", 
	    ermsg.c_str()));
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

    return StemDb::createDb(m_ndb->m_iswritable ? m_ndb->wdb : m_ndb->db, 
			     m_basedir, lang);
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
	m_ndb->wdb.flush();
    } catch (...) {
	LOGERR(("Db::purge: 1st flush failed\n"));

    }

    // Walk the document array and delete any xapian document whose
    // flag is not set (we did not see its source during indexing).
    for (Xapian::docid docid = 1; docid < updated.size(); ++docid) {
	if (!updated[docid]) {
	    try {
		m_ndb->wdb.delete_document(docid);
		LOGDEB(("Db::purge: deleted document #%d\n", docid));
	    } catch (const Xapian::DocNotFoundError &) {
		LOGDEB0(("Db::purge: document #%d not found\n", docid));
	    } catch (const Xapian::Error &e) {
		LOGERR(("Db::purge: document #%d: %s\n", docid, e.get_msg().c_str()));
	    } catch (...) {
		LOGERR(("Db::purge: document #%d: unknown error\n", docid));
	    }
	}
    }

    try {
	m_ndb->wdb.flush();
    } catch (...) {
	LOGERR(("Db::purge: 2nd flush failed\n"));
    }
    return true;
}

/* Delete document(s) for given unique identifier (doc and descendents) */
bool Db::purgeFile(const string &udi)
{
    LOGDEB(("Db:purgeFile: [%s]\n", udi.c_str()));
    if (m_ndb == 0)
	return false;
    Xapian::WritableDatabase db = m_ndb->wdb;
    string uniterm = make_uniterm(udi);
    string ermsg;
    try {
	Xapian::PostingIterator docid = db.postlist_begin(uniterm);
	if (docid == db.postlist_end(uniterm))
	    return true;
	LOGDEB(("purgeFile: delete docid %d\n", *docid));
	db.delete_document(*docid);
	vector<Xapian::docid> docids;
	m_ndb->subDocs(udi, docids);
	LOGDEB(("purgeFile: subdocs cnt %d\n", docids.size()));
	for (vector<Xapian::docid>::iterator it = docids.begin();
	     it != docids.end(); it++) {
	    LOGDEB(("Db::purgeFile: delete subdoc %d\n", *it));
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
    string pattern;
    if (!unacmaybefold(fnexp, pattern, "UTF-8", true)) {
	LOGERR(("Db::filenameWildExp: unac error for [%s]\n", fnexp.c_str()));
	return false;
    }
    names.clear();

    // If pattern is not quoted, and has no wildcards, we add * at
    // each end: match any substring
    if (pattern[0] == '"' && pattern[pattern.size()-1] == '"') {
	pattern = pattern.substr(1, pattern.size() -2);
    } else if (pattern.find_first_of("*?[") == string::npos) {
	pattern = "*" + pattern + "*";
    } // else let it be
    LOGDEB(("Rcl::Db::filenameWildExp: pattern: [%s]\n", pattern.c_str()));

    list<TermMatchEntry> entries;
    if (!termMatch(ET_WILD, string(), pattern, entries, 1000, Doc::keyfn))
	return false;
    for (list<TermMatchEntry>::const_iterator it = entries.begin();
	 it != entries.end(); it++) 
	names.push_back("XSFN"+it->term);

    if (names.empty()) {
	// Build an impossible query: we know its impossible because we
	// control the prefixes!
	names.push_back("XIMPOSSIBLE");
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
		    list<TermMatchEntry>& result, int max)
{
    list<string> dirs = m_extraDbs;
    dirs.push_front(m_basedir);
    for (list<string>::iterator it = dirs.begin();
	 it != dirs.end(); it++) {
	list<string> more;
	StemDb::stemExpand(*it, lang, term, more);
	LOGDEB1(("Db::stemExpand: Got %d from %s\n", 
		 more.size(), it->c_str()));
	result.insert(result.end(), more.begin(), more.end());
	if (result.size() >= (unsigned int)max)
	    break;
    }
    LOGDEB1(("Db:::stemExpand: final count %d \n", result.size()));
    return true;
}

// Characters that can begin a wildcard or regexp expression. We use skipto
// to begin the allterms search with terms that begin with the portion of
// the input string prior to these chars.
const string wildSpecChars = "*?[";
const string regSpecChars = "(.[{";

// Find all index terms that match a wildcard or regular expression
bool Db::termMatch(MatchType typ, const string &lang,
		   const string &root, 
		   list<TermMatchEntry>& res,
		   int max, 
		   const string& field)
{
    if (!m_ndb || !m_ndb->m_isopen)
	return false;
    Xapian::Database db = m_ndb->m_iswritable ? m_ndb->wdb: m_ndb->db;

    res.clear();

    // Get rid of capitals and accents
    string droot;
    if (!unacmaybefold(root, droot, "UTF-8", true)) {
	LOGERR(("Db::termMatch: unac failed for [%s]\n", root.c_str()));
	return false;
    }
    string nochars = typ == ET_WILD ? wildSpecChars : regSpecChars;

    string prefix;
    if (!field.empty()) {
	(void)fieldToPrefix(field, prefix); 
    }

    if (typ == ET_STEM) {
	if (!stemExpand(lang, root, res, max))
	    return false;
	res.sort();
	res.unique();
	for (list<TermMatchEntry>::iterator it = res.begin(); 
	     it != res.end(); it++) {
	    it->wcf = db.get_collection_freq(it->term);
	    LOGDEB1(("termMatch: %d [%s]\n", it->wcf, it->term.c_str()));
	}
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
		res.push_back(string(errbuf));
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

	string ermsg;
	try {
	    Xapian::TermIterator it = db.allterms_begin(); 
	    if (!is.empty())
		it.skip_to(is.c_str());
	    for (int n = 0; it != db.allterms_end(); it++) {
		// If we're beyond the terms matching the initial string, end
		if (!is.empty() && (*it).find(is) != 0)
		    break;
		string term;
		if (!prefix.empty())
		    term = (*it).substr(prefix.length());
		else
		    term = *it;
		if (typ == ET_WILD) {
		    if (fnmatch(droot.c_str(), term.c_str(), 0) == FNM_NOMATCH)
			continue;
		} else {
		    if (regexec(&reg, term.c_str(), 0, 0, 0))
			continue;
		}
		// Do we want stem expansion here? We don't do it for now
		res.push_back(TermMatchEntry(term, it.get_termfreq()));
		++n;
	    }
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR(("termMatch: %s\n", ermsg.c_str()));
	    return false;
	}

	if (typ == ET_REGEXP) {
	    regfree(&reg);
	}

    }

    TermMatchCmpByTerm tcmp;
    res.sort(tcmp);
    TermMatchTermEqual teq;
    res.unique(teq);
    TermMatchCmpByWcf wcmp;
    res.sort(wcmp);
    if (max > 0) {
	res.resize(MIN(res.size(), (unsigned int)max));
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
	tit->db = m_ndb->m_iswritable ? m_ndb->wdb: m_ndb->db;
	string ermsg;
	try {
	    tit->it = tit->db.allterms_begin();
	} XCATCHERROR(ermsg);
	if (!ermsg.empty()) {
	    LOGERR(("Db::termWalkOpen: xapian error: %s\n", ermsg.c_str()));
	    return 0;
	}
    }
    return tit;
}
bool Db::termWalkNext(TermIter *tit, string &term)
{
    string ermsg;
    try {
	if (tit && tit->it != tit->db.allterms_end()) {
	    term = *(tit->it)++;
	    return true;
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db::termWalkOpen: xapian error: %s\n", ermsg.c_str()));
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
    Xapian::Database db = m_ndb->m_iswritable ? m_ndb->wdb: m_ndb->db;
    string ermsg;
    try {
	if (!db.term_exists(word))
	    return false;
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db::termWalkOpen: xapian error: %s\n", ermsg.c_str()));
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


bool Db::makeDocAbstract(Doc &doc, Query *query, string& abstract)
{
    LOGDEB1(("Db::makeDocAbstract: exti %d\n", exti));
    if (!m_ndb) {
	LOGERR(("Db::makeDocAbstract: no db\n"));
	return false;
    }
    m_reason.erase();
    for (int i = 0; i < 2; i++) {
        try {
            abstract = m_ndb->makeAbstract(doc.xdocid, query);
            m_reason.erase();
            break;
        } catch (const Xapian::DatabaseModifiedError &error) {
            LOGDEB(("Db:makeDocAbstract: caught DatabaseModified\n"));
            m_reason = error.get_msg();
            reOpen();
        } catch (const Xapian::Error & error) {
            LOGERR(("Db::makeDocAbstract: exception: %s\n", 
                    error.get_msg().c_str()));
            m_reason = error.get_msg();
            break;
        }
    }
    return m_reason.empty() ? true : false;
}

// Retrieve document defined by file name and internal path. 
bool Db::getDoc(const string &udi, Doc &doc)
{
    LOGDEB(("Db:getDoc: [%s]\n", udi.c_str()));
    if (m_ndb == 0)
	return false;

    // Initialize what we can in any case. If this is history, caller
    // will make partial display in case of error
    doc.pc = 100;

    string uniterm = make_uniterm(udi);
    string ermsg;
    try {
	if (!m_ndb->db.term_exists(uniterm)) {
	    // Document found in history no longer in the database.
	    // We return true (because their might be other ok docs further)
	    // but indicate the error with pc = -1
	    doc.pc = -1;
	    LOGINFO(("Db:getDoc: no such doc in index: [%s] (len %d)\n",
		     uniterm.c_str(), uniterm.length()));
	    return true;
	}
	Xapian::PostingIterator docid = m_ndb->db.postlist_begin(uniterm);
	Xapian::Document xdoc = m_ndb->db.get_document(*docid);
	string data = xdoc.get_data();
	return m_ndb->dbDataToRclDoc(*docid, data, doc, 100);
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("Db::getDoc: %s\n", ermsg.c_str()));
    }
    return false;
}

#ifndef NO_NAMESPACES
}
#endif
