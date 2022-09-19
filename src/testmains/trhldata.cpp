/* Copyright (C) 2019 J.F.Dockes
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
#include <stdio.h>
#include <stdlib.h>

#include <string>
#include <iostream>
#include <vector>

#include "log.h"
#include "hldata.h"
#include "smallut.h"

using namespace std;


const char *thisprog;
static char usage [] =
"hldata\n"
" test the near/phrase matching code used for highlighting and snippets\n"
;

void Usage() {
    fprintf(stderr, "%s:%s\n", thisprog, usage);
    exit(1);
}

static int     op_flags;
#define OPT_v     0x2 

vector<CharFlags> kindflags {
    CHARFLAGENTRY(HighlightData::TermGroup::TGK_TERM),
        CHARFLAGENTRY(HighlightData::TermGroup::TGK_NEAR),
        CHARFLAGENTRY(HighlightData::TermGroup::TGK_PHRASE),
        };

// Provides a constructor for HighlightData, for easy static init.
class HLDataInitializer {
public:
    HLDataInitializer(vector<vector<string> > groups, int slack,
                      HighlightData::TermGroup::TGK kind, bool res) {
        hldata.index_term_groups.clear();
        hldata.index_term_groups.push_back(HighlightData::TermGroup());
        hldata.index_term_groups[0].orgroups = groups;
        hldata.index_term_groups[0].slack = slack;
        hldata.index_term_groups[0].kind = kind;
        expected = res;
    }
    HighlightData hldata;
    bool expected;
    void print() {
        const auto& tgp{hldata.index_term_groups[0]};
        cout << "{";
        for (const auto& group:tgp.orgroups) {
            cout << "{";
            for (const auto& term: group) {
                cout << term << ", ";
            }
            cout << "}, ";
        }
        cout << "} slack: " << tgp.slack << " kind " << valToString(kindflags, tgp.kind) << endl;
    }
};


// Data: source text (for display), 
string text1{"0 1 2 3 4"};
// Positions produced by textsplit -d from the above
unordered_map<string, vector<int> > plists1
{{"0", {0,}}, {"1", {1,}}, {"2", {2,}}, {"3", {3,}}, {"4", {4,}}, };
unordered_map<int, pair<int,int>> gpostobytes1
{{0, {0, 1}}, {1, {2, 3}}, {2, {4, 5}}, {3, {6, 7}}, {4, {8, 9}}, };


vector<HLDataInitializer> hldvec {
    {{{"0"}, {"1"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"2"}, {"3"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"3"}, {"4"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"0"}, {"1"}, {"2"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"1"}, {"2"}, {"3"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"0"}, {"1"}, {"2"}, {"3"}, {"4"}}, 0, HighlightData::TermGroup::TGK_PHRASE, true},
    {{{"0"}, {"2"}}, 1, HighlightData::TermGroup::TGK_PHRASE, true}, // slack 1 
    {{{"0"}, {"2"}}, 0, HighlightData::TermGroup::TGK_PHRASE, false},
    {{{"1"}, {"0"}}, 0, HighlightData::TermGroup::TGK_PHRASE, false},
    {{{"3"}, {"2"}}, 0, HighlightData::TermGroup::TGK_PHRASE, false},
    {{{"4"}, {"3"}}, 0, HighlightData::TermGroup::TGK_PHRASE, false},

    {{{"1"}, {"0"}}, 0, HighlightData::TermGroup::TGK_NEAR, true},
    {{{"2"}, {"0"}}, 0, HighlightData::TermGroup::TGK_NEAR, false},
    {{{"2"}, {"0"}}, 1, HighlightData::TermGroup::TGK_NEAR, true},
    {{{"4"}, {"0"}}, 2, HighlightData::TermGroup::TGK_NEAR, false},
    {{{"4"}, {"0"}}, 3, HighlightData::TermGroup::TGK_NEAR, true},
    {{{"4"}, {"3"}, {"1"}, {"2"}, {"0"}}, 1, HighlightData::TermGroup::TGK_NEAR, true},
    {{{"4"}, {"3"}, {"1"}, {"0"}}, 1, HighlightData::TermGroup::TGK_NEAR, true},
    {{{"4"}, {"3"}, {"0"}}, 1, HighlightData::TermGroup::TGK_NEAR, false},
};

int main(int argc, char **argv)
{
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
        (*argv)++;
        if (!(**argv))
            Usage();
        while (**argv)
            switch (*(*argv)++) {
            case 'v':   op_flags |= OPT_v; break;
            default: Usage();   break;
            }
        argc--;argv++;
    }

    cout << "text, bpos:\n";
    cout << "0123456789\n";
    cout << "0 1 2 3 4\n";
    for (auto& hld : hldvec) {
        vector<GroupMatchEntry> tboffs;
        bool ret = matchGroup(hld.hldata, 0, plists1, gpostobytes1, tboffs);
        if (ret && !hld.expected) {
            cout << "matchGroup: ok, expected false: ";
            hld.print();
            for (const auto& ent: tboffs) {
                cout << "{" << ent.offs.first << ", " << ent.offs.second << "} ";
            }
            cout << "\n";
        } else if (!ret && hld.expected) {
            cout << "matchGroup: failed, expected true:\n";
            hld.print();
        }
    }
}
