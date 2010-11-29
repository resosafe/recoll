#ifndef lint
static char rcsid[] = "@(#$Id: mh_mbox.cpp,v 1.5 2008-10-04 14:26:59 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include "autoconfig.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>
#include <sys/stat.h>
#include <errno.h>

#include <cstring>
#include <map>

#include "mimehandler.h"
#include "debuglog.h"
#include "readfile.h"
#include "mh_mbox.h"
#include "smallut.h"
#include "rclconfig.h"
#include "md5.h"
#include "conftree.h"

using namespace std;
class FpKeeper { 
public:
    FpKeeper(FILE **fpp) : m_fpp(fpp) {}
    ~FpKeeper() 
    {
        if (m_fpp && *m_fpp) {
            fclose(*m_fpp);
            *m_fpp = 0;
        }
    }
private: FILE **m_fpp;
};


/**
 * Handles a cache for message numbers to offset translations. Permits direct
 * accesses inside big folders instead of having to scan up to the right place
 *
 * Message offsets are saved to files stored under cfg(mboxcachedir), default
 * confdir/mboxcache. Mbox files smaller than cfg(mboxcacheminmbs) are not
 * cached.
 * Cache files are named as the md5 of the file UDI, which is kept in
 * the first block for possible collision detection. The 64 bits
 * offsets for all message "From_" lines follow. The format is purely
 * binary, values are not even byte-swapped to be proc-idependant.
 */
class MboxCache {
public:
    typedef MimeHandlerMbox::mbhoff_type mbhoff_type;
    MboxCache()
        : m_ok(false), m_minfsize(0)
    { 
        // Can't access rclconfig here, we're a static object, would
        // have to make sure it's initialized.
    }

    ~MboxCache() {}
    mbhoff_type get_offset(const string& udi, int msgnum)
    {
        LOGDEB0(("MboxCache::get_offsets: udi [%s] msgnum %d\n", udi.c_str(),
                 msgnum));
        if (!ok()) {
            LOGDEB0(("MboxCache::get_offsets: init failed\n"));
            return -1;
        }
        string fn = makefilename(udi);
        FILE *fp = 0;
        if ((fp = fopen(fn.c_str(), "r")) == 0) {
            LOGDEB(("MboxCache::get_offsets: open failed, errno %d\n", errno));
            return -1;
        }
        FpKeeper keeper(&fp);

        char blk1[o_b1size];
        if (fread(blk1, 1, o_b1size, fp) != o_b1size) {
            LOGDEB0(("MboxCache::get_offsets: read blk1 errno %d\n", errno));
            return -1;
        }
        ConfSimple cf(string(blk1, o_b1size));
        string fudi;
        if (!cf.get("udi", fudi) || fudi.compare(udi)) {
            LOGINFO(("MboxCache::get_offset:badudi fn %s udi [%s], fudi [%s]\n",
                     fn.c_str(), udi.c_str(), fudi.c_str()));
            return -1;
        }
        if (fseeko(fp, cacheoffset(msgnum), SEEK_SET) != 0) {
            LOGDEB0(("MboxCache::get_offsets: seek %ld errno %d\n",
                     (long)cacheoffset(msgnum), errno));
            return -1;
        }
        mbhoff_type offset = -1;
        int ret;
        if ((ret = fread(&offset, 1, sizeof(mbhoff_type), fp))
            != sizeof(mbhoff_type)) {
            LOGDEB0(("MboxCache::get_offsets: read ret %d errno %d\n", 
                     ret, errno));
            return -1;
        }
        LOGDEB0(("MboxCache::get_offsets: ret %lld\n", (long long)offset));
        return offset;
    }

    // Save array of offsets for a given file, designated by Udi
    void put_offsets(const string& udi, mbhoff_type fsize,
                     vector<mbhoff_type>& offs)
    {
        LOGDEB0(("MboxCache::put_offsets: %u offsets\n", offs.size()));
        if (!ok() || !maybemakedir())
            return;
        if (fsize < m_minfsize)
            return;
        string fn = makefilename(udi);
        FILE *fp;
        if ((fp = fopen(fn.c_str(), "w")) == 0) {
            LOGDEB(("MboxCache::put_offsets: fopen errno %d\n", errno));
            return;
        }
        FpKeeper keeper(&fp);
        string blk1;
        blk1.append("udi=");
        blk1.append(udi);
        blk1.append("\n");
        blk1.resize(o_b1size, 0);
        if (fwrite(blk1.c_str(), 1, o_b1size, fp) != o_b1size) {
            LOGDEB(("MboxCache::put_offsets: fwrite errno %d\n", errno));
            return;
        }

        for (vector<mbhoff_type>::const_iterator it = offs.begin();
             it != offs.end(); it++) {
            mbhoff_type off = *it;
            if (fwrite((char*)&off, 1, sizeof(mbhoff_type), fp) != 
                sizeof(mbhoff_type)) {
                return;
            }
        }
    }

    // Check state, possibly initialize
    bool ok() {
        if (m_minfsize == -1)
            return false;
        if (!m_ok) {
            RclConfig *config = RclConfig::getMainConfig();
            if (config == 0)
                return false;
            int minmbs = 5;
            config->getConfParam("mboxcacheminmbs", &minmbs);
            if (minmbs < 0) {
                // minmbs set to negative to disable cache
                m_minfsize = -1;
                return false;
            }
            m_minfsize = minmbs * 1000 * 1000;

            config->getConfParam("mboxcachedir", m_dir);
            if (m_dir.empty())
                m_dir = "mboxcache";
            m_dir = path_tildexpand(m_dir);
            // If not an absolute path, compute relative to config dir
            if (m_dir.at(0) != '/')
                m_dir = path_cat(config->getConfDir(), m_dir);
            m_ok = true;
        }
        return m_ok;
    }

private:
    bool m_ok;

    // Place where we store things
    string m_dir;
    // Don't cache smaller files. If -1, don't do anything.
    mbhoff_type m_minfsize;
    static const size_t o_b1size;

    // Create the cache directory if it does not exist
    bool maybemakedir()
    {
        struct stat st;
        if (stat(m_dir.c_str(), &st) != 0 && mkdir(m_dir.c_str(), 0700) != 0) {
            return false;
        }
        return true;
    }
    // Compute file name from udi
    string makefilename(const string& udi)
    {
        string digest, xdigest;
        MD5String(udi, digest);
        MD5HexPrint(digest, xdigest);
        return path_cat(m_dir, xdigest);
    }

    // Compute offset in cache file for the mbox offset of msgnum
    mbhoff_type cacheoffset(int msgnum)
    {// Msgnums are from 1
        return o_b1size + (msgnum-1) * sizeof(mbhoff_type);
    }
};

const size_t MboxCache::o_b1size = 1024;
static class MboxCache mcache;
static const string keyquirks("mhmboxquirks");

MimeHandlerMbox::~MimeHandlerMbox()
{
    clear();
}

void MimeHandlerMbox::clear()
{
    m_fn.erase();
    if (m_vfp) {
	fclose((FILE *)m_vfp);
	m_vfp = 0;
    }
    m_msgnum =  m_lineno = 0;
    m_ipath.erase();
    m_offsets.clear();
    RecollFilter::clear();
}

bool MimeHandlerMbox::set_document_file(const string &fn)
{
    LOGDEB(("MimeHandlerMbox::set_document_file(%s)\n", fn.c_str()));
    RecollFilter::set_document_file(fn);
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
    fseek((FILE *)m_vfp, 0, SEEK_END);
    m_fsize = ftell((FILE*)m_vfp);
    fseek((FILE*)m_vfp, 0, SEEK_SET);
    m_havedoc = true;
    m_offsets.clear();
    m_quirks = 0;

    // Check for location-based quirks:
    RclConfig *config = RclConfig::getMainConfig();
    string quirks;
    if (config && config->getConfParam(keyquirks, quirks)) {
	if (quirks == "tbird") {
	    LOGDEB(("MimeHandlerMbox: setting quirks TBIRD\n"));
	    m_quirks |= MBOXQUIRK_TBIRD;
	}
    }

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
// Update: 2009-11-27: word after From may be quoted string: From "john bull"
static const  char *frompat =  
"^From[ ]+([^ ]+|\"[^\"]+\")[ ]+"    // 'From (toto@tutu|"john bull") '
"[[:alpha:]]{3}[ ]+[[:alpha:]]{3}[ ]+[0-3 ][0-9][ ]+" // Fri Oct 26
"[0-2][0-9]:[0-5][0-9](:[0-5][0-9])?[ ]+"             // Time, seconds optional
"([^ ]+[ ]+)?"                                        // Optional tz
"[12][0-9][0-9][0-9]"            // Year, unanchored, more data may follow
"|"      // Or standard mail Date: header format
"^From[ ]+[^ ]+[ ]+"                                   // From toto@tutu
"[[:alpha:]]{3},[ ]+[0-3]?[0-9][ ]+[[:alpha:]]{3}[ ]+" // Mon, 8 May
"[12][0-9][0-9][0-9][ ]+"                              // Year
"[0-2][0-9]:[0-5][0-9](:[0-5][0-9])?"                  // Time, secs optional
    ;

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
        mbhoff_type off;
        line_type line;
        LOGDEB0(("MimeHandlerMbox::next_doc: mtarg %d m_udi[%s]\n",
                mtarg, m_udi.c_str()));
        if (!m_udi.empty() && 
            (off = mcache.get_offset(m_udi, mtarg)) >= 0 && 
            fseeko(fp, (off_t)off, SEEK_SET) >= 0 && 
            fgets(line, LL, fp) &&
            !regexec(&fromregex, line, 0, 0, 0)) {
                LOGDEB0(("MimeHandlerMbox: Cache: From_ Ok\n"));
                fseeko(fp, (off_t)off, SEEK_SET);
                m_msgnum = mtarg -1;
        } else {
            fseek(fp, 0, SEEK_SET);
            m_msgnum = 0;
        }
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
            mbhoff_type off_From = ftello(fp);
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
	    // Non empty line. If the previous one was empty, check regex
	    if (hademptyline) {
		// Tbird sometimes omits the empty line, so avoid resetting
		// state (initially true) and hope for the best
		if (!(m_quirks & MBOXQUIRK_TBIRD))
		    hademptyline = false;
		if (!regexec(&fromregex, line, 0, 0, 0)) {
		    LOGDEB0(("MimeHandlerMbox: msgnum %d, "
			 "From_ at line %d: [%s]\n", m_msgnum, m_lineno, line));
		    start = ftello(fp);
		    m_offsets.push_back(off_From);
		    m_msgnum++;
		    break;
		}
	    }
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
	    if (hademptyline) {
		if (ll > 0) {
		    if (!(m_quirks & MBOXQUIRK_TBIRD))
			hademptyline = false;
		    if (!regexec(&fromregex, line, 0, 0, 0)) {
			// Rewind to start of "From " line
			fseek(fp, end, SEEK_SET);
			m_lineno--;
                        hademptyline = true;
			break;
		    }
		}
	    } else if (ll <= 0) {
		hademptyline = true;
	    }
	    if (mtarg <= 0 || m_msgnum == mtarg) {
		line[ll] = '\n';
		line[ll+1] = 0;
		msgtxt += line;
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
	if (!m_udi.empty()) {
	  mcache.put_offsets(m_udi, m_fsize, m_offsets);
	}
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

#include "rclconfig.h"
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
static RclConfig *config;
RclConfig *RclConfig::getMainConfig()
{
    return config;
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
  config = recollinit(RclInitFlags(0), 0, 0, reason, 0);
  if (config == 0) {
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
      map<string, string>::const_iterator it = 
	mh.get_meta_data().find("content");
      int size;
      if (it == mh.get_meta_data().end()) {
	size = -1;
      } else {
	size = it->second.length();
      }
      cout << "Doc " << docnt << " size " << size  << endl;
      docnt++;
  }
  cout << docnt << " documents found in " << filename << endl;
  exit(0);
}


#endif // TEST_MH_MBOX
