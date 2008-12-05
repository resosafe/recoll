#ifndef lint
static char rcsid[] = "@(#$Id: mimetype.cpp,v 1.21.4.1 2008-12-05 13:13:02 dockes Exp $ (C) 2004 J.F.Dockes";
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

#ifndef TEST_MIMETYPE
#include <sys/stat.h>

#include <ctype.h>
#include <string>
#include <list>

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "mimetype.h"
#include "debuglog.h"
#include "execmd.h"
#include "rclconfig.h"
#include "smallut.h"
#include "idfile.h"

// Solaris8's 'file' command doesnt understand -i
#ifndef sun
#define USE_SYSTEM_FILE_COMMAND
#endif

/// Identification of file from contents. This is called for files with
/// unrecognized extensions (none, or not known either for indexing or
/// stop list)
///
/// The system 'file' utility is not that great for us. For exemple it
/// will mistake mail folders for simple text files if there is no
/// 'Received' header, which would be the case, for exemple in a 'Sent'
/// folder. Also "file -i" does not exist on all systems, and it's
/// quite costly.  
/// So we first call the internal file identifier, which currently
/// only knows about mail, but in which we can add the more
/// current/interesting file types.
/// As a last resort we execute 'file'

static string mimetypefromdata(const string &fn, bool usfc)
{
    // In any case first try the internal identifier
    string mime = idFile(fn.c_str());

#ifdef USE_SYSTEM_FILE_COMMAND
    if (usfc && mime.empty()) {
	// Last resort: use "file -i"
	list<string> args;

	args.push_back("-i");
	args.push_back(fn);
	ExecCmd ex;
	string result;
	string cmd = "file";
	int status = ex.doexec(cmd, args, 0, &result);
	if (status) {
	    LOGERR(("mimetypefromdata: doexec: status 0x%x\n", status));
	    return string();
	}
	// LOGDEB(("mimetypefromdata: %s [%s]\n", result.c_str(), fn.c_str()));

	// The result of 'file' execution begins with the file name
	// which may contain spaces. We happen to know its size, so
	// strip it:
	result = result.substr(fn.size());
	// Now looks like ": text/plain; charset=us-ascii"
	// Split it, and take second field
	list<string> res;
	stringToStrings(result, res);
	if (res.size() <= 1)
	    return string();
	list<string>::iterator it = res.begin();
	mime = *++it;
	// Remove possible punctuation at the end
	if (mime.length() > 0 && !isalpha(mime[mime.length() - 1]))
	    mime.erase(mime.length() -1);
	// File -i will sometimes return strange stuff (ie: "very small file")
	if(mime.find("/") == string::npos) 
	    mime.clear();
    }
#endif

    return mime;
}

/// Guess mime type, first from suffix, then from file data. We also
/// have a list of suffixes that we don't touch at all (ie: .jpg,
/// etc...)
string mimetype(const string &fn, const struct stat *stp,
		RclConfig *cfg, bool usfc)
{
    if (stp) {
	if (S_ISDIR(stp->st_mode))
	    return "application/x-fsdirectory";
	if (!S_ISREG(stp->st_mode))
	    return "application/x-fsspecial";
    }
    if (cfg == 0)
	return string();

    if (cfg->inStopSuffixes(fn)) {
	LOGDEB(("mimetype: fn [%s] in stopsuffixes\n", fn.c_str()));
	return string();
    }

    // First look for suffix in mimetype map
    string::size_type dot = fn.find_last_of(".");
    string suff;
    if (dot != string::npos) {
        suff = fn.substr(dot);
	for (unsigned int i = 0; i < suff.length(); i++)
	    suff[i] = tolower(suff[i]);

	string mtype = cfg->getMimeTypeFromSuffix(suff);
	if (!mtype.empty())
	    return mtype;
    }

    // Finally examine data
    if (!stp)
	return string();
    return mimetypefromdata(fn, usfc);
}



#else // TEST->

#include <stdio.h>
#include <sys/stat.h>

#include <cstdlib>
#include <iostream>

#include "debuglog.h"
#include "rclconfig.h"
#include "rclinit.h"
#include "mimetype.h"

using namespace std;
int main(int argc, const char **argv)
{
    string reason;
    RclConfig *config = recollinit(0, 0, reason);

    if (config == 0 || !config->ok()) {
	string str = "Configuration problem: ";
	str += reason;
	fprintf(stderr, "%s\n", str.c_str());
	exit(1);
    }

    while (--argc > 0) {
	string filename = *++argv;
	struct stat st;
	if (stat(filename.c_str(), &st)) {
	    fprintf(stderr, "Can't stat %s\n", filename.c_str());
	    continue;
	}
	cout << filename << " -> " << 
	    mimetype(filename, &st, config, true) << endl;

    }
    return 0;
}


#endif // TEST
