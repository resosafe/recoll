/* Copyright (C) 2017-2019 J.F.Dockes
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
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <iostream>

using namespace std;

#include "unacpp.h"
#include "readfile.h"
#include "rclinit.h"

static char *thisprog;

static char usage [] = "\n"
    "[-c|-C] <encoding> <infile> <outfile>\n"
    "   Default : unaccent\n"
    "   -c : unaccent and casefold\n"
    "   -C : casefold only\n"
    "-t <string> test string as capitalized, upper-case anywhere, accents\n"
    "   the parameter is supposedly utf-8 so this can only work in an utf-8\n"
    "   locale\n"
    "\n";
;

static void
Usage(void)
{
    fprintf(stderr, "%s: usage: %s\n", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_c      0x2 
#define OPT_C      0x4 
#define OPT_t     0x8

int main(int argc, char **argv)
{
    UnacOp op = UNACOP_UNAC;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
    (*argv)++;
    if (!(**argv))
        /* Cas du "adb - core" */
        Usage();
    while (**argv)
        switch (*(*argv)++) {
        case 'c':    op_flags |= OPT_c; break;
        case 'C':    op_flags |= OPT_C; break;
        case 't':    op_flags |= OPT_t; break;
        default: Usage();    break;
        }
    argc--; argv++;
    }

    if (op_flags & OPT_t) {
    if (argc != 1)
        Usage();
    string in = *argv++;argc--;
    bool capital, upper, accent;
    capital = unaciscapital(in);
    upper = unachasuppercase(in);
    accent = unachasaccents(in);
    cout << "[" << in << "] : " << 
        "capitalized: " << (capital ? "Yes. " : "No. ") <<
        "has uppercase: " << (upper ? "Yes. " : "No. ") <<
        "has accents: " << (accent ? "Yes. " : "No. ") << 
        endl;
    return 0;
    } else {
    if (argc != 3)
        Usage();
    if (op_flags & OPT_c) {
        op = UNACOP_UNACFOLD;
    } else if (op_flags & OPT_C) {
        op = UNACOP_FOLD;
    }

    const char *encoding = *argv++; argc--;
    string ifn = *argv++; argc--;
    if (!ifn.compare("stdin") || !ifn.compare("-")) {
            // file_read interprets an empty fn as "use stdin"
        ifn.clear();
        }
    const char *ofn = *argv++; argc--;

    string reason;
    (void)recollinit(RCLINIT_NONE, 0, 0, reason, 0);

    string odata;
    if (!file_to_string(ifn, odata)) {
        cerr << "file_to_string " << ifn << " : " << odata << endl;
        return 1;
    }
    string ndata;
    if (!unacmaybefold(odata, ndata, encoding, op)) {
        cerr << "unac: " << ndata << endl;
        return 1;
    }
    
    int fd;
    if (strcmp(ofn, "stdout") && strcmp(ofn, "-")) {
        fd = open(ofn, O_CREAT|O_EXCL|O_WRONLY, 0666);
    } else {
        fd = 1;
    }
    if (fd < 0) {
        cerr << "Open/Create " << ofn << " failed: " << strerror(errno) 
         << endl;
        return 1;
    }
    if (write(fd, ndata.c_str(), ndata.length()) != (int)ndata.length()) {
        cerr << "Write(2) failed: " << strerror(errno)  << endl;
        return 1;
    }
    close(fd);
    return 0;
    }
}
