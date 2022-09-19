/* Copyright (C) 2017-2019 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
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
#include <iostream>
#include <string>
#include "safesysstat.h"

using namespace std;

#include "log.h"

#include "rclinit.h"
#include "internfile.h"
#include "rclconfig.h"
#include "rcldoc.h"

static string thisprog;

static string usage =
    " internfile <filename> [ipath]\n"
    "  \n\n"
    ;

static void
Usage(void)
{
    cerr << thisprog << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;
#define OPT_q      0x1 

RclConfig *config;
int main(int argc, char **argv)
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
        default: Usage();    break;
        }
    argc--; argv++;
    }
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    if (argc < 1)
    Usage();
    string fn(*argv++);
    argc--;
    string ipath;
    if (argc >= 1) {
    ipath.append(*argv++);
    argc--;
    }
    string reason;
    config = recollinit(0, 0, 0, reason);

    if (config == 0 || !config->ok()) {
    string str = "Configuration problem: ";
    str += reason;
    fprintf(stderr, "%s\n", str.c_str());
    exit(1);
    }
    struct stat st;
    if (stat(fn.c_str(), &st)) {
    perror("stat");
    exit(1);
    }
    FileInterner interner(fn, &st, config, 0);
    Rcl::Doc doc;
    FileInterner::Status status = interner.internfile(doc, ipath);
    switch (status) {
    case FileInterner::FIDone:
    case FileInterner::FIAgain:
    break;
    case FileInterner::FIError:
    default:
    fprintf(stderr, "internfile failed\n");
    exit(1);
    }
    
    cout << "doc.url [[[[" << doc.url << 
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.ipath [[[[" << doc.ipath <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.mimetype [[[[" << doc.mimetype <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.fmtime [[[[" << doc.fmtime <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.dmtime [[[[" << doc.dmtime <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.origcharset [[[[" << doc.origcharset <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.meta[title] [[[[" << doc.meta["title"] <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.meta[keywords] [[[[" << doc.meta["keywords"] <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.meta[abstract] [[[[" << doc.meta["abstract"] <<
    "]]]]\n-----------------------------------------------------\n" <<
    "doc.text [[[[" << doc.text << "]]]]\n";
}
