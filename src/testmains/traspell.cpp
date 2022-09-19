/* Copyright (C) 2006-2019 J.F.Dockes
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

#include "autoconfig.h"

#ifdef RCL_USE_ASPELL

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <iostream>
using namespace std;

#include "rclinit.h"
#include "rclconfig.h"
#include "rcldb.h"
#include "rclaspell.h"

static char *thisprog;
RclConfig *rclconfig;

static char usage [] =
    " -b : build dictionary\n"
    " -s <term>: suggestions for term\n"
    "\n"
    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_s      0x2 
#define OPT_b      0x4 
#define OPT_c     0x8

int main(int argc, char **argv)
{
    string word;
    
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'b':    op_flags |= OPT_b; break;
            case 'c':    op_flags |= OPT_c; if (argc < 2)  Usage();
                word = *(++argv);
                argc--; 
                goto b1;
            case 's':    op_flags |= OPT_s; if (argc < 2)  Usage();
                word = *(++argv);
                argc--; 
                goto b1;
            default: Usage();    break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0 || op_flags == 0)
        Usage();

    string reason;
    rclconfig = recollinit(0, 0, 0, reason);
    if (!rclconfig || !rclconfig->ok()) {
        fprintf(stderr, "Configuration problem: %s\n", reason.c_str());
        exit(1);
    }

    string dbdir = rclconfig->getDbDir();
    if (dbdir.empty()) {
        fprintf(stderr, "No db directory in configuration");
        exit(1);
    }

    Rcl::Db rcldb(rclconfig);

    if (!rcldb.open(Rcl::Db::DbRO, 0)) {
        fprintf(stderr, "Could not open database in %s\n", dbdir.c_str());
        exit(1);
    }

    Aspell aspell(rclconfig);

    if (!aspell.init(reason)) {
        cerr << "Init failed: " << reason << endl;
        exit(1);
    }
    if (op_flags & OPT_b) {
        if (!aspell.buildDict(rcldb, reason)) {
            cerr << "buildDict failed: " << reason << endl;
            exit(1);
        }
    } else {
        list<string> suggs;
        if (!aspell.suggest(rcldb, word, suggs, reason)) {
            cerr << "suggest failed: " << reason << endl;
            exit(1);
        }
        cout << "Suggestions for " << word << ":" << endl;
        for (list<string>::iterator it = suggs.begin(); 
             it != suggs.end(); it++) {
            cout << *it << endl;
        }
    }
    exit(0);
}
#endif // RCL_USE_ASPELL
