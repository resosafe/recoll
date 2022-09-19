/* Copyright (C) 2022-2022 J.F.Dockes
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

#include <string>
#include <iostream>
#include <set>
#include <getopt.h>
#include <malloc.h>
#include <unistd.h>

#include "rclinit.h"
#include "rclconfig.h"
#include "rcldb.h"
#include "pathut.h"
#include "smallut.h"
#include "rclutil.h"
#include "log.h"

static std::map<std::string, int> options {
    {"dirlist", 0},
};

static char *thisprog;
static void Usage(FILE *fp = stderr)
{
    string sopts;
    for (const auto& opt: options) {
        sopts += "--" + opt.first + "\n";
    }
    fprintf(fp, "%s: usage: %s\n%s", thisprog, thisprog, sopts.c_str());
    exit(1);
}

int main(int argc, char *argv[])
{
    thisprog = *argv;
    std::vector<struct option> long_options;

    for (auto& entry : options) {
        struct option opt;
        opt.name = entry.first.c_str();
        opt.has_arg = 0;
        opt.flag = &entry.second;
        opt.val = 1;
        long_options.push_back(opt);
    }
    long_options.push_back({0, 0, 0, 0});

    std::string confdir;
    std::string *argcnf{nullptr};
    int opt;
    while ((opt = getopt_long(argc, argv, "c:", &long_options[0], nullptr)) != -1) {
        switch (opt) {
        case 'c':
            confdir = optarg;
            argcnf = &confdir;
            break;
        case 0:
            break;
        default:
            Usage();
        }
    }
#if 0
    for (const auto& e : options) {
        std::cerr << e.first << " -> " << e.second << "\n";
    }
#endif

    if (options["dirlist"]) {
        std::string reason;
        RclConfig *rclconfig = recollinit(0, 0, 0, reason, argcnf);
        if (!rclconfig || !rclconfig->ok()) {
            std::cerr << "Recoll init failed: " << reason << "\n";
            return 1;
        }
        Rcl::Db rcldb(rclconfig);
        if (!rcldb.open(Rcl::Db::DbRO)) {
            LOGERR("db open error\n");
            return 1;
        }
        const char *cp;
        if ((cp = getenv("RECOLL_EXTRA_DBS")) != 0) {
            vector<string> dbl;
            stringToTokens(cp, dbl, ":");
            for (const auto& path : dbl) {
                string dbdir = path_canon(path);
                path_catslash(dbdir);
                bool stripped;
                if (!Rcl::Db::testDbDir(dbdir, &stripped)) {
                    LOGERR("Not a xapian index: [" << dbdir << "]\n");
                    return 1;
                }
                if (!rcldb.addQueryDb(dbdir)) {
                    LOGERR("Can't add " << dbdir << " as extra index\n");
                    return 1;
                }
            }
        }

        std::string prefix;
        std::vector<std::string> dirs;
        rcldb.dirlist(1, prefix, dirs);
        sort(dirs.begin(), dirs.end());
        std::cout << "Prefix " << prefix << " dirs :\n";
        for (const auto& dir : dirs) {
            std::cout << dir << "\n";
        }
        return 0;
    } else {
        std::cerr << "No operation set\n";
        Usage();
    }
}
