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
#include <signal.h>
#include <errno.h>
#include <fnmatch.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>

#include <iostream>
#include <list>
#include <string>
#include <cstdlib>

using namespace std;

#include "debuglog.h"
#include "rclinit.h"
#include "indexer.h"
#include "smallut.h"
#include "pathut.h"
#include "rclmon.h"
#include "x11mon.h"
#include "cancelcheck.h"
#include "rcldb.h"
#include "beaglequeue.h"
#include "recollindex.h"
#include "fsindexer.h"
#include "rclionice.h"
#include "execmd.h"
#include "checkretryfailed.h"

// Command line options
static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_z     0x2 
#define OPT_h     0x4 
#define OPT_i     0x8
#define OPT_s     0x10
#define OPT_c     0x20
#define OPT_S     0x40
#define OPT_m     0x80
#define OPT_D     0x100
#define OPT_e     0x200
#define OPT_w     0x400
#define OPT_x     0x800
#define OPT_l     0x1000
#define OPT_b     0x2000
#define OPT_f     0x4000
#define OPT_C     0x8000
#define OPT_Z     0x10000
#define OPT_n     0x20000
#define OPT_r     0x40000
#define OPT_k     0x80000
#define OPT_E     0x100000
#define OPT_K     0x200000
ReExec *o_reexec;

// Globals for atexit cleanup
static ConfIndexer *confindexer;

// This is set as an atexit routine, 
static void cleanup()
{
    deleteZ(confindexer);
}

// Global stop request flag. This is checked in a number of place in the
// indexing routines.
int stopindexing;

// Receive status updates from the ongoing indexing operation
// Also check for an interrupt request and return the info to caller which
// should subsequently orderly terminate what it is doing.
class MyUpdater : public DbIxStatusUpdater {
 public:
    MyUpdater(const RclConfig *config) 
	: m_prevphase(DbIxStatus::DBIXS_NONE)
    {
	m_fd = open(config->getIdxStatusFile().c_str(), 
		    O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (m_fd < 0)
	    LOGERR(("Can't open/create status file: [%s]\n",
		    config->getIdxStatusFile().c_str()));
    }

    virtual bool update() 
    {
	// Update the status file. Avoid doing it too often
	if (status.phase != m_prevphase || m_chron.millis() > 300) {
	    m_prevphase = status.phase;
	    m_chron.restart();
	    lseek(m_fd, 0, 0);
	    int fd1 = dup(m_fd);
	    FILE *fp = fdopen(fd1, "w");
	    fprintf(fp, "phase = %d\n", int(status.phase));
	    fprintf(fp, "docsdone = %d\n", status.docsdone);
	    fprintf(fp, "filesdone = %d\n", status.filesdone);
	    fprintf(fp, "dbtotdocs = %d\n", status.dbtotdocs);
	    fprintf(fp, "fn = %s\n", status.fn.c_str());
	    if (ftruncate(m_fd, off_t(ftell(fp))) < 0) {
		// ? kill compiler warning about ignoring ftruncate return
		LOGDEB(("Status update: ftruncate failed\n"));
	    }
            // Flush data and closes fd1. m_fd still valid
	    fclose(fp); 
	}

	if (stopindexing) {
	    return false;
	}

	// If we are in the monitor, we also need to check X11 status
	// during the initial indexing pass (else the user could log
	// out and the indexing would go on, not good (ie: if the user
	// logs in again, the new recollindex will fail).
	if ((op_flags & OPT_m) && !(op_flags & OPT_x) && !x11IsAlive()) {
	    LOGDEB(("X11 session went away during initial indexing pass\n"));
	    stopindexing = true;
	    return false;
	}

	return true;
    }

private:
    int    m_fd;
    Chrono m_chron;
    DbIxStatus::Phase m_prevphase;
};
static MyUpdater *updater;

static void sigcleanup(int sig)
{
    fprintf(stderr, "Got signal, registering stop request\n");
    LOGDEB(("Got signal, registering stop request\n"));
    CancelCheck::instance().setCancel();
    stopindexing = 1;
}

static void makeIndexerOrExit(RclConfig *config, bool inPlaceReset)
{
    if (!confindexer) {
	confindexer = new ConfIndexer(config, updater);
	if (inPlaceReset)
	    confindexer->setInPlaceReset();
    }
    if (!confindexer) {
        cerr << "Cannot create indexer" << endl;
        exit(1);
    }
}

void rclIxIonice(const RclConfig *config)
{
    string clss, classdata;
    if (!config->getConfParam("monioniceclass", clss) || clss.empty())
	clss = "3";
    config->getConfParam("monioniceclassdata", classdata);
    rclionice(clss, classdata);
}

class MakeListWalkerCB : public FsTreeWalkerCB {
public:
    MakeListWalkerCB(list<string>& files, const vector<string>& selpats)
	: m_files(files), m_pats(selpats)
    {
    }
    virtual FsTreeWalker::Status 
    processone(const string& fn, const struct stat *, FsTreeWalker::CbFlag flg) 
    {
	if (flg== FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwRegular){
            if (m_pats.empty()) {
                cerr << "Selecting " << fn << endl;
                m_files.push_back(fn);
            } else {
                for (vector<string>::const_iterator it = m_pats.begin();
                     it != m_pats.end(); it++) {
                    if (fnmatch(it->c_str(), fn.c_str(), 0) == 0) {
                        m_files.push_back(fn);
                        break;
                    }
                }
            }
        }
	return FsTreeWalker::FtwOk;
    }
    list<string>& m_files;
    const vector<string>& m_pats;
};

// Build a list of things to index, then call purgefiles and/or
// indexfiles.  This is basically the same as find xxx | recollindex
// -i [-e] without the find (so, simpler but less powerfull)
bool recursive_index(RclConfig *config, const string& top, 
                     const vector<string>& selpats)
{
    list<string> files;
    MakeListWalkerCB cb(files, selpats);
    FsTreeWalker walker;
    walker.walk(top, cb);
    if (op_flags & OPT_e) {
        purgefiles(config, files);
    }
    return indexfiles(config, files);
}

// Index a list of files. We just call the top indexer method, which
// will sort out what belongs to the indexed trees and call the
// appropriate indexers.
//
// This is called either from the command line or from the monitor. In
// this case we're called repeatedly in the same process, and the
// confindexer is only created once by makeIndexerOrExit (but the db closed and
// flushed every time)
bool indexfiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
	return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    // The default is to retry failed files
    int indexerFlags = ConfIndexer::IxFNone; 
    if (op_flags & OPT_K) 
        indexerFlags |= ConfIndexer::IxFNoRetryFailed; 
    if (op_flags & OPT_f)
        indexerFlags |= ConfIndexer::IxFIgnoreSkip;
    return confindexer->indexFiles(filenames, indexerFlags);
}

// Delete a list of files. Same comments about call contexts as indexfiles.
bool purgefiles(RclConfig *config, list<string> &filenames)
{
    if (filenames.empty())
	return true;
    makeIndexerOrExit(config, (op_flags & OPT_Z) != 0);
    return confindexer->purgeFiles(filenames, ConfIndexer::IxFNone);
}

// Create stemming and spelling databases
bool createAuxDbs(RclConfig *config)
{
    makeIndexerOrExit(config, false);

    if (!confindexer->createStemmingDatabases())
	return false;

    if (!confindexer->createAspellDict())
	return false;

    return true;
}

// Create additional stem database 
static bool createstemdb(RclConfig *config, const string &lang)
{
    makeIndexerOrExit(config, false);
    return confindexer->createStemDb(lang);
}

// Check that topdir entries are valid (successfull tilde exp + abs
// path) or fail.
// In addition, topdirs, skippedPaths, daemSkippedPaths entries should
// match existing files or directories. Warn if they don't
static bool checktopdirs(RclConfig *config, vector<string>& nonexist)
{
    vector<string> tdl;
    if (!config->getConfParam("topdirs", &tdl)) {
        cerr << "No 'topdirs' parameter in configuration\n";
        LOGERR(("recollindex:No 'topdirs' parameter in configuration\n"));;
        return false;
    }

    for (vector<string>::iterator it = tdl.begin(); it != tdl.end(); it++) {
	*it = path_tildexpand(*it);
        if (!it->size() || (*it)[0] != '/') {
            if ((*it)[0] == '~') {
                cerr << "Tilde expansion failed: " << *it << endl;
                LOGERR(("recollindex: tilde expansion failed: %s\n",
                        it->c_str()));
            } else {
                cerr << "Not an absolute path: " << *it << endl;
                LOGERR(("recollindex: not an absolute path: %s\n",
                        it->c_str()));
            }
            return false;
        }
        if (access(it->c_str(), 0) < 0) {
            nonexist.push_back(*it);
        }
    }

    // Check skippedPaths too, but only the user part (shallow==true), not
    // the default values (e.g. /media, which might not exist).
    if (config->getConfParam("skippedPaths", &tdl, true)) {
        for (vector<string>::iterator it = tdl.begin(); it != tdl.end(); it++) {
            *it = path_tildexpand(*it);
            if (access(it->c_str(), 0) < 0) {
                nonexist.push_back(*it);
            }
        }
    }

    if (config->getConfParam("daemSkippedPaths", &tdl, true)) {
        for (vector<string>::iterator it = tdl.begin(); it != tdl.end(); it++) {
            *it = path_tildexpand(*it);
            if (access(it->c_str(), 0) < 0) {
                nonexist.push_back(*it);
            }
        }
    }
    return true;
}

static const char *thisprog;

static const char usage [] =
"\n"
"recollindex [-h] \n"
"    Print help\n"
"recollindex [-z|-Z] [-k]\n"
"    Index everything according to configuration file\n"
"    -z : reset database before starting indexing\n"
"    -Z : in place reset: consider all documents as changed. Can also\n"
"         be combined with -i or -r but not -m\n"
"    -k : retry files on which we previously failed\n"
#ifdef RCL_MONITOR
"recollindex -m [-w <secs>] -x [-D] [-C]\n"
"    Perform real time indexing. Don't become a daemon if -D is set.\n"
"    -w sets number of seconds to wait before starting.\n"
"    -C disables monitoring config for changes/reexecuting.\n"
"    -n disables initial incremental indexing (!and purge!).\n"
#ifndef DISABLE_X11MON
"    -x disables exit on end of x11 session\n"
#endif /* DISABLE_X11MON */
#endif /* RCL_MONITOR */
"recollindex -e <filename [filename ...]>\n"
"    Purge data for individual files. No stem database updates\n"
"recollindex -i [-f] [-Z] <filename [filename ...]>\n"
"    Index individual files. No database purge or stem database updates\n"
"    -f : ignore skippedPaths and skippedNames while doing this\n"
"recollindex -r [-K] [-f] [-Z] [-p pattern] <top> \n"
"   Recursive partial reindex. \n"
"     -p : filter file names, multiple instances are allowed, e.g.: \n"
"        -p *.odt -p *.pdf\n"
"     -K : skip previously failed files (they are retried by default)\n"
"recollindex -l\n"
"    List available stemming languages\n"
"recollindex -s <lang>\n"
"    Build stem database for additional language <lang>\n"
"recollindex -E\n"
"    Check configuration file for topdirs and other paths existence\n"
#ifdef FUTURE_IMPROVEMENT
"recollindex -W\n"
"    Process the Web queue\n"
#endif
#ifdef RCL_USE_ASPELL
"recollindex -S\n"
"    Build aspell spelling dictionary.>\n"
#endif
"Common options:\n"
"    -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
;

static void
Usage(FILE *where = stderr)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    fprintf(fp, "Recoll version: %s\n", Rcl::version_string().c_str());
    exit((op_flags & OPT_h)==0);
}

static RclConfig *config;

void lockorexit(Pidfile *pidfile)
{
    pid_t pid;
    if ((pid = pidfile->open()) != 0) {
	cerr << "Can't become exclusive indexer: " << pidfile->getreason() << 
	    ". Return (other pid?): " << pid << endl;
	exit(1);
    }
    if (pidfile->write_pid() != 0) {
	cerr << "Can't become exclusive indexer: " << pidfile->getreason() <<
	    endl;
	exit(1);
    }
}

int main(int argc, char **argv)
{
    string a_config;
    int sleepsecs = 60;
    vector<string> selpatterns;

    // The reexec struct is used by the daemon to shed memory after
    // the initial indexing pass and to restart when the configuration
    // changes
    o_reexec = new ReExec;
    o_reexec->init(argc, argv);

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'b': op_flags |= OPT_b; break;
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
#ifdef RCL_MONITOR
	    case 'C': op_flags |= OPT_C; break;
	    case 'D': op_flags |= OPT_D; break;
#endif
	    case 'E': op_flags |= OPT_E; break;
	    case 'e': op_flags |= OPT_e; break;
	    case 'f': op_flags |= OPT_f; break;
	    case 'h': op_flags |= OPT_h; break;
	    case 'i': op_flags |= OPT_i; break;
	    case 'k': op_flags |= OPT_k; break;
	    case 'K': op_flags |= OPT_K; break;
	    case 'l': op_flags |= OPT_l; break;
	    case 'm': op_flags |= OPT_m; break;
	    case 'n': op_flags |= OPT_n; break;
	    case 'p':	if (argc < 2)  Usage();
		selpatterns.push_back(*(++argv));
		argc--; goto b1;
	    case 'r': op_flags |= OPT_r; break;
	    case 's': op_flags |= OPT_s; break;
#ifdef RCL_USE_ASPELL
	    case 'S': op_flags |= OPT_S; break;
#endif
	    case 'w':	op_flags |= OPT_w; if (argc < 2)  Usage();
		if ((sscanf(*(++argv), "%d", &sleepsecs)) != 1) 
		    Usage(); 
		argc--; goto b1;
	    case 'x': op_flags |= OPT_x; break;
	    case 'Z': op_flags |= OPT_Z; break;
	    case 'z': op_flags |= OPT_z; break;
	    default: Usage(); break;
	    }
    b1: argc--; argv++;
    }
    if (op_flags & OPT_h)
	Usage(stdout);
#ifndef RCL_MONITOR
    if (op_flags & (OPT_m | OPT_w|OPT_x)) {
	cerr << "Sorry, -m not available: real-time monitoring was not "
	    "configured in this build\n";
	exit(1);
    }
#endif

    if ((op_flags & OPT_z) && (op_flags & (OPT_i|OPT_e|OPT_r)))
	Usage();
    if ((op_flags & OPT_Z) && (op_flags & (OPT_m)))
	Usage();
    if ((op_flags & OPT_E) && (op_flags & ~OPT_E)) {
        Usage();
    }
    string reason;
    RclInitFlags flags = (op_flags & OPT_m) && !(op_flags&OPT_D) ? 
	RCLINIT_DAEMON : RCLINIT_NONE;
    config = recollinit(flags, cleanup, sigcleanup, reason, &a_config);
    if (config == 0 || !config->ok()) {
	cerr << "Configuration problem: " << reason << endl;
	exit(1);
    }
    o_reexec->atexit(cleanup);

    vector<string> nonexist;
    if (!checktopdirs(config, nonexist))
        exit(1);

    if (nonexist.size()) {
        ostream& out = (op_flags & OPT_E) ? cout : cerr;
        if (!(op_flags & OPT_E)) {
            cerr << "Warning: invalid paths in topdirs, skippedPaths or "
                "daemSkippedPaths:\n";
        }
        for (vector<string>::const_iterator it = nonexist.begin(); 
             it != nonexist.end(); it++) {
            out << *it << endl;
        }
    }
    if ((op_flags & OPT_E)) {
        exit(0);
    }

    string rundir;
    config->getConfParam("idxrundir", rundir);
    if (!rundir.compare("tmp")) {
	LOGINFO(("recollindex: changing current directory to [%s]\n",
		tmplocation().c_str()));
	if (chdir(tmplocation().c_str()) < 0) {
	    LOGERR(("chdir(%s) failed, errno %d\n", 
		    tmplocation().c_str(), errno));
	}
    } else if (!rundir.empty()) {
	LOGINFO(("recollindex: changing current directory to [%s]\n",
		 rundir.c_str()));
	if (chdir(rundir.c_str()) < 0) {
	    LOGERR(("chdir(%s) failed, errno %d\n", 
		    rundir.c_str(), errno));
	}
    }

    bool rezero((op_flags & OPT_z) != 0);
    bool inPlaceReset((op_flags & OPT_Z) != 0);

    // We do not retry previously failed files by default. If -k is
    // set, we do.  If the checker script says so, we do too.
    int indexerFlags = ConfIndexer::IxFNoRetryFailed;
    if (op_flags & OPT_k) {
        indexerFlags &= ~ConfIndexer::IxFNoRetryFailed; 
    } else {
        if (checkRetryFailed(config, false)) {
            indexerFlags &= ~ConfIndexer::IxFNoRetryFailed; 
        }
    }

    Pidfile pidfile(config->getPidfile());
    updater = new MyUpdater(config);

    // Log something at LOGINFO to reset the trace file. Else at level
    // 3 it's not even truncated if all docs are up to date.
    LOGINFO(("recollindex: starting up\n"));

    if (setpriority(PRIO_PROCESS, 0, 20) != 0) {
        LOGINFO(("recollindex: can't setpriority(), errno %d\n", errno));
    }
    // Try to ionice. This does not work on all platforms
    rclIxIonice(config);

    if (op_flags & OPT_r) {
	if (argc != 1) 
	    Usage();
	string top = *argv++; argc--;
	bool status = recursive_index(config, top, selpatterns);
        if (confindexer && !confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;
        exit(status ? 0 : 1);
    } else if (op_flags & (OPT_i|OPT_e)) {
	lockorexit(&pidfile);

	list<string> filenames;

	if (argc == 0) {
	    // Read from stdin
	    char line[1024];
	    while (fgets(line, 1023, stdin)) {
		string sl(line);
		trimstring(sl, "\n\r");
		filenames.push_back(sl);
	    }
	} else {
	    while (argc--) {
		filenames.push_back(*argv++);
	    }
	}

        // Note that -e and -i may be both set. In this case we first erase,
        // then index. This is a slightly different from -Z -i because we 
        // warranty that all subdocs are purged.
        bool status = true;
	if (op_flags & OPT_e) {
	    status = purgefiles(config, filenames);
        }
        if (status && (op_flags & OPT_i)) {
	    status = indexfiles(config, filenames);
        }
        if (confindexer && !confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;
        exit(status ? 0 : 1);
    } else if (op_flags & OPT_l) {
	if (argc != 0) 
	    Usage();
	vector<string> stemmers = ConfIndexer::getStemmerNames();
	for (vector<string>::const_iterator it = stemmers.begin(); 
	     it != stemmers.end(); it++) {
	    cout << *it << endl;
	}
	exit(0);
    } else if (op_flags & OPT_s) {
	if (argc != 1) 
	    Usage();
	string lang = *argv++; argc--;
	exit(!createstemdb(config, lang));
#ifdef RCL_USE_ASPELL
    } else if (op_flags & OPT_S) {
	makeIndexerOrExit(config, false);
        exit(!confindexer->createAspellDict());
#endif // ASPELL

#ifdef RCL_MONITOR
    } else if (op_flags & OPT_m) {
	if (argc != 0) 
	    Usage();
	lockorexit(&pidfile);
	if (!(op_flags&OPT_D)) {
	    LOGDEB(("recollindex: daemonizing\n"));
	    if (daemon(0,0) != 0) {
	      fprintf(stderr, "daemon() failed, errno %d\n", errno);
	      LOGERR(("daemon() failed, errno %d\n", errno));
	      exit(1);
	    }
	}
	// Need to rewrite pid, it changed
	pidfile.write_pid();

        // Not too sure if I have to redo the nice thing after daemon(),
        // can't hurt anyway (easier than testing on all platforms...)
        if (setpriority(PRIO_PROCESS, 0, 20) != 0) {
            LOGINFO(("recollindex: can't setpriority(), errno %d\n", errno));
        }
	// Try to ionice. This does not work on all platforms
	rclIxIonice(config);

	if (sleepsecs > 0) {
	    LOGDEB(("recollindex: sleeping %d\n", sleepsecs));
	    for (int i = 0; i < sleepsecs; i++) {
	      sleep(1);
	      // Check that x11 did not go away while we were sleeping.
	      if (!(op_flags & OPT_x) && !x11IsAlive()) {
		LOGDEB(("X11 session went away during initial sleep period\n"));
		exit(0);
	      }
	    }
	}
	if (!(op_flags & OPT_n)) {
	    makeIndexerOrExit(config, inPlaceReset);
	    LOGDEB(("Recollindex: initial indexing pass before monitoring\n"));
	    if (!confindexer->index(rezero, ConfIndexer::IxTAll, indexerFlags)
                || stopindexing) {
		LOGERR(("recollindex, initial indexing pass failed, "
			"not going into monitor mode\n"));
		exit(1);
	    } else {
                // Record success of indexing pass with failed files retries.
                if (!(indexerFlags & ConfIndexer::IxFNoRetryFailed)) {
                    checkRetryFailed(config, true);
                }
            }
	    deleteZ(confindexer);
	    o_reexec->insertArgs(vector<string>(1, "-n"));
	    LOGINFO(("recollindex: reexecuting with -n after initial full pass\n"));
	    // Note that -n will be inside the reexec when we come
	    // back, but the monitor will explicitely strip it before
	    // starting a config change exec to ensure that we do a
	    // purging pass in this case.
	    o_reexec->reexec();
	}
        if (updater) {
	    updater->status.phase = DbIxStatus::DBIXS_MONITOR;
	    updater->status.fn.clear();
	    updater->update();
	}
	int opts = RCLMON_NONE;
	if (op_flags & OPT_D)
	    opts |= RCLMON_NOFORK;
	if (op_flags & OPT_C)
	    opts |= RCLMON_NOCONFCHECK;
	if (op_flags & OPT_x)
	    opts |= RCLMON_NOX11;
	bool monret = startMonitor(config, opts);
	MONDEB(("Monitor returned %d, exiting\n", monret));
	exit(monret == false);
#endif // MONITOR

    } else if (op_flags & OPT_b) {
        cerr << "Not yet" << endl;
        return 1;
    } else {
	lockorexit(&pidfile);
	makeIndexerOrExit(config, inPlaceReset);
	bool status = confindexer->index(rezero, ConfIndexer::IxTAll, 
                                         indexerFlags);

        // Record success of indexing pass with failed files retries.
        if (status && !(indexerFlags & ConfIndexer::IxFNoRetryFailed)) {
            checkRetryFailed(config, true);
        }
	if (!status) 
	    cerr << "Indexing failed" << endl;
        if (!confindexer->getReason().empty())
            cerr << confindexer->getReason() << endl;

        if (updater) {
	    updater->status.phase = DbIxStatus::DBIXS_DONE;
	    updater->status.fn.clear();
	    updater->update();
	}
	return !status;
    }
}
