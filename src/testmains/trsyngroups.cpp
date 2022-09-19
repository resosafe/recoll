/* Copyright (C) 2014-2019 J.F.Dockes
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

#include "syngroups.h"
#include "log.h"

#include <string>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstdio>

using namespace std;

static char *thisprog;

static char usage [] =
    "syngroups <synfilename> <word>\n"
    "  \n\n"
    ;
static void Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s     0x2
#define OPT_b     0x4

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    if (argc != 2) {
        Usage();
    }
    string fn = *argv++;argc--;
    string word = *argv++;argc--;

    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");
    SynGroups syns;
    syns.setfile(fn);
    if (!syns.ok()) {
    cerr << "Initialization failed\n";
    return 1;
    }

    vector<string> group = syns.getgroup(word);
    cout << group.size() << " terms in group\n";
    for (vector<string>::const_iterator it = group.begin();
     it != group.end(); it++) {
    cout << "[" << *it << "] ";
    }
    cout << endl;
    return 0;
}
