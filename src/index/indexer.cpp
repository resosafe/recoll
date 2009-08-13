#ifndef lint
static char rcsid[] = "@(#$Id: indexer.cpp,v 1.71 2008-12-17 08:01:40 dockes Exp $ (C) 2004 J.F.Dockes";
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
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cstring>
#include <fnmatch.h>

#include <iostream>
#include <list>
#include <map>
#include <algorithm>

#include "pathut.h"
#include "conftree.h"
#include "rclconfig.h"
#include "fstreewalk.h"
#include "rcldb.h"
#include "readfile.h"
#include "indexer.h"
#include "csguess.h"
#include "transcode.h"
#include "debuglog.h"
#include "internfile.h"
#include "smallut.h"
#include "wipedir.h"
#include "fileudi.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

// When using extended attributes, we have to use the ctime. 
// This is quite an expensive price to pay...
#ifdef RCL_USE_XATTR
#define RCL_STTIME st_ctime
#else
#define RCL_STTIME st_mtime
#endif // RCL_USE_XATTR

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#ifndef deleteZ
#define deleteZ(X) {delete X;X = 0;}
#endif

DbIndexer::~DbIndexer() {
    // Maybe clean up temporary directory
    if (m_tmpdir.length()) {
	wipedir(m_tmpdir);
	if (rmdir(m_tmpdir.c_str()) < 0) {
	    LOGERR(("DbIndexer::~DbIndexer: cannot clear temp dir %s\n",
		    m_tmpdir.c_str()));
	}
    }
    m_db.close();
}

list<string> DbIndexer::getStemmerNames()
{
    return Rcl::Db::getStemmerNames();
}

// Index each directory in the topdirs for a given db
bool DbIndexer::indexDb(bool resetbefore, list<string> *topdirs)
{
    if (!init(resetbefore))
	return false;

    if (m_updater) {
	m_updater->status.reset();
	m_updater->status.dbtotdocs = m_db.docCnt();
    }

    m_walker.setSkippedPaths(m_config->getSkippedPaths());

    for (list<string>::const_iterator it = topdirs->begin();
	 it != topdirs->end(); it++) {
	LOGDEB(("DbIndexer::index: Indexing %s into %s\n", it->c_str(), 
		getDbDir().c_str()));

	// Set the current directory in config so that subsequent
	// getConfParams() will get local values
	m_config->setKeyDir(*it);

	// Adjust the "follow symlinks" option
	bool follow;
	if (m_config->getConfParam("followLinks", &follow) && follow) {
	    m_walker.setOpts(FsTreeWalker::FtwFollow);
	} else {
	    m_walker.setOpts(FsTreeWalker::FtwOptNone);
	}	    

	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db.setAbstractParams(abslen, -1, -1);

	// Set up skipped patterns for this subtree. This probably should be
	// done in the directory change code in processone() instead.
	m_walker.setSkippedNames(m_config->getSkippedNames());

	// Walk the directory tree
	if (m_walker.walk(*it, *this) != FsTreeWalker::FtwOk) {
	    LOGERR(("DbIndexer::index: error while indexing %s: %s\n", 
		    it->c_str(), m_walker.getReason().c_str()));
	    return false;
	}
    }
    if (m_updater) {
	m_updater->status.fn.erase();
	m_updater->status.phase = DbIxStatus::DBIXS_PURGE;
	m_updater->update();
    }

    // Get rid of all database entries that don't exist in the
    // filesystem anymore.
    m_db.purge();

    createStemmingDatabases();
    createAspellDict();

    // The close would be done in our destructor, but we want status here
    if (m_updater) {
	m_updater->status.phase = DbIxStatus::DBIXS_CLOSING;
	m_updater->status.fn.erase();
	m_updater->update();
    }
    if (!m_db.close()) {
	LOGERR(("DbIndexer::index: error closing database in %s\n", 
		getDbDir().c_str()));
	return false;
    }
    string missing;
    FileInterner::getMissingDescription(missing);
    if (!missing.empty()) {
	LOGINFO(("DbIndexer::index missing helper program(s):\n%s\n", 
		 missing.c_str()));
    }
    m_config->storeMissingHelperDesc(missing);
    return true;
}

// Create stemming databases. We also remove those which are not
// configured. 
bool DbIndexer::createStemmingDatabases()
{
    string slangs;
    if (m_config->getConfParam("indexstemminglanguages", slangs)) {
	list<string> langs;
	stringToStrings(slangs, langs);

	// Get the list of existing stem dbs from the database (some may have 
	// been manually created, we just keep those from the config
	list<string> dblangs = m_db.getStemLangs();
	list<string>::const_iterator it;
	for (it = dblangs.begin(); it != dblangs.end(); it++) {
	    if (find(langs.begin(), langs.end(), *it) == langs.end())
		m_db.deleteStemDb(*it);
	}
	for (it = langs.begin(); it != langs.end(); it++) {
	    if (m_updater) {
		m_updater->status.phase = DbIxStatus::DBIXS_STEMDB;
		m_updater->status.fn = *it;
		m_updater->update();
	    }
	    m_db.createStemDb(*it);
	}
    }
    return true;
}

bool DbIndexer::init(bool resetbefore, bool rdonly)
{
    if (m_tmpdir.empty() || access(m_tmpdir.c_str(), 0) < 0) {
	string reason;
	if (!maketmpdir(m_tmpdir, reason)) {
	    LOGERR(("DbIndexer: cannot create temporary directory: %s\n",
		    reason.c_str()));
	    return false;
	}
    }
    Rcl::Db::OpenMode mode = rdonly ? Rcl::Db::DbRO :
	resetbefore ? Rcl::Db::DbTrunc : Rcl::Db::DbUpd;
    if (!m_db.open(mode)) {
	LOGERR(("DbIndexer: error opening database %s\n", getDbDir().c_str()));
	return false;
    }

    return true;
}

bool DbIndexer::createStemDb(const string &lang)
{
    if (!init(false, true))
	return false;
    return m_db.createStemDb(lang);
}

// The language for the aspell dictionary is handled internally by the aspell
// module, either from a configuration variable or the NLS environment.
bool DbIndexer::createAspellDict()
{
    LOGDEB2(("DbIndexer::createAspellDict()\n"));
#ifdef RCL_USE_ASPELL
    // For the benefit of the real-time indexer, we only initialize
    // noaspell from the configuration once. It can then be set to
    // true if dictionary generation fails, which avoids retrying
    // it forever.
    static int noaspell = -12345;
    if (noaspell == -12345) {
	noaspell = false;
	m_config->getConfParam("noaspell", &noaspell);
    }
    if (noaspell)
	return true;

    if (!init(false, true))
	return false;
    Aspell aspell(m_config);
    string reason;
    if (!aspell.init(reason)) {
	LOGERR(("DbIndexer::createAspellDict: aspell init failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
    LOGDEB(("DbIndexer::createAspellDict: creating dictionary\n"));
    if (!aspell.buildDict(m_db, reason)) {
	LOGERR(("DbIndexer::createAspellDict: aspell buildDict failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("DbIndexer::indexfiles: error closing database in %s\n", 
		getDbDir().c_str()));
	noaspell = true;
	return false;
    }
#endif
    return true;
}

/** 
 * Index individual files, out of a full tree run. No database purging
 */
bool DbIndexer::indexFiles(const list<string> &filenames)
{
    bool called_init = false;

    list<string>::const_iterator it;
    for (it = filenames.begin(); it != filenames.end(); it++) {
	string dir = path_getfather(*it);
	m_config->setKeyDir(dir);
	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db.setAbstractParams(abslen, -1, -1);
	struct stat stb;
	if (lstat(it->c_str(), &stb) != 0) {
	    LOGERR(("DbIndexer::indexFiles: lstat(%s): %s", it->c_str(),
		    strerror(errno)));
	    continue;
	}

	// If we get to indexing directory names one day, will need to test 
	// against dbdir here to avoid modification loops (with rclmon).
	if (!S_ISREG(stb.st_mode)) {
	    LOGDEB2(("DbIndexer::indexFiles: %s: not a regular file\n", 
		    it->c_str()));
	    continue;
	}

	static string lstdir;
	static list<string> skpl;
	if (lstdir.compare(dir)) {
	    LOGDEB(("Recomputing list of skipped names\n"));
	    skpl = m_config->getSkippedNames();
	    lstdir = dir;
	}
	if (!skpl.empty()) {
	    list<string>::const_iterator skit;
	    string fn = path_getsimple(*it);
	    for (skit = skpl.begin(); skit != skpl.end(); skit++) {
		if (fnmatch(skit->c_str(), fn.c_str(), 0) == 0) {
		    LOGDEB(("Skipping [%s] :matches skip list\n", fn.c_str()));
		    goto skipped;
		}
	    }
	}
	// Defer opening db until really needed.
	if (!called_init) {
	    if (!init())
		return false;
	    called_init = true;
	}
	if (processone(*it, &stb, FsTreeWalker::FtwRegular) != 
	    FsTreeWalker::FtwOk) {
	    LOGERR(("DbIndexer::indexFiles: processone failed\n"));
	    return false;
	}
    skipped: 
	false; // Need a statement here to make compiler happy ??
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("DbIndexer::indexfiles: error closing database in %s\n", 
		getDbDir().c_str()));
	return false;
    }
    return true;
}


/** Purge docs for given files out of the database */
bool DbIndexer::purgeFiles(const list<string> &filenames)
{
    if (!init())
	return false;

    list<string>::const_iterator it;
    for (it = filenames.begin(); it != filenames.end(); it++) {
	string udi;
	make_udi(*it, "", udi);
	if (!m_db.purgeFile(udi)) {
	    LOGERR(("DbIndexer::purgeFiles: Database error\n"));
	    return false;
	}
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("DbIndexer::purgefiles: error closing database in %s\n", 
		getDbDir().c_str()));
	return false;
    }
    return true;
}


/// This method gets called for every file and directory found by the
/// tree walker. 
///
/// It checks with the db if the file has changed and needs to be
/// reindexed. If so, it calls internfile() which will identify the
/// file type and call an appropriate handler to convert the document into
/// internal format, which we then add to the database.
///
/// Accent and majuscule handling are performed by the db module when doing
/// the actual indexing work. The Rcl::Doc created by internfile()
/// mostly contains pretty raw utf8 data.
FsTreeWalker::Status 
DbIndexer::processone(const std::string &fn, const struct stat *stp, 
		      FsTreeWalker::CbFlag flg)
{
    if (m_updater && !m_updater->update()) {
	    return FsTreeWalker::FtwStop;
    }
    // If we're changing directories, possibly adjust parameters (set
    // the current directory in configuration object)
    if (flg == FsTreeWalker::FtwDirEnter || 
	flg == FsTreeWalker::FtwDirReturn) {
	m_config->setKeyDir(fn);
	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db.setAbstractParams(abslen, -1, -1);
	if (flg == FsTreeWalker::FtwDirReturn)
	    return FsTreeWalker::FtwOk;
    }

    // Check db up to date ? Doing this before file type
    // identification means that, if usesystemfilecommand is switched
    // from on to off it may happen that some files which are now
    // without mime type will not be purged from the db, resulting
    // in possible 'cannot intern file' messages at query time...
    char cbuf[100]; 
    // Document signature. This is based on m/ctime and size and used
    // for the uptodate check (the value computed here is checked
    // against the stored one). Changing the computation forces a full
    // reindex of course.
    sprintf(cbuf, "%ld%ld", (long)stp->st_size, (long)stp->RCL_STTIME);
    string sig = cbuf;
    string udi;
    make_udi(fn, "", udi);
    if (!m_db.needUpdate(udi, sig)) {
	LOGDEB(("processone: up to date: %s\n", fn.c_str()));
	if (m_updater) {
	    // Status bar update, abort request etc.
	    m_updater->status.fn = fn;
	    if (!m_updater->update()) {
		return FsTreeWalker::FtwStop;
	    }
	}
	return FsTreeWalker::FtwOk;
    }

    FileInterner interner(fn, stp, m_config, m_tmpdir);

    // File name transcoded to utf8 for indexation. 
    string charset = m_config->getDefCharset(true);
    // If this fails, the file name won't be indexed, no big deal
    // Note that we used to do the full path here, but I ended up believing
    // that it made more sense to use only the file name
    string utf8fn; int ercnt;
    if (!transcode(path_getsimple(fn), utf8fn, charset, "UTF-8", &ercnt)) {
	LOGERR(("processone: fn transcode failure from [%s] to UTF-8: %s\n",
		charset.c_str(), path_getsimple(fn).c_str()));
    } else if (ercnt) {
	LOGDEB(("processone: fn transcode %d errors from [%s] to UTF-8: %s\n",
		ercnt, charset.c_str(), path_getsimple(fn).c_str()));
    }
    LOGDEB2(("processone: fn transcoded from [%s] to [%s] (%s->%s)\n",
	     path_getsimple(fn).c_str(), utf8fn.c_str(), charset.c_str(), 
	     "UTF-8"));

    string parent_udi;
    make_udi(fn, "", parent_udi);
    Rcl::Doc doc;
    const string plus("+");
    char ascdate[20];
    sprintf(ascdate, "%ld", long(stp->st_mtime));

    FileInterner::Status fis = FileInterner::FIAgain;
    bool hadNullIpath = false;
    while (fis == FileInterner::FIAgain) {
	doc.erase();
	string ipath;
	fis = interner.internfile(doc, ipath);
	if (fis == FileInterner::FIError) {
	    // We used to return at this point. 
	    //
	    // The nice side was that if a filter failed because of a
	    // lacking supporting app, the file would be indexed once
	    // the app was installed.
	    //
	    // The not so nice point was that the file name was not
	    // indexed.
	    //
	    // We now index at least the file name and the mod time. 
	    // We change the signature to ensure that the indexing will 
	    // be retried every time. This can make indexing passes quite
	    // slower if there are many files of types with no helper
	    doc.fmtime.erase();
	    // Go through:
	} 

	if (doc.fmtime.empty()) {
	    // Set the date if this was not done in the document handler
	    doc.fmtime = ascdate;
	}

	// Internal access path for multi-document files
	if (ipath.empty())
	    hadNullIpath = true;
	else
	    doc.ipath = ipath;

	doc.url = string("file://") + fn;

	// Note that the filter may have its own idea of the file name 
	// (ie: mail attachment)
	if (doc.utf8fn.empty())
	    doc.utf8fn = utf8fn;

	char cbuf[100]; 
	sprintf(cbuf, "%ld", (long)stp->st_size);
	doc.fbytes = cbuf;
	// Document signature for up to date checks: concatenate mtime and 
	// size. Note: looking for changes only, no need to parseback so no
	// need for reversible formatting
	sprintf(cbuf, "%ld%ld", (long)stp->st_size, (long)stp->RCL_STTIME);
	doc.sig = cbuf;
	// If there was an error, ensure indexing will be
	// retried. This is for the once missing, later installed
	// filter case. It can make indexing much slower (if there are
	// myriads of such files, the ext script is executed for them
	// and fails every time)
	if (fis == FileInterner::FIError) {
	    doc.sig += plus;
	}

	// Add document to database. If there is an ipath, add it as a children
	// of the file document.
	string udi;
	make_udi(fn, ipath, udi);
	if (!m_db.addOrUpdate(udi, ipath.empty() ? "" : parent_udi, doc)) 
	    return FsTreeWalker::FtwError;

	// Tell what we are doing and check for interrupt request
	if (m_updater) {
	    if ((++(m_updater->status.docsdone) % 10) == 0) {
		m_updater->status.fn = fn;
		if (!ipath.empty())
		    m_updater->status.fn += "|" + ipath;
		if (!m_updater->update()) {
		    return FsTreeWalker::FtwStop;
		}
	    }
	}
    }

    // If we had no instance with a null ipath, we create an empty
    // document to stand for the file itself, to be used mainly for up
    // to date checks. Typically this happens for an mbox file.
    if (hadNullIpath == false) {
	LOGDEB1(("Creating empty doc for file\n"));
	Rcl::Doc fileDoc;
	fileDoc.fmtime = ascdate;
	fileDoc.utf8fn = utf8fn;
	fileDoc.mimetype = interner.getMimetype();
	fileDoc.url = string("file://") + fn;

	char cbuf[100]; 
	sprintf(cbuf, "%ld", (long)stp->st_size);
	fileDoc.fbytes = cbuf;
	// Document signature for up to date checks.
	sprintf(cbuf, "%ld%ld", (long)stp->st_size, (long)stp->RCL_STTIME);
	fileDoc.sig = cbuf;
	if (!m_db.addOrUpdate(parent_udi, "", fileDoc)) 
	    return FsTreeWalker::FtwError;
    }

    return FsTreeWalker::FtwOk;
}

////////////////////////////////////////////////////////////////////////////
// ConIndexer methods: ConfIndexer is the top-level object, that can index
// multiple directories to multiple databases.

ConfIndexer::~ConfIndexer()
{
     deleteZ(m_dbindexer);
}

bool ConfIndexer::index(bool resetbefore)
{
    list<string> tdl = m_config->getTopdirs();
    if (tdl.empty()) {
	m_reason = "Top directory list (topdirs param.) not found in config"
	    "or Directory list parse error";
	return false;
    }
    
    // Each top level directory to be indexed can be associated with a
    // different database. We first group the directories by database:
    // it is important that all directories for a database be indexed
    // at once so that deleted file cleanup works
    list<string>::iterator dirit;
    map<string, list<string> > dbmap;
    map<string, list<string> >::iterator dbit;
    for (dirit = tdl.begin(); dirit != tdl.end(); dirit++) {
	string dbdir;
	string doctopdir = *dirit;
	m_config->setKeyDir(doctopdir);
	dbdir = m_config->getDbDir();
	if (dbdir.empty()) {
	    LOGERR(("ConfIndexer::index: no database directory in "
		    "configuration for %s\n", doctopdir.c_str()));
	    m_reason = "No database directory set for " + doctopdir;
	    return false;
	}
	dbit = dbmap.find(dbdir);
	if (dbit == dbmap.end()) {
	    list<string> l;
	    l.push_back(doctopdir);
	    dbmap[dbdir] = l;
	} else {
	    dbit->second.push_back(doctopdir);
	}
    }
    m_config->setKeyDir("");

    // The dbmap now has dbdir as key and directory lists as values.
    // Index each directory group in turn
    for (dbit = dbmap.begin(); dbit != dbmap.end(); dbit++) {
	//cout << dbit->first << " -> ";
	//list<string>::const_iterator dit;
	//for (dit = dbit->second.begin(); dit != dbit->second.end(); dit++) {
	//    cout << *dit << " ";
	//}
	//cout << endl;
	m_dbindexer = new DbIndexer(m_config, dbit->first, m_updater);
	if (!m_dbindexer->indexDb(resetbefore, &dbit->second)) {
	    deleteZ(m_dbindexer);
	    m_reason = "Failed indexing in " + dbit->first;
	    return false;
	}
	deleteZ(m_dbindexer);
    }
    return true;
}
