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
#ifndef _INDEXER_H_INCLUDED_
#define _INDEXER_H_INCLUDED_
/* @(#$Id: indexer.h,v 1.21.2.1 2006-11-27 10:00:53 dockes Exp $  (C) 2004 J.F.Dockes */

#include <string>
#include <list>

#ifndef NO_NAMESPACES
using std::string;
using std::list;
#endif

#include "rclconfig.h"
#include "fstreewalk.h"
#include "rcldb.h"

/* Forward decl for lower level indexing object */
class DbIndexer;

class DbIxStatus {
 public:
    enum Phase {DBIXS_FILES, DBIXS_PURGE, DBIXS_STEMDB, DBIXS_CLOSING};
    Phase phase;
    string fn;   // Last file processed
    int docsdone;  // Documents processed
    int dbtotdocs;  // Doc count in index at start
    void reset() {phase = DBIXS_FILES;fn.erase();docsdone=dbtotdocs=0;}
    DbIxStatus() {reset();}
};

/** Callback to say what we're doing. If the update func returns false, we
 * stop as soon as possible without corrupting state */
class DbIxStatusUpdater {
 public:
    DbIxStatus status;
    virtual ~DbIxStatusUpdater(){}
    virtual bool update() = 0;
};

/**
   The top level indexing object. Processes the configuration, then invokes
   file system walking to populate/update the database(s).
 
   Multiple top-level directories can be listed in the
   configuration. Each can be indexed to a different
   database. Directories are first grouped by database, then an
   internal class (DbIndexer) is used to process each group.
*/
class ConfIndexer {
 public:
    enum runStatus {IndexerOk, IndexerError};
    ConfIndexer(RclConfig *cnf, DbIxStatusUpdater *updfunc = 0)
	: m_config(cnf), m_dbindexer(0), m_updater(updfunc)
	{}
    virtual ~ConfIndexer();
    /** Worker function: doe the actual indexing */
    bool index(bool resetbefore = false);
    const string &getReason() {return m_reason;}
 private:
    RclConfig *m_config;
    DbIndexer *m_dbindexer; // Object to process directories for a given db
    DbIxStatusUpdater *m_updater;
    string m_reason;
};

/** Index things into one database
 
Tree indexing: we inherits FsTreeWalkerCB so that, the processone()
  method is called by the file-system tree walk code for each file and
  directory. We keep all state needed while indexing, and finally call
  the methods to purge the db of stale entries and create the stemming
  databases.

Single file(s) indexing: no database purging or stem db updating.
*/
class DbIndexer : public FsTreeWalkerCB {
 public:
    /** Constructor does nothing but store parameters */
    DbIndexer(RclConfig *cnf,         // Configuration data
	      const string &dbd, // Place where the db lives
	      DbIxStatusUpdater *updfunc = 0 // status updater callback
	      ) 
	: m_config(cnf), m_dbdir(dbd), m_updater(updfunc) { 
    }
	
    virtual ~DbIndexer();

    /** Top level file system tree index method for updating a
	given database.

	The list is supposed to have all the filename space for the
	db, and we shall purge entries for non-existing files at the
	end. We create the temporary directory, open the database,
	then call a file system walk for each top-level directory.
	When walking is done, we create the stem databases and close
	the main db.
    */
    bool indexDb(bool resetbefore, std::list<string> *topdirs);

    /** Index a list of files. No db cleaning or stemdb updating */
    bool indexFiles(const std::list<string> &files);

    /** Purge a list of files. */
    bool purgeFiles(const std::list<string> &files);

    /** Stemming reset to config: create needed, delete unconfigured */
    bool createStemmingDatabases();

    /** Create stem database for given language */
    bool createStemDb(const string &lang);

    /** Create misspelling expansion dictionary if aspell i/f is available */
    bool createAspellDict();

    /**  Tree walker callback method */
    FsTreeWalker::Status 
	processone(const string &, const struct stat *, 
		   FsTreeWalker::CbFlag);

    /** Return my db dir */
    string getDbDir() {return m_dbdir;}

 private:
    FsTreeWalker m_walker;
    RclConfig   *m_config;
    string       m_dbdir;
    Rcl::Db      m_db;
    string       m_tmpdir;
    DbIxStatusUpdater *m_updater;

    bool init(bool rst = false, bool rdonly = false);
};

/** Helper methods in recollindex.cpp for initial checks/setup to index 
 * a list of files (either from the monitor or the command line) */
extern bool indexfiles(RclConfig *config, const list<string> &filenames);
extern bool purgefiles(RclConfig *config, const list<string> &filenames);
extern bool createAuxDbs(RclConfig *config);

#endif /* _INDEXER_H_INCLUDED_ */
