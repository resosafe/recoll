#ifndef lint
static char rcsid[] = "@(#$Id: readfile.cpp,v 1.9 2008-12-08 11:22:58 dockes Exp $ (C) 2004 J.F.Dockes";
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
#ifndef TEST_READFILE

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef O_STREAMING
#define O_STREAMING 0
#endif
#include <errno.h>
#include <cstdio>
#include <cstring>

#include <string>

#ifndef NO_NAMESPACES
using std::string;
#endif /* NO_NAMESPACES */

#include "readfile.h"

static void caterrno(string *reason, const char *what, int _errno)
{
    if (reason) {
	reason->append(what);
	reason->append(": errno: ");
	char nbuf[10];
	sprintf(nbuf, "%d", _errno);
	reason->append(nbuf);
	reason->append(" : ");
#ifdef sun
	// Note: sun strerror is noted mt-safe ??
	reason->append(strerror(_errno));
#else
#define ERRBUFSZ 200    
	char errbuf[ERRBUFSZ];
	// There are 2 versions of strerror_r. 
	// - The GNU one returns a pointer to the message (maybe
	//   static storage or supplied buffer).
        // - The POSIX one always stores in supplied buffer and
	//   returns 0 on success. As the possibility of error and
	//   error code are not specified, we're basically doomed
	//   cause we can't use a test on the 0 value to know if we
	//   were returned a pointer... 
	// Also couldn't find an easy way to disable the gnu version without
	// changing the cxxflags globally, so forget it.
	// At worse we get no message at all here.
	errbuf[0] = 0;
	strerror_r(_errno, errbuf, ERRBUFSZ);
	reason->append(errbuf);
#endif
    }
}

class FileToString : public FileScanDo {
public:
    FileToString(string& data) : m_data(data) {}
    string& m_data;
    bool init(unsigned int size, string *reason) {
	if (size > 0)
	    m_data.reserve(size); 
	return true;
    }
    bool data(const char *buf, int cnt, string *reason) {
	try {
	    m_data.append(buf, cnt);
	} catch (...) {
	    caterrno(reason, "append", errno);
	    return false;
	}
	return true;
    }
};

bool file_to_string(const string &fn, string &data, string *reason)
{
    FileToString accum(data);
    return file_scan(fn, &accum, reason);
}

// Note: the fstat() + reserve() (in init()) calls divide cpu usage almost by 2
// on both linux i586 and macosx (compared to just append())
// Also tried a version with mmap, but it's actually slower on the mac and not
// faster on linux.
bool file_scan(const string &fn, FileScanDo* doer, string *reason)
{
    bool ret = false;
    bool noclosing = true;
    int fd = 0;
    struct stat st;
    // Initialize st_size: if fn.empty() , the fstat() call won't happen. 
    st.st_size = 0;

    // If we have a file name, open it, else use stdin.
    if (!fn.empty()) {
	fd = open(fn.c_str(), O_RDONLY|O_STREAMING);
	if (fd < 0 || fstat(fd, &st) < 0) {
	    caterrno(reason, "open/stat", errno);
	    return false;
	}
	noclosing = false;
    }
    if (st.st_size > 0)
	doer->init(st.st_size+1, reason);
    else 
	doer->init(0, reason);
    char buf[4096];
    for (;;) {
	int n = read(fd, buf, 4096);
	if (n < 0) {
	    caterrno(reason, "read", errno);
	    goto out;
	}
	if (n == 0)
	    break;

	if (!doer->data(buf, n, reason)) {
	    goto out;
	}
    }

    ret = true;
 out:
    if (fd >= 0 && !noclosing)
	close(fd);
    return ret;
}

#else // Test

#include <sys/stat.h>
#include <stdlib.h>

#include <string>
#include <iostream>
using namespace std;

#include "readfile.h"
#include "fstreewalk.h"

using namespace std;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_f	  0x2
#define OPT_F	  0x4

class myCB : public FsTreeWalkerCB {
 public:
    FsTreeWalker::Status processone(const string &path, 
				    const struct stat *st,
				    FsTreeWalker::CbFlag flg)
    {
	if (flg == FsTreeWalker::FtwDirEnter) {
	    //cout << "[Entering " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwDirReturn) {
	    //cout << "[Returning to " << path << "]" << endl;
	} else if (flg == FsTreeWalker::FtwRegular) {
	    //cout << path << endl;
	    string s, reason;
	    if (!file_to_string(path, s, &reason)) {
		cerr << "Failed: " << reason << " : " << path << endl;
	    } else {
		//cout << 
		//"================================================" << endl;
		cout << path << endl;
		//		cout << s;
	    }
	    reason.clear();
	}
	return FsTreeWalker::FtwOk;
    }
};

static const char *thisprog;
static char usage [] =
"trreadfile topdirorfile\n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    list<string> patterns;
    list<string> paths;
    thisprog = argv[0];
    argc--; argv++;

  while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
      /* Cas du "adb - core" */
      Usage();
    while (**argv)
      switch (*(*argv)++) {
      case 'f':	op_flags |= OPT_f;break;
      case 'F':	op_flags |= OPT_F;break;
      default: Usage();	break;
      }
    argc--; argv++;
  }

  if (argc != 1)
    Usage();
  string top = *argv++;argc--;

  struct stat st;
  if (stat(top.c_str(), &st) < 0) {
      perror("stat");
      exit(1);
  }
  if (S_ISDIR(st.st_mode)) {
      FsTreeWalker walker;
      myCB cb;
      walker.walk(top, cb);
      if (walker.getErrCnt() > 0)
	  cout << walker.getReason();
  } else if (S_ISREG(st.st_mode)) {
      string s, reason;
      if (!file_to_string(top, s, &reason)) {
	  cerr << reason << endl;
	  exit(1);
      } else {
	  cout << s;
      }
  }
  exit(0);
}
#endif //TEST_READFILE
