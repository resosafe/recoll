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
#ifndef _SMALLUT_H_INCLUDED_
#define _SMALLUT_H_INCLUDED_
/* @(#$Id: smallut.h,v 1.19.2.1 2006-12-07 07:06:05 dockes Exp $  (C) 2004 J.F.Dockes */
#include <string>
#include <list>
#include <map>

#ifndef NO_NAMESPACES
using std::string;
using std::list;
using std::map;
#endif /* NO_NAMESPACES */

extern int stringicmp(const string& s1, const string& s2);
extern int stringlowercmp(const string& alreadylower, const string& s2);
extern int stringuppercmp(const string& alreadyupper, const string& s2); 

// Compare charset names, removing the more common spelling variations
extern bool samecharset(const string &cs1, const string &cs2);

extern bool maketmpdir(string& tdir);
extern string stringlistdisp(const list<string>& strs);

/**
 * Parse input string into list of strings. 
 *
 * Token delimiter is " \t\n" except inside dquotes. dquote inside
 * dquotes can be escaped with \ etc...
 */
extern bool stringToStrings(const string &s, list<string> &tokens);

/**
 * Split input string. No handling of quoting
 */
extern void stringToTokens(const string &s, list<string> &tokens, 
			   const string &delims = " \t");

/** Convert string to boolean */
extern bool stringToBool(const string &s);

/** Remove instances of characters belonging to set (default {space,
    tab}) at beginning and end of input string */
extern void trimstring(string &s, const char *ws = " \t");

/** Escape things like < or & by turining them to entities */
extern string escapeHtml(const string &in);

/** Replace some chars with spaces (ie: newline chars). This is not utf8-aware
 *  so chars should only contain ascii */
extern string neutchars(const string &str, string chars);

/** turn string into something that won't be expanded by a shell. In practise
 * quote with single-quotes and escape internal singlequotes */
extern string escapeShell(const string &str);

/** Truncate a string to a given maxlength, avoiding cutting off midword
 *  if reasonably possible. */
extern string truncate_to_word(string &input, string::size_type maxlen);

/** Small utility to substitute printf-like percents cmds in a string */
bool pcSubst(const string& in, string& out, map<char, string>& subs);

class Chrono {
 public:
  Chrono();
  /** Reset origin */
  long restart();
  /** Snapshot current time */
  static void refnow();
  /** Get current elapsed since creation or restart
   *
   *  @param frozen give time since the last refnow call (this is to
   * allow for using one actual system call to get values from many
   * chrono objects, like when examining timeouts in a queue)
   */
  long millis(int frozen = 0);
  long ms() {return millis();}
  long micros(int frozen = 0);
  float secs(int frozen = 0);
 private:
  long	m_secs;
  long 	m_nsecs; 
};


#endif /* _SMALLUT_H_INCLUDED_ */
