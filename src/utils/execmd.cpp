#ifndef lint
static char rcsid[] = "@(#$Id: execmd.cpp,v 1.27 2008-10-06 06:22:47 dockes Exp $ (C) 2004 J.F.Dockes";
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
#ifndef TEST_EXECMD
#include "autoconfig.h"

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#if !defined(PUTENV_ARG_CONST)
#include <string.h>
#endif

#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>

#include "execmd.h"
#include "pathut.h"
#include "debuglog.h"
#include "smallut.h"
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#define MAX(A,B) ((A) > (B) ? (A) : (B))

/* From FreeBSD's which command */
static bool
exec_is_there(const char *candidate)
{
    struct stat fin;

    /* XXX work around access(2) false positives for superuser */
    if (access(candidate, X_OK) == 0 &&
	stat(candidate, &fin) == 0 &&
	S_ISREG(fin.st_mode) &&
	(getuid() != 0 ||
	 (fin.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0)) {
	return true;
    }
    return false;
}

bool ExecCmd::which(const string& cmd, string& exepath, const char* path)
{
    if (cmd.empty()) 
	return false;
    if (cmd[0] == '/') {
	if (exec_is_there(cmd.c_str())) {
	    exepath = cmd;
	    return true;
	} else {
	    return false;
	}
    }

    const char *pp;
    if (path) {
	pp = path;
    } else {
	pp = getenv("PATH");
    }
    if (pp == 0)
	return false;

    list<string> pels;
    stringToTokens(pp, pels, ":");
    for (list<string>::iterator it = pels.begin(); it != pels.end(); it++) {
	if (it->empty())
	    *it = ".";
	string candidate = (it->empty() ? string(".") : *it) + "/" + cmd;
	if (exec_is_there(candidate.c_str())) {
	    exepath = candidate;
	    return true;
	}
    }
    return false;
}

void  ExecCmd::putenv(const string &ea)
{
    m_env.push_back(ea);
}

/** A resource manager to ensure that execcmd cleans up if an exception is 
 * raised in the callback */
class ExecCmdRsrc {
public:
    // Pipe for data going to the command
    int pipein[2];
    // Pipe for data coming out
    int pipeout[2];
    pid_t pid;
    ExecCmdRsrc() {
	reset();
    }
    void reset() {
	pipein[0] = pipein[1] = pipeout[0] = pipeout[1] = -1;
	pid = -1;
    }
    ~ExecCmdRsrc() {
	int status;
	if (pid > 0) {
	    LOGDEB(("ExecCmd: killing cmd\n"));
            pid_t grp = getpgid(pid);
            int ret = killpg(grp, SIGTERM);
	    if (ret == 0) {
		for (int i = 0; i < 3; i++) {
		    (void)waitpid(pid, &status, WNOHANG);
		    if (kill(pid, 0) != 0)
			break;
		    sleep(1);
		    if (i == 2) {
			LOGDEB(("ExecCmd: killing (KILL) cmd\n"));
			killpg(grp, SIGKILL);
		    }
		}
	    } else {
                LOGERR(("ExecCmd: error killing process group %d: %d\n",
                        grp, errno));
            }
	}
	if (pipein[0] >= 0)
	    close(pipein[0]);
	if (pipein[1] >= 0)
	    close(pipein[1]);
	if (pipeout[0] >= 0)
	    close(pipeout[0]);
	if (pipeout[1] >= 0)
	    close(pipeout[1]);
    }
};

int ExecCmd::doexec(const string &cmd, const list<string>& args,
		const string *inputstring, string *output)
{
    // Need something to take note of my own errors (apart from the command's)
    bool haderror = false;

    { // Debug and logging
	string command = cmd + " ";
	for (list<string>::const_iterator it = args.begin();it != args.end();
	     it++) {
	    command += "{" + *it + "} ";
	}
	LOGDEB(("ExecCmd::doexec: (%p|%p) %s\n", 
		inputstring, output, command.c_str()));
    }
    const char *input = inputstring ? inputstring->data() : 0;
    unsigned int inputlen = inputstring ? inputstring->length() : 0;

    ExecCmdRsrc e;

    if (input && pipe(e.pipein) < 0) {
	LOGERR(("ExecCmd::doexec: pipe(2) failed. errno %d\n", errno));
	return -1;
    }
    if (output && pipe(e.pipeout) < 0) {
	LOGERR(("ExecCmd::doexec: pipe(2) failed. errno %d\n", errno));
	return -1;
    }

    e.pid = fork();
    if (e.pid < 0) {
	LOGERR(("ExecCmd::doexec: fork(2) failed. errno %d\n", errno));
	return -1;
    }

    if (e.pid) {
        // Set the process group for the child. This is also done in the
        // child process see wikipedia(Process_group)
        if (setpgid(e.pid, e.pid)) {
            LOGERR(("ExecCmd: father failed setting pgid of son process\n"));
        }
	// Father process
	sigset_t blkcld;
	sigemptyset(&blkcld);
	sigaddset(&blkcld, SIGCHLD);
	pthread_sigmask(SIG_BLOCK, &blkcld, 0);

	if (input) {
	    close(e.pipein[0]);
	    e.pipein[0] = -1;
	    fcntl(e.pipein[1], F_SETFL, O_NONBLOCK);
	}
	if (output) {
	    close(e.pipeout[1]);
	    e.pipeout[1] = -1;
	    fcntl(e.pipeout[0], F_SETFL, O_NONBLOCK);
	}

	if (input || output) {
	    unsigned int nwritten = 0;
	    int nfds = MAX(e.pipein[1], e.pipeout[0]) + 1;
	    fd_set readfds, writefds;
	    struct timeval tv;
	    tv.tv_sec = m_timeoutMs / 1000;
	    tv.tv_usec = 1000 * (m_timeoutMs % 1000);
	    for(; nfds > 0;) {
		if (m_cancelRequest)
		    break;

		FD_ZERO(&writefds);
		FD_ZERO(&readfds);
		if (e.pipein[1] >= 0)
		    FD_SET(e.pipein[1], &writefds);
		if (e.pipeout[0] >= 0)
		    FD_SET(e.pipeout[0], &readfds);
		nfds = MAX(e.pipein[1], e.pipeout[0]) + 1;
		//struct timeval to; to.tv_sec = 1;to.tv_usec=0;
		//cerr << "e.pipein[1] "<< e.pipein[1] << " e.pipeout[0] " << 
		//e.pipeout[0] << " nfds " << nfds << endl;
		int ss;
		if ((ss = select(nfds, &readfds, &writefds, 0, &tv)) <= 0) {
                    // Call m_advise even on error (if this was due to
                    // a signal, we may not want to wait for the son
                    // process), m_advise can call setCancel() or
                    // raise an exception.
                    if (m_advise)
                        m_advise->newData(0);
		    if (ss == 0) {
			// Timeout, is ok.
			continue;
		    }
		    LOGERR(("ExecCmd::doexec: select(2) failed. errno %d\n", 
			    errno));
		    haderror = true;
		    break;
		}
		if (e.pipein[1] >= 0 && FD_ISSET(e.pipein[1], &writefds)) {
		    int n = write(e.pipein[1], input + nwritten, 
				  inputlen - nwritten);
		    if (n < 0) {
			LOGERR(("ExecCmd::doexec: write(2) failed. errno %d\n",
				errno));
			haderror = true;
			goto out;
		    }
		    nwritten += n;
		    if (nwritten == inputlen) {
			if (m_provide) {
			    m_provide->newData();
			    if (inputstring->empty()) {
				close(e.pipein[1]);
				e.pipein[1] = -1;
			    } else {
				input = inputstring->data();
				inputlen = inputstring->length();
				nwritten = 0;
			    }
			} else {
			    // cerr << "Closing output" << endl;
			    close(e.pipein[1]);
			    e.pipein[1] = -1;
			}
		    }
		}
		if (e.pipeout[0] > 0 && FD_ISSET(e.pipeout[0], &readfds)) {
		    char buf[8192];
		    int n = read(e.pipeout[0], buf, 8192);
		    if (n == 0) {
			goto out;
		    } else if (n < 0) {
			LOGERR(("ExecCmd::doexec: read(2) failed. errno %d\n",
				errno));
			haderror = true;
			goto out;
		    } else if (n > 0) {
			// cerr << "READ: " << n << endl;
			output->append(buf, n);
			if (m_advise)
			    m_advise->newData(n);
		    }
		}
	    }
	}

    out:
	int status = -1;
	if (!m_cancelRequest) {
	    (void)waitpid(e.pid, &status, 0);
	    e.pid = -1;
	}
	LOGDEB1(("ExecCmd::doexec: father got status 0x%x\n", status));
	pthread_sigmask(SIG_UNBLOCK, &blkcld, 0);

	return haderror ? -1 : status;

    } else {
	// In child process. Set up pipes, environment, and exec command

	if (input) {
	    close(e.pipein[1]);
	    e.pipein[1] = -1;
	    if (e.pipein[0] != 0) {
		dup2(e.pipein[0], 0);
		close(e.pipein[0]);
		e.pipein[0] = -1;
	    }
	}
	if (output) {
	    close(e.pipeout[0]);
	    e.pipeout[0] = -1;
	    if (e.pipeout[1] != 1) {
		if (dup2(e.pipeout[1], 1) < 0) {
		    LOGERR(("ExecCmd::doexec: dup2(2) failed. errno %d\n",
			    errno));
		}
		if (close(e.pipeout[1]) < 0) {
		    LOGERR(("ExecCmd::doexec: close(2) failed. errno %d\n",
			    errno));
		}
		e.pipeout[1] = -1;
	    }
	}
	// Do we need to redirect stderr ?
	if (!m_stderrFile.empty()) {
	    int fd = open(m_stderrFile.c_str(), O_WRONLY|O_CREAT
#ifdef O_APPEND
			  |O_APPEND
#endif
			  , 0600);
	    if (fd < 0) {
		close(2);
	    } else {
		if (fd != 2) {
		    dup2(fd, 2);
		}
		lseek(2, 0, 2);
	    }
	}
	e.reset();

        // Start our own process group
        setpgid(0, getpid());

	// Allocate arg vector (2 more for arg0 + final 0)
	typedef const char *Ccharp;
	Ccharp *argv;
	argv = (Ccharp *)malloc((args.size()+2) * sizeof(char *));
	if (argv == 0) {
	    LOGERR(("ExecCmd::doexec: malloc() failed. errno %d\n",
		    errno));
	    exit(1);
	}
	
	// Fill up argv
	argv[0] = path_getsimple(cmd).c_str();
	int i = 1;
	list<string>::const_iterator it;
	for (it = args.begin(); it != args.end(); it++) {
	    argv[i++] = it->c_str();
	}
	argv[i] = 0;

#if 0
	{int i = 0;cerr << "cmd: " << cmd << endl << "ARGS: " << endl; 
	    while (argv[i]) cerr << argv[i++] << endl;}
#endif

	for (vector<string>::const_iterator it = m_env.begin(); 
	     it != m_env.end(); it++) {
#ifdef PUTENV_ARG_CONST
	    ::putenv(it->c_str());
#else
	    ::putenv(strdup(it->c_str()));
#endif
	}
	execvp(cmd.c_str(), (char *const*)argv);
	// Hu ho
	LOGERR(("ExecCmd::doexec: execvp(%s) failed. errno %d\n", cmd.c_str(),
		errno));
	_exit(127);
    }
    /* This cant be reached: to make cc happy */
    return -1;
}



////////////////////////////////////////////////////////////////////
#else // TEST
#include <stdio.h>
#include <string>
#include <iostream>
#include <list>
#include "debuglog.h"
#include "cancelcheck.h"

using namespace std;

#include "execmd.h"

const char *data = "Une ligne de donnees\n";
class MEAdv : public ExecCmdAdvise {
public:
    ExecCmd *cmd;
    void newData(int cnt) {
	cerr << "newData(" << cnt << ")" << endl;
	//	CancelCheck::instance().setCancel();
	//	CancelCheck::instance().checkCancel();
	//	cmd->setCancel();
    }
};

class MEPv : public ExecCmdProvide {
public:
    FILE *m_fp;
    string *m_input;
    MEPv(string *i) 
	: m_input(i)
    {
	m_fp = fopen("/etc/group", "r");
    }
    ~MEPv() {
	if (m_fp)
	    fclose(m_fp);
    }
    void newData() {
	char line[1024];
	if (m_fp && fgets(line, 1024, m_fp)) {
	    m_input->assign((const char *)line);
	} else {
	    m_input->erase();
	}
    }
};


static char *thisprog;
static char usage [] =
"execmd cmd [arg1 arg2 ...]\n" 
"  \n\n"
;
static void Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s	  0x2 
#define OPT_b	  0x4 
#define OPT_w     0x8
int main(int argc, char **argv)
{
    int count = 10;
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'b':	op_flags |= OPT_b; if (argc < 2)  Usage();
		if ((sscanf(*(++argv), "%d", &count)) != 1) 
		    Usage(); 
		argc--; 
		goto b1;
	    case 's':	op_flags |= OPT_s; break;
	    case 'w':	op_flags |= OPT_w; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc < 1)
	Usage();

    string cmd = *argv++; argc--;
    list<string> l;
    while (argc > 0) {
	l.push_back(*argv++); argc--;
    }

    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    if (op_flags & OPT_w) {
	string path;
	if (ExecCmd::which(cmd, path)) {
	    cout << path << endl;
	    exit(0);
	} 
	exit(1);
    }
    ExecCmd mexec;
    MEAdv adv;
    adv.cmd = &mexec;
    mexec.setAdvise(&adv);
    mexec.setTimeout(500);
    mexec.setStderr("/tmp/trexecStderr");
    mexec.putenv("TESTVARIABLE1=TESTVALUE1");
    mexec.putenv("TESTVARIABLE2=TESTVALUE2");
    mexec.putenv("TESTVARIABLE3=TESTVALUE3");

    string input, output;
    //    input = data;
    string *ip = 0;
    ip = &input;

    MEPv  pv(&input);
    mexec.setProvide(&pv);

    int status = -1;
    try {
	status = mexec.doexec(cmd, l, ip, &output);
    } catch (CancelExcept) {
	cerr << "CANCELED" << endl;
    }

    fprintf(stderr, "Status: 0x%x\n", status);
    cout << "Output:[" << output << "]" << endl;
    exit (status >> 8);
}
#endif // TEST
