#ifndef lint
static char rcsid[] = "@(#$Id: mh_mbox.cpp,v 1.3.2.1 2008-08-29 13:47:20 dockes Exp $ (C) 2005 J.F.Dockes";
#endif
/*
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
#ifndef TEST_MH_MBOX
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>
#include <cstring>

#include <map>
#include <sstream>

#include "mimehandler.h"
#include "debuglog.h"
#include "readfile.h"
#include "mh_mbox.h"
#include "smallut.h"

using namespace std;

MimeHandlerMbox::~MimeHandlerMbox()
{
    if (m_vfp) {
	fclose((FILE *)m_vfp);
	m_vfp = 0;
    }
}

bool MimeHandlerMbox::set_document_file(const string &fn)
{
    LOGDEB(("MimeHandlerMbox::set_document_file(%s)\n", fn.c_str()));
    m_fn = fn;
    if (m_vfp) {
	fclose((FILE *)m_vfp);
	m_vfp = 0;
    }

    m_vfp = fopen(fn.c_str(), "r");
    if (m_vfp == 0) {
	LOGERR(("MimeHandlerMail::set_document_file: error opening %s\n", 
		fn.c_str()));
	return false;
    }
    m_havedoc = true;
    return true;
}

#define LL 1024
typedef char line_type[LL+10];
static inline void stripendnl(line_type& line, int& ll)
{
    ll = strlen(line);
    while (ll > 0) {
	if (line[ll-1] == '\n' || line[ll-1] == '\r') {
	    line[ll-1] = 0;
	    ll--;
	} else 
	    break;
    }
}

// The mbox format uses lines beginning with 'From ' as separator.
// Mailers are supposed to quote any other lines beginning with 
// 'From ', turning it into '>From '. This should make it easy to detect
// message boundaries by matching a '^From ' regular expression
// Unfortunately this quoting is quite often incorrect in the real world.
//
// The rest of the format for the line is somewhat variable, but there will 
// be a 4 digit year somewhere... 
// The canonic format is the following, with a 24 characters date: 
//         From toto@tutu.com Sat Sep 30 16:44:06 2000
// This resulted into the pattern for versions up to 1.9.0: 
//         "^From .* [1-2][0-9][0-9][0-9]$"
//
// Some mailers add a time zone to the date, this is non-"standard", 
// but happens, like in: 
//    From toto@truc.com Sat Sep 30 16:44:06 2000 -0400 
//
// This is taken into account in the new regexp, which also matches more
// of the date format, to catch a few actual issues like
//     From http://www.itu.int/newsroom/press/releases/1998/NP-2.html:
// Note that this *should* have been quoted.
//
// http://www.qmail.org/man/man5/mbox.html seems to indicate that the
// fact that From_ is normally preceded by a blank line should not be
// used, but we do it anyway (for now).
// The same source indicates that arbitrary data can follow the date field
//
// A variety of pathologic From_ lines:
//   Bad date format:
//      From uucp Wed May 22 11:28 GMT 1996
//   Added timezone at the end (ok, part of the "any data" after the date)
//      From qian2@fas.harvard.edu Sat Sep 30 16:44:06 2000 -0400
//  Emacs VM botch ? Adds tz between hour and year
//      From dockes Wed Feb 23 10:31:20 +0100 2005
//      From dockes Fri Dec  1 20:36:39 +0100 2006
// The modified regexp gives the exact same results on the ietf mail archive
// and my own's.
// Update, 2008-08-29: some old? Thunderbird versions apparently use a date
// in "Date: " header format, like:   From - Mon, 8 May 2006 10:57:32
// This was added as an alternative format. By the way it also fools "mail" and
// emacs-vm, Recoll is not alone
//
static const  char *frompat =  
#if 0 //1.9.0
    "^From .* [1-2][0-9][0-9][0-9]$";
#endif
#if 1
"^From[ ]+[^ ]+[ ]+"                                  // From whatever
"[[:alpha:]]{3}[ ]+[[:alpha:]]{3}[ ]+[0-3 ][0-9][ ]+" // Date
"[0-2][0-9]:[0-5][0-9](:[0-5][0-9])?[ ]+"             // Time, seconds optional
"([^ ]+[ ]+)?"                                        // Optional tz
"[12][0-9][0-9][0-9]"            // Year, unanchored, more data may follow
"|"      // Or standard mail Date: header format
"^From[ ]+[^ ]+[ ]+"                                  // From toto@tutu
"[[:alpha:]]{3},[ ]+[0-3]?[0-9][ ]+[[:alpha:]]{3}[ ]+" // Date Mon, 8 May
"[12][0-9][0-9][0-9][ ]+"            // Year
"[0-2][0-9]:[0-5][0-9](:[0-5][0-9])?" // Time, secs optional: 10:57(:32)?
    ;
#endif
    //    "([ ]+[-+][0-9]{4})?$"
static regex_t fromregex;
static bool regcompiled;

bool MimeHandlerMbox::next_document()
{
    if (m_vfp == 0) {
	LOGERR(("MimeHandlerMbox::next_document: not open\n"));
	return false;
    }
    if (!m_havedoc) {
	return false;
    }
    FILE *fp = (FILE *)m_vfp;
    int mtarg = 0;
    if (m_ipath != "") {
	sscanf(m_ipath.c_str(), "%d", &mtarg);
    } else if (m_forPreview) {
	// Can't preview an mbox. 
	LOGDEB(("MimeHandlerMbox::next_document: can't preview folders!\n"));
	return false;
    }
    LOGDEB0(("MimeHandlerMbox::next_document: fn %s, msgnum %d mtarg %d \n", 
	    m_fn.c_str(), m_msgnum, mtarg));

    if (!regcompiled) {
	regcomp(&fromregex, frompat, REG_NOSUB|REG_EXTENDED);
	regcompiled = true;
    }

    // If we are called to retrieve a specific message, seek to bof
    // (then scan up to the message). This is for the case where the
    // same object is reused to fetch several messages (else the fp is
    // just opened no need for a seek).  We could also check if the
    // current message number is lower than the requested one and
    // avoid rereading the whole thing in this case. But I'm not sure
    // we're ever used in this way (multiple retrieves on same
    // object).  So:
    if (mtarg > 0) {
	fseek(fp, 0, SEEK_SET);
	m_msgnum = 0;
    }

    off_t start, end;
    bool iseof = false;
    bool hademptyline = true;
    string& msgtxt = m_metaData["content"];
    msgtxt.erase();
    do  {
	// Look for next 'From ' Line, start of message. Set start to
	// line after this
	line_type line;
	for (;;) {
	    if (!fgets(line, LL, fp)) {
		// Eof hit while looking for 'From ' -> file done. We'd need
		// another return code here
		LOGDEB2(("MimeHandlerMbox:next: hit eof while looking for "
			 "start From_ line\n"));
		return false;
	    }
	    m_lineno++;
	    int ll;
	    stripendnl(line, ll);
	    LOGDEB2(("Start: hadempty %d ll %d Line: [%s]\n", 
		    hademptyline, ll, line));
	    if (ll <= 0) {
		hademptyline = true;
		continue;
	    }
	    if (hademptyline && !regexec(&fromregex, line, 0, 0, 0)) {
		LOGDEB0(("MimeHandlerMbox: From_ at line %d: [%s]\n",
			m_lineno, line));
		start = ftello(fp);
		m_msgnum++;
		break;
	    }
	    hademptyline = false;
	}

	// Look for next 'From ' line or eof, end of message.
	for (;;) {
	    end = ftello(fp);
	    if (!fgets(line, LL, fp)) {
		if (ferror(fp) || feof(fp))
		    iseof = true;
		break;
	    }
	    m_lineno++;
	    int ll;
	    stripendnl(line, ll);
	    LOGDEB2(("End: hadempty %d ll %d Line: [%s]\n", 
		    hademptyline, ll, line));
	    if (hademptyline && !regexec(&fromregex, line, 0, 0, 0)) {
		// Rewind to start of "From " line
		fseek(fp, end, SEEK_SET);
		m_lineno--;
		break;
	    }
	    if (mtarg <= 0 || m_msgnum == mtarg) {
		line[ll] = '\n';
		line[ll+1] = 0;
		msgtxt += line;
	    }
	    if (ll <= 0) {
		hademptyline = true;
	    } else {
		hademptyline = false;
	    }
	}

    } while (mtarg > 0 && m_msgnum < mtarg);

    LOGDEB1(("Message text: [%s]\n", msgtxt.c_str()));
    char buf[20];
    sprintf(buf, "%d", m_msgnum);
    m_metaData["ipath"] = buf;
    m_metaData["mimetype"] = "message/rfc822";
    if (iseof) {
	LOGDEB2(("MimeHandlerMbox::next: eof hit\n"));
	m_havedoc = false;
    }
    return msgtxt.empty() ? false : true;
}

#else // Test driver ->

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
using namespace std;

#include "rclinit.h"
#include "mh_mbox.h"

static char *thisprog;

static char usage [] =
"  \n\n"
;
static void
Usage(void)
{
    fprintf(stderr, "%s: usage:\n%s", thisprog, usage);
    exit(1);
}

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
      default: Usage();	break;
      }
    argc--; argv++;
  }

  if (argc != 1)
    Usage();
  string filename = *argv++;argc--;
  string reason;
  RclConfig *conf = recollinit(RclInitFlags(0), 0, 0, reason, 0);
  if (conf == 0) {
      cerr << "init failed " << reason << endl;
      exit(1);
  }
  MimeHandlerMbox mh("text/x-mail");
  if (!mh.set_document_file(filename)) {
      cerr << "set_document_file failed" << endl;
      exit(1);
  }
  int docnt = 0;
  while (mh.has_documents()) {
      if (!mh.next_document()) {
	  cerr << "next_document failed" << endl;
	  exit(1);
      }
      docnt++;
  }
  cout << docnt << " documents found in " << filename << endl;
  exit(0);
}


#endif // TEST_MH_MBOX
