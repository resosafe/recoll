#ifndef lint
static char rcsid[] = "@(#$Id: stemdb.cpp,v 1.12 2008-07-30 13:16:13 dockes Exp $ (C) 2005 J.F.Dockes";
#endif

/**
 * Management of the auxiliary databases listing stems and their expansion 
 * terms
 */
#include <unistd.h>

#include <algorithm>
#include <map>

#include <xapian.h>
#include <xapian/stem.h>

#include "stemdb.h"
#include "wipedir.h"
#include "pathut.h"
#include "debuglog.h"
#include "smallut.h"

using namespace std;

namespace Rcl {
namespace StemDb {


const static string stemdirstem = "stem_";

/// Compute name of stem db for given base database and language
static string stemdbname(const string& dbdir, const string& lang)
{
    return path_cat(dbdir, stemdirstem + lang);
}

list<string> getLangs(const string& dbdir)
{
    string pattern = stemdirstem + "*";
    list<string> dirs = path_dirglob(dbdir, pattern);
    for (list<string>::iterator it = dirs.begin(); it != dirs.end(); it++) {
	*it = path_basename(*it);
	*it = it->substr(stemdirstem.length(), string::npos);
    }
    return dirs;
}

bool deleteDb(const string& dbdir, const string& lang)
{
    string dir = stemdbname(dbdir, lang);
    if (wipedir(dir) == 0 && rmdir(dir.c_str()) == 0)
	return true;
    return false;
}

// Autoclean/delete directory 
class DirWiper {
 public:
    string dir;
    bool do_it;
    DirWiper(string d) : dir(d), do_it(true) {}
    ~DirWiper() {
	if (do_it) {
	    wipedir(dir);
	    rmdir(dir.c_str());
	}
    }
};

inline static bool
p_notlowerascii(unsigned int c)
{
    if (c < 'a' || (c > 'z' && c < 128))
	return true;
    return false;
}

static bool addAssoc(Xapian::WritableDatabase &sdb, const string& stem,
                     const list<string>& derivs)
{
    Xapian::Document newdocument;
    newdocument.add_term(stem);
    // The doc data is just parents=blank-separated-list
    string record = "parents=";
    for (list<string>::const_iterator it = derivs.begin(); 
         it != derivs.end(); it++) {
        record += *it + " ";
    }
    record += "\n";
    LOGDEB2(("createStemDb: stmdoc data: [%s]\n", record.c_str()));
    newdocument.set_data(record);
    try {
        sdb.replace_document(stem, newdocument);
    } catch (...) {
        LOGERR(("Db::createstemdb(addAssoc): replace failed\n"));
        return false;
    }
    return true;
}


/**
 * Create database of stem to parents associations for a given language.
 * We walk the list of all terms, stem them, and create another Xapian db
 * with documents indexed by a single term (the stem), and with the list of
 * parent terms in the document data.
 */
bool createDb(Xapian::Database& xdb, const string& dbdir, const string& lang)
{
    LOGDEB(("StemDb::createDb(%s)\n", lang.c_str()));
    Chrono cron;

    // First build the in-memory stem database:
    // We walk the list of all terms, and stem each. 
    //   If the stem is identical to the term, no need to create an entry
    // Else, we add an entry to the multimap.
    // At the end, we only save stem-terms associations with several terms, the
    // others are not useful
    // Note: a map<string, list<string> > would probably be more efficient
    multimap<string, string> assocs;
    // Statistics
    int nostem=0; // Dont even try: not-alphanum (incomplete for now)
    int stemconst=0; // Stem == term
    int stemdiff=0;  // Count of all different stems
    int stemmultiple = 0; // Count of stems with multiple derivatives
    try {
        Xapian::Stem stemmer(lang);
        Xapian::TermIterator it;
        for (it = xdb.allterms_begin(); it != xdb.allterms_end(); it++) {
            // Deciding if we try to stem the term. If it has any
            // non-lowercase 7bit char (that is, numbers, capitals and
            // punctuation) dont. We're still sending all multibyte
            // utf-8 chars to the stemmer, which is not too well
            // defined for xapian < 1.0, but seems to work anyway. We don't
            // try to look for multibyte non alphabetic data.
            string::iterator sit = (*it).begin(), eit = sit + (*it).length();
            if ((sit = find_if(sit, eit, p_notlowerascii)) != eit) {
                ++nostem;
                LOGDEB1(("stemskipped: [%s], because of 0x%x\n", 
                         (*it).c_str(), *sit));
                continue;
            }
            string stem = stemmer(*it);
            LOGDEB2(("Db::createStemDb: word [%s], stem [%s]\n", (*it).c_str(),
                     stem.c_str()));
            if (stem == *it) {
                ++stemconst;
                continue;
            }
            assocs.insert(pair<string,string>(stem, *it));
        }
    } catch (const Xapian::Error &e) {
        LOGERR(("Db::createStemDb: build failed: %s\n", e.get_msg().c_str()));
        return false;
    } catch (...) {
        LOGERR(("Db::createStemDb: build failed: no stemmer for %s ? \n", 
                lang.c_str()));
        return false;
    }
    LOGDEB1(("StemDb::createDb(%s): in memory map built: %.2f S\n", 
             lang.c_str(), cron.secs()));

    // Create xapian database for stem relations
    string stemdbdir = stemdbname(dbdir, lang);
    // We want to get rid of the db dir in case of error. This gets disarmed
    // just before success return.
    DirWiper wiper(stemdbdir);
    string ermsg;
    Xapian::WritableDatabase sdb;
    try {
        sdb = Xapian::WritableDatabase(stemdbdir, 
                                       Xapian::DB_CREATE_OR_OVERWRITE);
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
        LOGERR(("Db::createstemdb: exception while opening [%s]: %s\n", 
                stemdbdir.c_str(), ermsg.c_str()));
        return false;
    }

    // Enter pseud-docs in db by walking the multimap.
    string stem;
    list<string> derivs;
    for (multimap<string,string>::const_iterator it = assocs.begin();
         it != assocs.end(); it++) {
        if (stem == it->first) {
            // Staying with same stem
            derivs.push_back(it->second);
            // cerr << " " << it->second << endl;
        } else {
            // Changing stems 
            ++stemdiff;
            LOGDEB2(("createStemDb: stem [%s]\n", stem.c_str()));

            // We need an entry even if there is only one derivative
            // so that it is possible to search by entering the stem
            // even if it doesnt exist as a term
            if (!derivs.empty()) {

                if (derivs.size() > 1)
                    ++stemmultiple;
                    
                if (!addAssoc(sdb, stem, derivs)) {
                    return false;
                }
                derivs.clear();
            }
            stem = it->first;
            derivs.push_back(it->second);
            //	    cerr << "\n" << stem << " " << it->second;
        }
    }
    if (!derivs.empty()) {
        if (derivs.size() > 1)
            ++stemmultiple;
                    
        if (!addAssoc(sdb, stem, derivs)) {
            return false;
        }
    }

    LOGDEB1(("StemDb::createDb(%s): done: %.2f S\n", 
             lang.c_str(), cron.secs()));
    LOGDEB(("Stem map size: %d stems %d mult %d no %d const %d\n", 
	    assocs.size(), stemdiff, stemmultiple, nostem, stemconst));
    wiper.do_it = false;
    return true;
}

static string stringlistdisp(const list<string>& sl)
{
    string s;
    for (list<string>::const_iterator it = sl.begin(); it!= sl.end(); it++)
	s += "[" + *it + "] ";
    if (!s.empty())
	s.erase(s.length()-1);
    return s;
}

/**
 * Expand term to list of all terms which stem to the same term, for one
 * expansion language
 */
static bool stemExpandOne(const std::string& dbdir, 
			  const std::string& lang,
			  const std::string& term,
			  list<string>& result)
{
    try {
	Xapian::Stem stemmer(lang);
	string stem = stemmer(term);
	LOGDEB(("stemExpand:%s: [%s] stem-> [%s]\n", 
                lang.c_str(), term.c_str(), stem.c_str()));

	// Open stem database
	string stemdbdir = stemdbname(dbdir, lang);
	Xapian::Database sdb(stemdbdir);
	LOGDEB0(("stemExpand: %s lastdocid: %d\n", 
		stemdbdir.c_str(), sdb.get_lastdocid()));

	// Try to fetch the doc from the stem db
	if (!sdb.term_exists(stem)) {
	    LOGDEB0(("Db::stemExpand: no term for %s\n", stem.c_str()));
	} else {
	    Xapian::PostingIterator did = sdb.postlist_begin(stem);
	    if (did == sdb.postlist_end(stem)) {
		LOGDEB0(("stemExpand: no term(1) for %s\n",stem.c_str()));
	    } else {
		Xapian::Document doc = sdb.get_document(*did);
		string data = doc.get_data();

		// Build expansion list from database data No need for
		// a conftree, but we need to massage the data a
		// little
		string::size_type pos = data.find_first_of("=");
		++pos;
		string::size_type pos1 = data.find_last_of("\n");
		if (pos == string::npos || pos1 == string::npos || 
		    pos1 <= pos) {
		    // ??
		} else {
		    stringToStrings(data.substr(pos, pos1-pos), result);
		}
	    }
	}

	// If the user term or stem are not in the list, add them
	if (find(result.begin(), result.end(), term) == result.end()) {
	    result.push_back(term);
	}
	if (find(result.begin(), result.end(), stem) == result.end()) {
	    result.push_back(stem);
	}
	LOGDEB0(("stemExpand:%s: %s ->  %s\n", lang.c_str(), stem.c_str(),
		 stringlistdisp(result).c_str()));

    } catch (...) {
	LOGERR(("stemExpand: error accessing stem db. dbdir [%s] lang [%s]\n",
		dbdir.c_str(), lang.c_str()));
	result.push_back(term);
	return false;
    }

    return true;
}
    
/**
 * Expand term to list of all terms which stem to the same term, add the
 * expansion sets for possibly multiple expansion languages
 */
bool stemExpand(const std::string& dbdir, 
		const std::string& langs,
		const std::string& term,
		list<string>& result)
{

    list<string> llangs;
    stringToStrings(langs, llangs);
    for (list<string>::const_iterator it = llangs.begin();
	 it != llangs.end(); it++) {
	list<string> oneexp;
	stemExpandOne(dbdir, *it, term, oneexp);
	result.insert(result.end(), oneexp.begin(), oneexp.end());
    }
    result.sort();
    result.unique();
    return true;
}


}
}
