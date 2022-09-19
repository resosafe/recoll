/* Copyright (C) 2017-2019 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation; either version 2.1 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "fstreewalk.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <iostream>

#include "rclinit.h"
#include "rclconfig.h"
#include "pathut.h"

using namespace std;

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_p     0x2 
#define OPT_P     0x4 
#define OPT_r     0x8
#define OPT_c     0x10
#define OPT_b     0x20
#define OPT_d     0x40
#define OPT_m     0x80
#define OPT_L     0x100
#define OPT_w     0x200
#define OPT_M     0x400
#define OPT_D     0x800
#define OPT_k     0x1000
#define OPT_y     0x2000
#define OPT_s     0x4000
#define OPT_S     0x8000

class myCB : public FsTreeWalkerCB {
public:
    FsTreeWalker::Status processone(const string &path, 
                                    const struct PathStat *st,
                                    FsTreeWalker::CbFlag flg) {
        if (flg == FsTreeWalker::FtwDirEnter) {
            if (op_flags & OPT_r) {
                cout << path << endl;
            } else {
                if (!(op_flags&OPT_s)) {
                    cout << "[Entering " << path << "]" << endl;
                }
            }
        } else if (flg == FsTreeWalker::FtwDirReturn) {
            if (!(op_flags&OPT_s)) {
                cout << "[Returning to " << path << "]" << endl;
            }
        } else if (flg == FsTreeWalker::FtwRegular) {
            cout << path << endl;
        } else if (flg == FsTreeWalker::FtwSkipped) {
            cout << "SKIPPED: " << path << endl;
        }
        return FsTreeWalker::FtwOk;
    }
};

static const char *thisprog;

// Note that breadth first sorting is relatively expensive: less inode
// locality, more disk usage (and also more user memory usage, does
// not appear here). Some typical results on a real tree with 2.6
// million entries (220MB of name data)
// Recoll 1.13
// time trfstreewalk / > /data/tmp/old
// real    13m32.839s user    0m4.443s sys     0m31.128s
// 
// Recoll 1.14
// time trfstreewalk / > /data/tmp/nat;
// real    13m28.685s user    0m4.430s sys     0m31.083s
// time trfstreewalk -d / > /data/tmp/depth;
// real    13m30.051s user    0m4.140s sys     0m33.862s
// time trfstreewalk -m / > /data/tmp/mixed;
// real    14m53.245s user    0m4.244s sys     0m34.494s
// time trfstreewalk -b / > /data/tmp/breadth;
// real    17m10.585s user    0m4.532s sys     0m35.033s

static char usage [] =
                    "trfstreewalk [-p pattern] [-P ignpath] [-r] [-c] [-L] topdir\n"
                    " -D : skip dotfiles\n"
                    " -L : follow symbolic links\n"
                    " -M <depth>: limit depth (works with -b/m/d)\n"
                    " -P <pattern> : add skippedPaths entry\n"
                    " -p <pattern> : add skippedNames entry\n"
                    " -b : use breadth first walk\n"
                    " -c : no path canonification\n"
                    " -d : use almost depth first (dir files, then subdirs)\n"
                    " -k : like du\n"
                    " -m : use breadth up to 4 deep then switch to -d\n"
                    " -r : norecurse\n"
                    " -s : don't print dir change info\n"
                    " -w : unset default FNM_PATHNAME when using fnmatch() to match skipped paths\n"
                    " -y <pattern> : add onlyNames entry\n"
                    " -S : only print skipped files and directories\n";
                    ;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

int main(int argc, const char **argv)
{
    vector<string> skpnames;
    vector<string> onlynames;
    vector<string> skppaths;
    int maxdepth = -1;

    thisprog = argv[0];
    argc--; argv++;
    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'b':   op_flags |= OPT_b; break;
            case 'c':   op_flags |= OPT_c; break;
            case 'd':   op_flags |= OPT_d; break;
            case 'D':   op_flags |= OPT_D; break;
            case 'k':   op_flags |= OPT_k; break;
            case 'L':   op_flags |= OPT_L; break;
            case 'm':   op_flags |= OPT_m; break;
            case 'M':   op_flags |= OPT_M; if (argc < 2)  Usage();
                maxdepth = atoi(*(++argv));
                argc--; 
                goto b1;
            case 'p':   op_flags |= OPT_p; if (argc < 2)  Usage();
                skpnames.push_back(*(++argv));
                argc--; 
                goto b1;
            case 'P':   op_flags |= OPT_P; if (argc < 2)  Usage();
                skppaths.push_back(*(++argv));
                argc--; 
                goto b1;
            case 'r':   op_flags |= OPT_r; break;
            case 's':   op_flags |= OPT_s; break;
            case 'S':   op_flags |= OPT_S; break;
            case 'w':   op_flags |= OPT_w; break;
            case 'y':   op_flags |= OPT_y; if (argc < 2)  Usage();
                onlynames.push_back(*(++argv));
                argc--; 
                goto b1;
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    if (argc != 1)
        Usage();
    string topdir = *argv++;argc--;

    if (op_flags & OPT_k) {
        int64_t bytes = fsTreeBytes(topdir);
        if (bytes < 0) {
            cerr << "fsTreeBytes failed\n";
            return 1;
        } else {
            cout << bytes / 1024 << "\t" << topdir << endl;
            return 0;
        }
    }
    
    int opt = 0;
    if (op_flags & OPT_r)
        opt |= FsTreeWalker::FtwNoRecurse;
    if (op_flags & OPT_c)
        opt |= FsTreeWalker::FtwNoCanon;
    if (op_flags & OPT_L)
        opt |= FsTreeWalker::FtwFollow;
    if (op_flags & OPT_D)
        opt |= FsTreeWalker::FtwSkipDotFiles;
    if (op_flags & OPT_S)
        opt |= FsTreeWalker::FtwOnlySkipped;

    if (op_flags & OPT_b)
        opt |= FsTreeWalker::FtwTravBreadth;
    else if (op_flags & OPT_d)
        opt |= FsTreeWalker::FtwTravFilesThenDirs;
    else if (op_flags & OPT_m)
        opt |= FsTreeWalker::FtwTravBreadthThenDepth;

    
    string reason;
    if (!recollinit(0, 0, 0, reason)) {
        fprintf(stderr, "Init failed: %s\n", reason.c_str());
        exit(1);
    }
    if (op_flags & OPT_w) {
        FsTreeWalker::setNoFnmPathname();
    }
    FsTreeWalker walker;
    walker.setOpts(opt); 
    walker.setMaxDepth(maxdepth);
    walker.setSkippedNames(skpnames);
    walker.setOnlyNames(onlynames);
    walker.setSkippedPaths(skppaths);
    myCB cb;
    walker.walk(topdir, cb);
    if (walker.getErrCnt() > 0)
        cout << walker.getReason();
}
