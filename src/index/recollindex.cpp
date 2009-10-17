#ifndef lint
static char rcsid[] = "@(#$Id: recollindex.cpp,v 1.38 2008-10-14 06:07:42 dockes Exp $ (C) 2004 J.F.Dockes";
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
#include <signal.h>
#include <sys/stat.h>

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

// Globals for exit cleanup
ConfIndexer *confindexer;
DbIndexer *dbindexer;

int stopindexing;
// Mainly used to request indexing stop, we currently do not use the
// current file name
class MyUpdater : public DbIxStatusUpdater {
 public:
    virtual bool update() {
	if (stopindexing) {
	    return false;
	}
	return true;
    }
};
MyUpdater updater;

static void sigcleanup(int sig)
{
    fprintf(stderr, "sigcleanup\n");
    LOGDEB(("sigcleanup\n"));
    CancelCheck::instance().setCancel();
    stopindexing = 1;
}

static bool makeDbIndexer(RclConfig *config)
{
    string dbdir = config->getDbDir();
    if (dbdir.empty()) {
	fprintf(stderr, "makeDbIndexer: no database directory in "
		"configuration for %s\n", config->getKeyDir().c_str());
	return false;
    }
    // Check if there is already an indexer for the right db
    if (dbindexer && dbindexer->getDbDir().compare(dbdir)) {
	deleteZ(dbindexer);
    }

    if (!dbindexer)
	dbindexer = new DbIndexer(config, dbdir, &updater);

    return true;
}

// The list of top directories/files wont change during program run,
// let's cache it:
static list<string> o_tdl;

// Index a list of files. We just check that they belong to one of the topdirs
// subtrees, and call the indexer method
bool indexfiles(RclConfig *config, const list<string> &filenames)
{
    if (filenames.empty())
	return true;
    
    if (o_tdl.empty()) {
	o_tdl = config->getTopdirs();
	if (o_tdl.empty()) {
	    fprintf(stderr, "Top directory list (topdirs param.) "
		    "not found in config or Directory list parse error");
	    return false;
	}
    }

    list<string> myfiles;
    for (list<string>::const_iterator it = filenames.begin(); 
	 it != filenames.end(); it++) {
	string fn = path_canon(*it);
	bool ok = false;
	// Check that this file name belongs to one of our subtrees
	for (list<string>::iterator dit = o_tdl.begin(); 
	     dit != o_tdl.end(); dit++) {
	    if (fn.find(*dit) == 0) {
		myfiles.push_back(fn);
		ok = true;
		break;
	    }
	}
	if (!ok) {
	    fprintf(stderr, "File %s not in indexed area\n", fn.c_str());
	}
    }
    if (myfiles.empty())
	return true;

    // Note: we should sort the file names against the topdirs here
    // and check for different databases. But we can for now only have
    // one database per config, so we set the keydir from the first
    // file (which is not really needed...), create the indexer/db and
    // go:
    config->setKeyDir(path_getfather(*myfiles.begin()));

    if (!makeDbIndexer(config) || !dbindexer)
	return false;
    else
	return dbindexer->indexFiles(myfiles);
}

// Delete a list of files.
bool purgefiles(RclConfig *config, const list<string> &filenames)
{
    if (filenames.empty())
	return true;
    
    if (o_tdl.empty()) {
	o_tdl = config->getTopdirs();
	if (o_tdl.empty()) {
	    fprintf(stderr, "Top directory list (topdirs param.) "
		    "not found in config or Directory list parse error");
	    return false;
	}
    }

    list<string> myfiles;
    for (list<string>::const_iterator it = filenames.begin(); 
	 it != filenames.end(); it++) {
	myfiles.push_back(path_canon(*it));
    }

    // Note: we should sort the file names against the topdirs here
    // and check for different databases. But we can for now only have
    // one database per config, so we set the keydir from the first
    // file (which is not really needed...), create the indexer/db and
    // go:
    config->setKeyDir(path_getfather(*myfiles.begin()));

    if (!makeDbIndexer(config) || !dbindexer)
	return false;
    else
	return dbindexer->purgeFiles(myfiles);
}

// Create stemming and spelling databases
bool createAuxDbs(RclConfig *config)
{
    if (!makeDbIndexer(config) || !dbindexer)
	return false;

    if (!dbindexer->createStemmingDatabases())
	return false;

    if (!dbindexer->createAspellDict())
	return false;

    return true;
}

// Create additional stem database 
static bool createstemdb(RclConfig *config, const string &lang)
{
    makeDbIndexer(config);
    if (dbindexer)
	return dbindexer->createStemDb(lang);
    else
	return false;
}

static void cleanup()
{
    deleteZ(confindexer);
    deleteZ(dbindexer);
}

static const char *thisprog;
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

static const char usage [] =
"\n"
"recollindex [-h] \n"
"    Print help\n"
"recollindex [-z] \n"
"    Index everything according to configuration file\n"
"    -z : reset database before starting indexation\n"
#ifdef RCL_MONITOR
"recollindex -m [-w <secs>] -x [-D]\n"
"    Perform real time indexation. Don't become a daemon if -D is set.\n"
"    -w sets number of seconds to wait before starting.\n"
"    -x disables exit on end of x11 session\n"
#endif
"recollindex -e <filename [filename ...]>\n"
"    Purge data for individual files. No stem database updates\n"
"recollindex -i <filename [filename ...]>\n"
"    Index individual files. No database purge or stem database updates\n"
"recollindex -l\n"
"    List available stemming languages\n"
"recollindex -s <lang>\n"
"    Build stem database for additional language <lang>\n"
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
RclConfig *RclConfig::getMainConfig() 
{
    return config;
}

int main(int argc, const char **argv)
{
    string a_config;
    int sleepsecs = 60;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
#ifdef RCL_MONITOR
	    case 'D': op_flags |= OPT_D; break;
#endif
	    case 'e': op_flags |= OPT_e; break;
	    case 'h': op_flags |= OPT_h; break;
	    case 'i': op_flags |= OPT_i; break;
	    case 'l': op_flags |= OPT_l; break;
	    case 'm': op_flags |= OPT_m; break;
	    case 's': op_flags |= OPT_s; break;
#ifdef RCL_USE_ASPELL
	    case 'S': op_flags |= OPT_S; break;
#endif
	    case 'w':	op_flags |= OPT_w; if (argc < 2)  Usage();
		if ((sscanf(*(++argv), "%d", &sleepsecs)) != 1) 
		    Usage(); 
		argc--; goto b1;
	    case 'x': op_flags |= OPT_x; break;
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

    if ((op_flags & OPT_z) && (op_flags & (OPT_i|OPT_e)))
	Usage();

    string reason;
    RclInitFlags flags = (op_flags & OPT_m) && !(op_flags&OPT_D) ? 
	RCLINIT_DAEMON : RCLINIT_NONE;
    config = recollinit(flags, cleanup, sigcleanup, reason, &a_config);
    if (config == 0 || !config->ok()) {
	cerr << "Configuration problem: " << reason << endl;
	exit(1);
    }
    bool rezero(op_flags & OPT_z);
    
    if (op_flags & (OPT_i|OPT_e)) {
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

	if (op_flags & OPT_i)
	    exit(!indexfiles(config, filenames));
	else 
	    exit(!purgefiles(config, filenames));

    } else if (op_flags & OPT_l) {
	if (argc != 0) 
	    Usage();
	list<string> stemmers = DbIndexer::getStemmerNames();
	for (list<string>::const_iterator it = stemmers.begin(); 
	     it != stemmers.end(); it++) {
	    cout << *it << endl;
	}
	exit(0);
    } else if (op_flags & OPT_s) {
	if (argc != 1) 
	    Usage();
	string lang = *argv++; argc--;
	exit(!createstemdb(config, lang));

#ifdef RCL_MONITOR
    } else if (op_flags & OPT_m) {
	if (argc != 0) 
	    Usage();
	if (!(op_flags&OPT_D)) {
	    LOGDEB(("recollindex: daemonizing\n"));
	    daemon(0,0);
	}
	if (sleepsecs > 0) {
	    LOGDEB(("recollindex: sleeping %d\n", sleepsecs));
	    sleep(sleepsecs);
	}
	// Check that x11 did not go away while we were sleeping.
	if (!(op_flags & OPT_x) && !x11IsAlive())
	    exit(0);

	confindexer = new ConfIndexer(config, &updater);
	confindexer->index(rezero);
	deleteZ(confindexer);
	int opts = RCLMON_NONE;
	if (op_flags & OPT_D)
	    opts |= RCLMON_NOFORK;
	if (op_flags & OPT_x)
	    opts |= RCLMON_NOX11;
	bool monret = startMonitor(config, opts);
	MONDEB(("Monitor returned %d, exiting\n", monret));
	exit(monret == false);
#endif // MONITOR

#ifdef RCL_USE_ASPELL
    } else if (op_flags & OPT_S) {
	makeDbIndexer(config);
	if (dbindexer)
	    exit(!dbindexer->createAspellDict());
	else
	    exit(1);
#endif // ASPELL

    } else {
	confindexer = new ConfIndexer(config, &updater);
	bool status = confindexer->index(rezero);
	if (!status) 
	    cerr << "Indexing failed" << endl;
	exit(!status);
    }
}
