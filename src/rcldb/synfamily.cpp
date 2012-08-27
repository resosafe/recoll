/* Copyright (C) 2012 J.F.Dockes
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifndef TEST_SYNFAMILY

#include "autoconfig.h"

#include "debuglog.h"
#include "xmacros.h"
#include "synfamily.h"

#include <iostream>

using namespace std;

namespace Rcl {

bool XapSynFamily::synExpand(const string& member, const string& term,
			     vector<string>& result)
{
    string key = entryprefix(member) + term;
    string ermsg;
    try {
	for (Xapian::TermIterator xit = m_rdb.synonyms_begin(key);
	     xit != m_rdb.synonyms_end(key); xit++) {
	    result.push_back(*xit);
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("synFamily::synExpand: error for member [%s] term [%s]\n",
		member.c_str(), term.c_str()));
	return false;
    }
#if 0
    string out;
    stringsToString(result, out);
    LOGDEB0(("XapSynFamily::synExpand:%s: [%s] -> %s\n", member.c_str(), 
	     term.c_str(), out.c_str()));
#endif
    return true;
}

bool XapSynFamily::getMembers(vector<string>& members)
{
    string key = memberskey();
    string ermsg;
    try {
	for (Xapian::TermIterator xit = m_rdb.synonyms_begin(key);
	     xit != m_rdb.synonyms_end(key); xit++) {
	    members.push_back(*xit);
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("XapSynFamily::getMembers: xapian error %s\n", ermsg.c_str()));
	return false;
    }
    return true;
}

bool XapSynFamily::listMap(const string& membername)
{
    string key = entryprefix(membername);
    string ermsg;
    try {
	for (Xapian::TermIterator kit = m_rdb.synonym_keys_begin(key);
	     kit != m_rdb.synonym_keys_end(key); kit++) {
	    cout << "[" << *kit << "] -> ";
	    for (Xapian::TermIterator xit = m_rdb.synonyms_begin(*kit);
		 xit != m_rdb.synonyms_end(*kit); xit++) {
		cout << *xit << " ";
	    }
	    cout << endl;
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("XapSynFamily::listMap: xapian error %s\n", ermsg.c_str()));
	return false;
    }
    vector<string>members;
    getMembers(members);
    cout << "All family members: ";
    for (vector<string>::const_iterator it = members.begin();
	 it != members.end(); it++) {
	cout << *it << " ";
    }
    cout << endl;
    return true;
}

bool XapWritableSynFamily::deleteMember(const string& membername)
{
    string key = entryprefix(membername);

    for (Xapian::TermIterator xit = m_wdb.synonym_keys_begin(key);
	 xit != m_wdb.synonym_keys_end(key); xit++) {
	m_wdb.clear_synonyms(*xit);
    }
    m_wdb.remove_synonym(memberskey(), membername);
    return true;
}

bool XapWritableSynFamily::createMember(const string& membername)
{
    string ermsg;
    try {
	m_wdb.add_synonym(memberskey(), membername);
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("XapSynFamily::createMember: xapian error %s\n", ermsg.c_str()));
	return false;
    }
    return true;
}

bool XapWritableSynFamily::addSynonyms(const string& membername, 
				       const string& term, 
				       const vector<string>& trans)
{
    string key = entryprefix(membername) + term;
    string ermsg;
    try {
	for (vector<string>::const_iterator it = trans.begin();
	     it != trans.end(); it++) {
	    m_wdb.add_synonym(key, *it);
	}
    } XCATCHERROR(ermsg);
    if (!ermsg.empty()) {
	LOGERR(("XapSynFamily::addSynonyms: xapian error %s\n", ermsg.c_str()));
	return false;
    }
    return true;
}

}

#else  // TEST_SYNFAMILY 
#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <strings.h>

#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "xapian.h"

#include "smallut.h"
#include "pathut.h"
#include "xmacros.h"
#include "synfamily.h"

static string thisprog;
static int        op_flags;
#define OPT_a     0x4
#define OPT_c     0x8
#define OPT_D     0x1
#define OPT_d     0x10
#define OPT_L     0x2
#define OPT_l     0x20
#define OPT_s     0x40
#define OPT_e     0x80
static string usage =
    " -d <dbdir> {-s|-a|-c} database dir and synfamily: stem accents case\n"
    " -l : list members\n"
    " -L <member>: list entries for given member\n"
    " -e <member> <key> : list expansion for given member and key\n"
    " -D <member>: delete member\n"
    "  \n\n"
    ;
static void Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

int main(int argc, char **argv)
{
    string dbdir(path_tildexpand("~/.recoll/xapiandb"));
    string outencoding = "UTF-8";
    string member;
    string key;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    /* Cas du "adb - core" */
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'a':	op_flags |= OPT_a; break;
	    case 'c':	op_flags |= OPT_c; break;
	    case 'D':	op_flags |= OPT_D; break;
	    case 'd':	op_flags |= OPT_d; if (argc < 2)  Usage();
		dbdir = *(++argv); argc--; 
		goto b1;
	    case 'e':	op_flags |= OPT_e; if (argc < 3)  Usage();
		member = *(++argv);argc--;
		key = *(++argv); argc--;
		goto b1;
	    case 'l':	op_flags |= OPT_l; break;
	    case 'L':	op_flags |= OPT_L; if (argc < 2)  Usage();
		member = *(++argv); argc--; 
		goto b1;
	    case 's':	op_flags |= OPT_s; break;
	    default: Usage();	break;
	    }
    b1: argc--; argv++;
    }

    if (argc != 0)
	Usage();

    // We do stem only for now
    string familyname;
    if (op_flags & (OPT_a|OPT_c)) {
	cerr << "Accents and case not ready" << endl;
	return 1;
    } else {
	op_flags |= OPT_s;
	familyname = Rcl::synFamStem;
    }
    if ((op_flags & (OPT_l|OPT_L|OPT_D|OPT_e)) == 0)
	Usage();

    string ermsg;
    try {
	if ((op_flags & (OPT_D)) == 0) { // Need write ?
	    Xapian::Database db(dbdir);
	    Rcl::XapSynFamily fam(db, familyname);
	    if (op_flags & OPT_l) {
		vector<string> members;
		if (!fam.getMembers(members)) {
		    cerr << "getMembers error" << endl;
		    return 1;
		}
		string out;
		stringsToString(members, out);
		cout << "Family: " << familyname << " Members: " << out << endl;
	    } else if (op_flags & OPT_L) {
		fam.listMap(member); 
	    } else if (op_flags & OPT_e) {
		vector<string> exp;
		if (!fam.synExpand(member, key, exp)) {
		    cerr << "expand error" << endl;
		    return 1;
		}
		string out;
		stringsToString(exp, out);
		cout << "Family: " << familyname << " Key: " <<  key 
		     << " Expansion: " << out << endl;
	    } else {
		Usage();
	    }

	} else {
	    Xapian::WritableDatabase db(dbdir, Xapian::DB_CREATE_OR_OPEN);
	    Rcl::XapWritableSynFamily fam(db, familyname);
	    if (op_flags & OPT_D) {
	    } else {
		Usage();
	    }
	}
    } XCATCHERROR (ermsg);
    if (!ermsg.empty()) {
	cerr << "Xapian Exception: " << ermsg << endl;
	return 1;
    }
    return 0;
}

#endif // TEST_SYNFAMILY