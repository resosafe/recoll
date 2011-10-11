/* Copyright (C) 2009 J.F.Dockes
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
#include "fsindexer.h"
#include "csguess.h"
#include "transcode.h"
#include "debuglog.h"
#include "internfile.h"
#include "smallut.h"
#include "wipedir.h"
#include "fileudi.h"
#include "cancelcheck.h"

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

FsIndexer::FsIndexer(RclConfig *cnf, Rcl::Db *db, DbIxStatusUpdater *updfunc) 
    : m_config(cnf), m_db(db), m_updater(updfunc), m_missing(new FIMissingStore)
{
    m_havelocalfields = m_config->hasNameAnywhere("localfields");
}
FsIndexer::~FsIndexer() {
    delete m_missing;
}

bool FsIndexer::init()
{
    if (m_tdl.empty()) {
        m_tdl = m_config->getTopdirs();
        if (m_tdl.empty()) {
            LOGERR(("FsIndexers: no topdirs list defined\n"));
            return false;
        }
    }
    return true;
}

// Recursively index each directory in the topdirs:
bool FsIndexer::index()
{
    if (!init())
	return false;

    if (m_updater) {
	m_updater->status.reset();
	m_updater->status.dbtotdocs = m_db->docCnt();
    }

    m_walker.setSkippedPaths(m_config->getSkippedPaths());
    m_walker.addSkippedPath(path_tildexpand("~/.beagle"));
    for (list<string>::const_iterator it = m_tdl.begin();
	 it != m_tdl.end(); it++) {
	LOGDEB(("FsIndexer::index: Indexing %s into %s\n", it->c_str(), 
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
	    m_db->setAbstractParams(abslen, -1, -1);

	// Walk the directory tree
	if (m_walker.walk(*it, *this) != FsTreeWalker::FtwOk) {
	    LOGERR(("FsIndexer::index: error while indexing %s: %s\n", 
		    it->c_str(), m_walker.getReason().c_str()));
	    return false;
	}
    }

    string missing;
    FileInterner::getMissingDescription(m_missing, missing);
    if (!missing.empty()) {
	LOGINFO(("FsIndexer::index missing helper program(s):\n%s\n", 
		 missing.c_str()));
    }
    m_config->storeMissingHelperDesc(missing);
    return true;
}

static bool matchesSkipped(const list<string>& tdl,
                           FsTreeWalker& walker,
                           const string& path)
{
    // First check what (if any) topdir this is in:
    string td;
    for (list<string>::const_iterator it = tdl.begin(); it != tdl.end(); it++) {
        if (path.find(*it) == 0) {
            td = *it;
            break;
        }
    }
    if (td.empty()) {
        LOGDEB(("FsIndexer::indexFiles: skipping [%s] (ntd)\n", path.c_str()));
        return true;
    }

    // Check path against skippedPaths. 
    if (walker.inSkippedPaths(path)) {
        LOGDEB(("FsIndexer::indexFiles: skipping [%s] (skpp)\n", path.c_str()));
        return true;
    }

    // Then check all path components up to the topdir against skippedNames
    string mpath = path;
    while (mpath.length() >= td.length() && mpath.length() > 1) {
        string fn = path_getsimple(mpath);
        if (walker.inSkippedNames(fn)) {
            LOGDEB(("FsIndexer::indexFiles: skipping [%s] (skpn)\n", 
                    path.c_str()));
            return true;
        }

        string::size_type len = mpath.length();
        mpath = path_getfather(mpath);
        // getfather normally returns a path ending with /, getsimple 
        // would then return ''
        if (!mpath.empty() && mpath[mpath.size()-1] == '/')
            mpath.erase(mpath.size()-1);
        // should not be necessary, but lets be prudent. If the
        // path did not shorten, something is seriously amiss
        // (could be an assert actually)
        if (mpath.length() >= len)
            return true;
    }
    return false;
}

/** 
 * Index individual files, out of a full tree run. No database purging
 */
bool FsIndexer::indexFiles(list<string>& files, ConfIndexer::IxFlag flag)
{
    if (!init())
        return false;

    // We use an FsTreeWalker just for handling the skipped path/name lists
    FsTreeWalker walker;
    walker.setSkippedPaths(m_config->getSkippedPaths());
    m_walker.addSkippedPath(path_tildexpand("~/.beagle"));

    for (list<string>::iterator it = files.begin(); it != files.end(); ) {
        LOGDEB2(("FsIndexer::indexFiles: [%s]\n", it->c_str()));

        m_config->setKeyDir(path_getfather(*it));
        if (m_havelocalfields)
            localfieldsfromconf();
        walker.setSkippedNames(m_config->getSkippedNames());

	// Check path against indexed areas and skipped names/paths
        if (!(flag&ConfIndexer::IxFIgnoreSkip) && 
	    matchesSkipped(m_tdl, walker, *it)) {
            it++; continue;
        }

	struct stat stb;
	if (lstat(it->c_str(), &stb) != 0) {
	    LOGERR(("FsIndexer::indexFiles: lstat(%s): %s", it->c_str(),
		    strerror(errno)));
            it++; continue;
	}

	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db->setAbstractParams(abslen, -1, -1);
        
	if (processone(*it, &stb, FsTreeWalker::FtwRegular) != 
	    FsTreeWalker::FtwOk) {
	    LOGERR(("FsIndexer::indexFiles: processone failed\n"));
	    return false;
	}
        it = files.erase(it);
    }

    return true;
}


/** Purge docs for given files out of the database */
bool FsIndexer::purgeFiles(list<string>& files)
{
    if (!init())
	return false;
    for (list<string>::iterator it = files.begin(); it != files.end(); ) {
	string udi;
	make_udi(*it, "", udi);
        // rcldb::purgefile returns true if the udi was either not
        // found or deleted, false only in case of actual error
        bool existed;
	if (!m_db->purgeFile(udi, &existed)) {
	    LOGERR(("FsIndexer::purgeFiles: Database error\n"));
	    return false;
	}
        // If we actually deleted something, take it off the list
        if (existed) {
            it = files.erase(it);
        } else {
            it++;
        }
    }

    return true;
}

// Local fields can be set for fs subtrees in the configuration file 
void FsIndexer::localfieldsfromconf()
{
    LOGDEB0(("FsIndexer::localfieldsfromconf\n"));
    m_localfields.clear();
    m_config->addLocalFields(&m_localfields);
}

// 
void FsIndexer::setlocalfields(Rcl::Doc& doc)
{
    for (map<string, string>::const_iterator it = m_localfields.begin();
         it != m_localfields.end(); it++) {
        // Should local fields override those coming from the document
        // ? I think not, but not too sure
        if (doc.meta.find(it->second) == doc.meta.end()) {
            doc.meta[it->first] = it->second;
        }
    }
}

static void makesig(const struct stat *stp, string& out)
{
    char cbuf[100]; 
    sprintf(cbuf, OFFTPC "%ld", stp->st_size, (long)stp->RCL_STTIME);
    out = cbuf;
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
FsIndexer::processone(const std::string &fn, const struct stat *stp, 
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

	// Set up skipped patterns for this subtree. 
	m_walker.setSkippedNames(m_config->getSkippedNames());

	int abslen;
	if (m_config->getConfParam("idxabsmlen", &abslen))
	    m_db->setAbstractParams(abslen, -1, -1);

        // Adjust local fields from config for this subtree
        if (m_havelocalfields)
            localfieldsfromconf();

	if (flg == FsTreeWalker::FtwDirReturn)
	    return FsTreeWalker::FtwOk;
    }

    ////////////////////
    // Check db up to date ? Doing this before file type
    // identification means that, if usesystemfilecommand is switched
    // from on to off it may happen that some files which are now
    // without mime type will not be purged from the db, resulting
    // in possible 'cannot intern file' messages at query time...

    // Document signature. This is based on m/ctime and size and used
    // for the uptodate check (the value computed here is checked
    // against the stored one). Changing the computation forces a full
    // reindex of course.
    string sig;
    makesig(stp, sig);
    string udi;
    make_udi(fn, "", udi);
    if (!m_db->needUpdate(udi, sig)) {
	LOGDEB0(("processone: up to date: %s\n", fn.c_str()));
	if (m_updater) {
	    // Status bar update, abort request etc.
	    m_updater->status.fn = fn;
	    if (!m_updater->update()) {
		return FsTreeWalker::FtwStop;
	    }
	}
	return FsTreeWalker::FtwOk;
    }

    LOGDEB0(("processone: processing: [%s] %s\n", 
             displayableBytes(stp->st_size).c_str(), fn.c_str()));

    FileInterner interner(fn, stp, m_config, m_tmpdir, FileInterner::FIF_none);
    if (!interner.ok()) {
        // no indexing whatsoever in this case. This typically means that
        // indexallfilenames is not set
        return FsTreeWalker::FtwOk;
    }
    interner.setMissingStore(m_missing);

    // File name transcoded to utf8 for indexing. 
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
    char ascdate[30];
    sprintf(ascdate, "%ld", long(stp->st_mtime));

    FileInterner::Status fis = FileInterner::FIAgain;
    bool hadNullIpath = false;
    while (fis == FileInterner::FIAgain) {
	doc.erase();
        try {
            fis = interner.internfile(doc);
        } catch (CancelExcept) {
            LOGERR(("fsIndexer::processone: interrupted\n"));
            return FsTreeWalker::FtwStop;
        }

        // Index at least the file name even if there was an error.
        // We'll change the signature to ensure that the indexing will
        // be retried every time.

	// Internal access path for multi-document files
	if (doc.ipath.empty())
	    hadNullIpath = true;

	// Set file name, mod time and url if not done by filter
	if (doc.fmtime.empty())
	    doc.fmtime = ascdate;
        if (doc.url.empty())
            doc.url = string("file://") + fn;
	if (doc.utf8fn.empty())
	    doc.utf8fn = utf8fn;

	char cbuf[100]; 
	sprintf(cbuf, OFFTPC, stp->st_size);
	doc.fbytes = cbuf;
	// Document signature for up to date checks: concatenate
	// m/ctime and size. Looking for changes only, no need to
	// parseback so no need for reversible formatting. Also set,
	// but never used, for subdocs.
	makesig(stp, doc.sig);

	// If there was an error, ensure indexing will be
	// retried. This is for the once missing, later installed
	// filter case. It can make indexing much slower (if there are
	// myriads of such files, the ext script is executed for them
	// and fails every time)
	if (fis == FileInterner::FIError) {
	    doc.sig += plus;
	}

        // Possibly add fields from local config
        if (m_havelocalfields) 
            setlocalfields(doc);
	// Add document to database. If there is an ipath, add it as a children
	// of the file document.
	string udi;
	make_udi(fn, doc.ipath, udi);
	if (!m_db->addOrUpdate(udi, doc.ipath.empty() ? "" : parent_udi, doc)) 
	    return FsTreeWalker::FtwError;

	// Tell what we are doing and check for interrupt request
	if (m_updater) {
	    ++(m_updater->status.docsdone);
            if (m_updater->status.dbtotdocs < m_updater->status.docsdone)
                m_updater->status.dbtotdocs = m_updater->status.docsdone;
            m_updater->status.fn = fn;
            if (!doc.ipath.empty())
                m_updater->status.fn += "|" + doc.ipath;
            if (!m_updater->update()) {
                return FsTreeWalker::FtwStop;
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
	sprintf(cbuf, OFFTPC, stp->st_size);
	fileDoc.fbytes = cbuf;
	// Document signature for up to date checks.
	makesig(stp, fileDoc.sig);
	if (!m_db->addOrUpdate(parent_udi, "", fileDoc)) 
	    return FsTreeWalker::FtwError;
    }

    return FsTreeWalker::FtwOk;
}
