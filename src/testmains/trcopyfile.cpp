/* Copyright (C) 2021 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>
#include <iostream>

#include "copyfile.h"

using namespace std;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_m     0x2
#define OPT_e     0x4

static const char *thisprog;
static char usage [] =
    "trcopyfile [-m] src dst\n"
    " -m : move instead of copying\n"
    " -e : fail if dest exists (only for copy)\n"
    "\n"
    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'm':   op_flags |= OPT_m; break;
            case 'e':   op_flags |= OPT_e; break;
            default: Usage();   break;
            }
        argc--; argv++;
    }

    if (argc != 2)
        Usage();
    string src = *argv++;argc--;
    string dst = *argv++;argc--;
    bool ret;
    string reason;
    if (op_flags & OPT_m) {
        ret = renameormove(src.c_str(), dst.c_str(), reason);
    } else {
        int flags = 0;
        if (op_flags & OPT_e) {
            flags |= COPYFILE_EXCL;
        }
        ret = copyfile(src.c_str(), dst.c_str(), reason, flags);
    }
    if (!ret) {
        cerr << reason << endl;
        exit(1);
    }  else {
        cout << "Succeeded" << endl;
        if (!reason.empty()) {
            cout << "Warnings: " << reason << endl;
        }
        exit(0);
    }
}
