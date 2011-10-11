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
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include <algorithm>

#include "debuglog.h"
#include "indexer.h"
#include "fsindexer.h"
#include "beaglequeue.h"
#include "mimehandler.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

ConfIndexer::ConfIndexer(RclConfig *cnf, DbIxStatusUpdater *updfunc)
    : m_config(cnf), m_db(cnf), m_fsindexer(0), 
      m_dobeagle(false), m_beagler(0),
      m_updater(updfunc)
{
    m_config->getConfParam("processbeaglequeue", &m_dobeagle);
}

ConfIndexer::~ConfIndexer()
{
     deleteZ(m_fsindexer);
     deleteZ(m_beagler);
}

bool ConfIndexer::index(bool resetbefore, ixType typestorun)
{
    Rcl::Db::OpenMode mode = resetbefore ? Rcl::Db::DbTrunc : Rcl::Db::DbUpd;
    if (!m_db.open(mode)) {
	LOGERR(("ConfIndexer: error opening database %s : %s\n", 
                m_config->getDbDir().c_str(), m_db.getReason().c_str()));
	return false;
    }

    m_config->setKeyDir("");
    if (typestorun & IxTFs) {
        deleteZ(m_fsindexer);
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
        if (!m_fsindexer || !m_fsindexer->index()) {
            return false;
        }
    }

    if (m_dobeagle && (typestorun & IxTBeagleQueue)) {
        deleteZ(m_beagler);
        m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (!m_beagler || !m_beagler->index()) {
            return false;
        }
    }

    if (typestorun == IxTAll) {
        // Get rid of all database entries that don't exist in the
        // filesystem anymore. Only if all *configured* indexers ran.
        if (m_updater && !m_updater->update(DbIxStatus::DBIXS_PURGE, string()))
	    return false;
        m_db.purge();
    }

    // The close would be done in our destructor, but we want status
    // here. Makes no sense to check for cancel, we'll have to close
    // anyway
    if (m_updater)
	m_updater->update(DbIxStatus::DBIXS_CLOSING, string());
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::index: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }

    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;
    createStemmingDatabases();
    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_CLOSING, string()))
	return false;
    createAspellDict();
    clearMimeHandlerCache();
    return true;
}

bool ConfIndexer::indexFiles(std::list<string>& ifiles, IxFlag flag)
{
    list<string> myfiles;
    for (list<string>::const_iterator it = ifiles.begin(); 
	 it != ifiles.end(); it++) {
	myfiles.push_back(path_canon(*it));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR(("ConfIndexer: indexFiles error opening database %s\n", 
                m_config->getDbDir().c_str()));
	return false;
    }
    m_config->setKeyDir("");
    bool ret = false;
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->indexFiles(myfiles, flag);
    LOGDEB2(("ConfIndexer::indexFiles: fsindexer returned %d, "
            "%d files remainining\n", ret, myfiles.size()));

    if (m_dobeagle && !myfiles.empty()) {
        if (!m_beagler)
            m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (m_beagler) {
            ret = ret && m_beagler->indexFiles(myfiles);
        } else {
            ret = false;
        }
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::index: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }
    ifiles = myfiles;
    clearMimeHandlerCache();
    return ret;
}

bool ConfIndexer::purgeFiles(std::list<string> &files)
{
    list<string> myfiles;
    for (list<string>::const_iterator it = files.begin(); 
	 it != files.end(); it++) {
	myfiles.push_back(path_canon(*it));
    }
    myfiles.sort();

    if (!m_db.open(Rcl::Db::DbUpd)) {
	LOGERR(("ConfIndexer: purgeFiles error opening database %s\n", 
                m_config->getDbDir().c_str()));
	return false;
    }
    bool ret = false;
    m_config->setKeyDir("");
    if (!m_fsindexer)
        m_fsindexer = new FsIndexer(m_config, &m_db, m_updater);
    if (m_fsindexer)
        ret = m_fsindexer->purgeFiles(myfiles);

    if (m_dobeagle && !myfiles.empty()) {
        if (!m_beagler)
            m_beagler = new BeagleQueueIndexer(m_config, &m_db, m_updater);
        if (m_beagler) {
            ret = ret && m_beagler->purgeFiles(myfiles);
        } else {
            ret = false;
        }
    }

    // The close would be done in our destructor, but we want status here
    if (!m_db.close()) {
	LOGERR(("ConfIndexer::purgefiles: error closing database in %s\n", 
		m_config->getDbDir().c_str()));
	return false;
    }
    return ret;
}

// Create stemming databases. We also remove those which are not
// configured. 
bool ConfIndexer::createStemmingDatabases()
{
    string slangs;
    if (m_config->getConfParam("indexstemminglanguages", slangs)) {
        if (!m_db.open(Rcl::Db::DbRO)) {
            LOGERR(("ConfIndexer::createStemmingDb: could not open db\n"))
            return false;
        }
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
	    if (m_updater && !m_updater->update(DbIxStatus::DBIXS_STEMDB, *it))
		return false;
	    m_db.createStemDb(*it);
	}
    }
    m_db.close();
    return true;
}

bool ConfIndexer::createStemDb(const string &lang)
{
    if (!m_db.open(Rcl::Db::DbRO)) {
	return false;
    }
    return m_db.createStemDb(lang);
}

// The language for the aspell dictionary is handled internally by the aspell
// module, either from a configuration variable or the NLS environment.
bool ConfIndexer::createAspellDict()
{
    LOGDEB2(("ConfIndexer::createAspellDict()\n"));
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

    if (!m_db.open(Rcl::Db::DbRO)) {
        LOGERR(("ConfIndexer::createAspellDict: could not open db\n"));
	return false;
    }

    Aspell aspell(m_config);
    string reason;
    if (!aspell.init(reason)) {
	LOGERR(("ConfIndexer::createAspellDict: aspell init failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
    LOGDEB(("ConfIndexer::createAspellDict: creating dictionary\n"));
    if (!aspell.buildDict(m_db, reason)) {
	LOGERR(("ConfIndexer::createAspellDict: aspell buildDict failed: %s\n", 
		reason.c_str()));
	noaspell = true;
	return false;
    }
#endif
    return true;
}

list<string> ConfIndexer::getStemmerNames()
{
    return Rcl::Db::getStemmerNames();
}
