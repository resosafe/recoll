/* Copyright (C) 2009-2019 J.F.Dockes
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef _fsindexer_h_included_
#define _fsindexer_h_included_

#include <list>
#include <mutex>

#include "indexer.h"
#include "fstreewalk.h"
#ifdef IDX_THREADS
#include "workqueue.h"
#endif // IDX_THREADS

class FIMissingStore;

class DbUpdTask;
class InternfileTask;

namespace Rcl {
class Doc;
}

/** Index selected parts of the file system
 
    Tree indexing: we inherits FsTreeWalkerCB so that, the processone()
    method is called by the file-system tree walk code for each file and
    directory. We keep all state needed while indexing, and finally call
    the methods to purge the db of stale entries and create the stemming
    databases.

    Single file(s) indexing: there are also calls to index or purge lists of files.
    No database purging or stem db updating in this case.
*/
class FsIndexer : public FsTreeWalkerCB {
public:
    /** Constructor does nothing but store parameters 
     *
     * @param cnf Configuration data
     */
    FsIndexer(RclConfig *cnf, Rcl::Db *db);
    virtual ~FsIndexer();
    FsIndexer(const FsIndexer&) = delete;
    FsIndexer& operator=(const FsIndexer&) = delete;

    /** 
     * Top level file system tree index method for updating a given database.
     *
     * We open the database,
     * then call a file system walk for each top-level directory.
     */
    bool index(int flags);

    /** Index a list of files. No db cleaning or stemdb updating */
    bool indexFiles(std::list<std::string> &files, 
                    int f = ConfIndexer::IxFNone);

    /** Purge a list of files. */
    bool purgeFiles(std::list<std::string> &files);

    /**  Tree walker callback method */
    FsTreeWalker::Status 
    processone(const string &fn, const struct PathStat *, FsTreeWalker::CbFlag);

private:

    class PurgeCandidateRecorder {
    public:
        PurgeCandidateRecorder() 
            : dorecord(false) {}
        void setRecord(bool onoff) {
            dorecord = onoff;
        }
        void record(const string& udi) {
            // This test does not need to be protected: the value is set at
            // init and never changed.
            if (!dorecord)
                return;
#ifdef IDX_THREADS
            std::unique_lock<std::mutex> locker(mutex);
#endif
            udis.push_back(udi);
        }
        const vector<string>& getCandidates() {
            return udis;
        }
    private:
#ifdef IDX_THREADS
        std::mutex mutex;
#endif
        bool dorecord;
        std::vector<std::string> udis;
    };

    bool launchAddOrUpdate(const std::string& udi,
                           const std::string& parent_udi, Rcl::Doc& doc);

    FsTreeWalker m_walker;
    RclConfig   *m_config;
    Rcl::Db     *m_db;
    string       m_reason;
    // Top/start directories list
    std::vector<std::string> m_tdl;
    // Store for missing filters and associated mime types
    FIMissingStore *m_missing;
    // Recorder for files that may need subdoc purging.
    PurgeCandidateRecorder m_purgeCandidates;

    // The configuration can set attribute fields to be inherited by
    // all files in a file system area. Ie: set "rclaptg = thunderbird"
    // inside ~/.thunderbird. The boolean is set at init to avoid
    // further wasteful processing if no local fields are set.
    // This should probably moved to internfile so that the
    // localfields get exactly the same processing as those generated by the
    // filters (as was done for metadatacmds fields)
    bool         m_havelocalfields;
    string       m_slocalfields;
    map<string, string>  m_localfields;

    // Activate detection of xattr-only document updates. Experimental, so
    // needs a config option
    bool         m_detectxattronly;

    // No retry of previously failed files
    bool         m_noretryfailed;
    // use FADV_DONTNEED if available
    bool         m_cleancache{false};
    
#ifdef IDX_THREADS
    friend void *FsIndexerDbUpdWorker(void*);
    friend void *FsIndexerInternfileWorker(void*);
    WorkQueue<InternfileTask*> m_iwqueue;
    WorkQueue<DbUpdTask*> m_dwqueue;
    bool m_haveInternQ;
    bool m_haveSplitQ;
    RclConfig   *m_stableconfig;
#endif // IDX_THREADS

    bool init();
    void localfieldsfromconf();
    void setlocalfields(const map<string, string>& flds, Rcl::Doc& doc);
    string getDbDir() {return m_config->getDbDir();}
    FsTreeWalker::Status 
    processonefile(RclConfig *config, const string &fn, 
                   const struct PathStat *,
                   const map<string,string>& localfields);
    void shutdownQueues(bool);
};

#endif /* _fsindexer_h_included_ */
