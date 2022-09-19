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
#include <errno.h>
#include <string.h>

#include <string>
#include <iostream>

using namespace std;

#include "ecrontab.h"


static char *thisprog;

static char usage [] =
    " -a add or replace crontab line \n"
    " -d delete crontab line \n"
    " -s get scheduling \n"    
    " -c <string> check for unmanaged lines for string\n"
    ;

static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_MOINS 0x1
#define OPT_a      0x2 
#define OPT_d      0x4 
#define OPT_w     0x8
#define OPT_c     0x10
#define OPT_s     0x20

const string& marker("RCLCRON_RCLINDEX=");
// Note of course the -w does not make sense for a cron entry
const string& cmd0("recollindex -w ");
const string& id("RECOLL_CONFDIR=\"/home/dockes/.recoll/\"");
const string& sched("30 8 * 1 *");

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    string wt = "10";
    string cmd;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'a':    op_flags |= OPT_a; break;
            case 'c':    op_flags |= OPT_c; if (argc < 2)  Usage();
                cmd = *(++argv); argc--; 
                goto b1;
            case 'd':    op_flags |= OPT_d; break;
            case 's':    op_flags |= OPT_s; break;
            case 'w':    op_flags |= OPT_w; if (argc < 2)  Usage();
                wt = *(++argv); argc--; 
                goto b1;
      
            default: Usage();    break;
            }
    b1: argc--; argv++;
    }

    if (argc != 0)
        Usage();

    string reason;
    bool status = false;
  
    if (op_flags & OPT_a) {
        cmd = cmd0 + wt;
        status = editCrontab(marker, id, sched, cmd, reason);
    } else if (op_flags & OPT_d) {
        status = editCrontab(marker, id, sched, "", reason);
    } else if (op_flags & OPT_s) {
        vector<string> sched;
        if (!(status = getCrontabSched(marker, id, sched))) {
            cerr << "getCrontabSched failed: " << reason << endl;
            exit(1);
        }
        cout << "sched vec size " << sched.size() << endl;
        cout << "mins " << sched[0] << " hours " << sched[1] <<
            " days of month " << sched[2] << " months " << sched[3] << 
            " days of week " << sched[4] << endl;
        exit(0);
      
    } else if (op_flags & OPT_c) {
        if ((status = checkCrontabUnmanaged(marker, cmd))) {
            cerr << "crontab has unmanaged lines for " << cmd << endl;
            exit(1);
        }
        exit(0);
    } else {
        Usage();
    }
    if (!status) {
        cerr << "editCrontab failed: " << reason << endl;
        exit(1);
    }
    exit(0);
}
