#ifndef lint
static char rcsid[] = "@(#$Id: rclinit.cpp,v 1.11 2007-12-13 06:58:21 dockes Exp $ (C) 2004 J.F.Dockes";
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
#include <signal.h>
#include <locale.h>
#include <pthread.h>
#include <cstdlib>

#include "debuglog.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "pathut.h"

static const int catchedSigs[] = {SIGHUP, SIGINT, SIGQUIT, SIGTERM, 
     SIGUSR1, SIGUSR2};

RclConfig *recollinit(RclInitFlags flags, 
		      void (*cleanup)(void), void (*sigcleanup)(int), 
		      string &reason, const string *argcnf)
{
    if (cleanup)
	atexit(cleanup);

    // We ignore SIGPIPE always. All pieces of code which can write to a pipe
    // must check write() return values.
    signal(SIGPIPE, SIG_IGN);
    
    // We would like to block SIGCHLD globally, but we can't because
    // QT uses it. Have to block it inside execmd.cpp

    // Install signal handler
    if (sigcleanup) {
	for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++)
	    if (signal(catchedSigs[i], SIG_IGN) != SIG_IGN)
		signal(catchedSigs[i], sigcleanup);
    }

    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");
    RclConfig *config = new RclConfig(argcnf);
    if (!config || !config->ok()) {
	reason = "Configuration could not be built:\n";
	if (config)
	    reason += config->getReason();
	else
	    reason += "Out of memory ?";
	return 0;
    }

    // Retrieve the log file name and level
    string logfilename, loglevel;
    if (flags & RCLINIT_DAEMON) {
	config->getConfParam(string("daemlogfilename"), logfilename);
	config->getConfParam(string("daemloglevel"), loglevel);
    }
    if (logfilename.empty())
	config->getConfParam(string("logfilename"), logfilename);
    if (loglevel.empty())
	config->getConfParam(string("loglevel"), loglevel);

    // Initialize logging
    if (!logfilename.empty()) {
	logfilename = path_tildexpand(logfilename);
	// If not an absolute path or , compute relative to config dir
	if (logfilename.at(0) != '/' && 
	    !DebugLog::DebugLog::isspecialname(logfilename.c_str())) {
	    logfilename = path_cat(config->getConfDir(), logfilename);
	}
	DebugLog::setfilename(logfilename.c_str());
    }
    if (!loglevel.empty()) {
	int lev = atoi(loglevel.c_str());
	DebugLog::getdbl()->setloglevel(lev);
    }

    // Make sure the locale is set. This is only for converting file names 
    // to utf8 for indexation.
    setlocale(LC_CTYPE, "");

    return config;
}

// Signals are handled by the main thread. All others should call this routine
// to block possible signals
void recoll_threadinit()
{
    sigset_t sset;
    sigemptyset(&sset);

    for (unsigned int i = 0; i < sizeof(catchedSigs) / sizeof(int); i++)
	sigaddset(&sset, catchedSigs[i]);
    pthread_sigmask(SIG_BLOCK, &sset, 0);
}
