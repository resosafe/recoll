/* Copyright (C) 2021-2021 J.F.Dockes
 *
 * License: LGPL 2.1
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

// A test driver for the plaintorich highlighting module.
// Very incomplete yet, nothing about groups

#include "autoconfig.h"

#include "plaintorich.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>

#include <iostream>

#include "readfile.h"
#include "smallut.h"
#include "log.h"

using namespace std;

#define OPT_H 0x1
#define OPT_L 0x2   
#define OPT_t 0x4

static string thisprog;

static string usage =
    " plaintorich [opts] [filename]\n"
    "   -H : input is html\n"
    "   -L <loglevel> : set log level 0-6\n"
    "   -t term : add term or group to match. Must in index form (lowercase/unac)\n"
    " Omit filename to read from stdin\n";
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;

class TstPlainToRich : public PlainToRich {
public:
    virtual string startMatch(unsigned int idx) {
        (void)idx;
#if 0
        if (m_hdata) {
            string s1, s2;
            stringsToString<vector<string> >(m_hdata->groups[idx], s1); 
            stringsToString<vector<string> >(
                m_hdata->ugroups[m_hdata->grpsugidx[idx]], s2);
            LOGDEB2("Reslist startmatch: group " << s1 << " user group " <<
                    s2 << "\n");
        }
#endif                
        return "<SPAN class='RCLMATCH'>";
    }
        
    virtual string endMatch() {
        return "</SPAN>";
    }
};

int main(int argc, char **argv)
{
    int loglevel = 4;
    HighlightData hldata;
    bool ishtml{false};
    
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            /* Cas du "adb - core" */
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'H':   ishtml = true; break;
            case 'L':   op_flags |= OPT_L; if (argc < 2)  Usage();
                loglevel = atoi(*(++argv)); argc--; goto b1;
            case 't':   {
                if (argc < 2)  Usage();
                HighlightData::TermGroup tg;
                string in = *(++argv); argc--;
                // Note that this is not exactly how the query processor would split the group, but
                // it should be enough for generating groups for testing.
                vector<string> vterms;
                stringToTokens(in, vterms);
                if (vterms.size() == 1) {
                    cerr << "SINGLE TERM\n";
                    tg.term = in;
                    hldata.ugroups.push_back(vector<string>{in});
                } else {
                    for (const auto& term : vterms) {
                        tg.orgroups.push_back(vector<string>{term});
                    }
                    tg.slack = 0;
                    tg.kind = HighlightData::TermGroup::TGK_NEAR;
                    hldata.ugroups.push_back(vterms);
                }
                hldata.index_term_groups.push_back(tg);
                goto b1;
            }
            default: Usage();   break;
            }
    b1: argc--; argv++;
    }

    Logger::getTheLog("")->setLogLevel(Logger::LogLevel(loglevel));

    string inputdata, reason;
    if (argc == 1) {
        const char *filename = *argv++; argc--;
        if (!file_to_string(filename, inputdata, &reason)) {
            cerr << "Failed: file_to_string(" << filename << ") failed: " << reason << endl;
            exit(1);
        }
    } else if (argc == 0) {
        char buf[1024];
        int nread;
        while ((nread = read(0, buf, 1024)) > 0) {
            inputdata.append(buf, nread);
        }
    } else {
        Usage();
    }
            

    TstPlainToRich hiliter;
    if (ishtml) {
        hiliter.set_inputhtml(true);
    }
    std::cout << "Using hldata: " << hldata.toString() << "\n";
    std::list<std::string> result;
    hiliter.plaintorich(inputdata, result, hldata);
    std::cout << *result.begin() << "\n";
    return 0;
}
