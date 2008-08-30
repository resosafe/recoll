#ifndef lint
static char rcsid[] = "@(#$Id: smallut.cpp,v 1.29.2.1 2008-08-30 07:31:16 dockes Exp $ (C) 2004 J.F.Dockes";
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
#ifndef TEST_SMALLUT
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "smallut.h"
#include "utf8iter.h"

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#define MIN(A,B) ((A)<(B)?(A):(B))

int stringicmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c1, c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c1 = ::toupper(*it1);
	    c2 = ::toupper(*it2);
	    if (c1 != c2) {
		return c1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c1 = ::toupper(*it1);
	    c2 = ::toupper(*it2);
	    if (c1 != c2) {
		return c1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}
void stringtolower(string& io)
{
    string::iterator it = io.begin();
    string::iterator ite = io.end();
    while (it != ite) {
	*it = ::tolower(*it);
	it++;
    }
}

extern int stringisuffcmp(const string& s1, const string& s2)
{
    string::const_reverse_iterator r1 = s1.rbegin(), re1 = s1.rend(),
	r2 = s2.rbegin(), re2 = s2.rend();
    while (r1 != re1 && r2 != re2) {
	char c1 = ::toupper(*r1);
	char c2 = ::toupper(*r2);
	if (c1 != c2) {
	    return c1 > c2 ? 1 : -1;
	}
	++r1; ++r2;
    }
    return 0;
}

//  s1 is already lowercase
int stringlowercmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c2 = ::tolower(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c2 = ::tolower(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}

//  s1 is already uppercase
int stringuppercmp(const string & s1, const string& s2) 
{
    string::const_iterator it1 = s1.begin();
    string::const_iterator it2 = s2.begin();
    int size1 = s1.length(), size2 = s2.length();
    char c2;

    if (size1 > size2) {
	while (it1 != s1.end()) { 
	    c2 = ::toupper(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : 1;
    } else {
	while (it2 != s2.end()) { 
	    c2 = ::toupper(*it2);
	    if (*it1 != c2) {
		return *it1 > c2 ? 1 : -1;
	    }
	    ++it1; ++it2;
	}
	return size1 == size2 ? 0 : -1;
    }
}

// Compare charset names, removing the more common spelling variations
bool samecharset(const string &cs1, const string &cs2)
{
    string mcs1, mcs2;
    // Remove all - and _, turn to lowecase
    for (unsigned int i = 0; i < cs1.length();i++) {
	if (cs1[i] != '_' && cs1[i] != '-') {
	    mcs1 += ::tolower(cs1[i]);
	}
    }
    for (unsigned int i = 0; i < cs2.length();i++) {
	if (cs2[i] != '_' && cs2[i] != '-') {
	    mcs2 += ::tolower(cs2[i]);
	}
    }
    return mcs1 == mcs2;
}

template <class T> bool stringToStrings(const string &s, T &tokens)
{
    string current;
    tokens.clear();
    enum states {SPACE, TOKEN, INQUOTE, ESCAPE};
    states state = SPACE;
    for (unsigned int i = 0; i < s.length(); i++) {
	switch (s[i]) {
	    case '"': 
	    switch(state) {
	      case SPACE: 
		state=INQUOTE; continue;
	      case TOKEN: 
	        current += '"';
		continue;
	      case INQUOTE: 
		tokens.push_back(current);
		current.clear();
		state = SPACE;
		continue;
	      case ESCAPE:
	        current += '"';
	        state = INQUOTE;
	      continue;
	    }
	    break;
	    case '\\': 
	    switch(state) {
	      case SPACE: 
	      case TOKEN: 
		  current += '\\';
		  state=TOKEN; 
		  continue;
	      case INQUOTE: 
		  state = ESCAPE;
		  continue;
	      case ESCAPE:
		  current += '\\';
		  state = INQUOTE;
		  continue;
	    }
	    break;

	    case ' ': 
	    case '\t': 
	    case '\n': 
	    case '\r': 
	    switch(state) {
	      case SPACE: 
		  continue;
	      case TOKEN: 
		tokens.push_back(current);
		current.clear();
		state = SPACE;
		continue;
	      case INQUOTE: 
	      case ESCAPE:
		  current += s[i];
		  continue;
	    }
	    break;

	    default:
	    switch(state) {
	      case ESCAPE:
		  state = INQUOTE;
		  break;
	      case SPACE: 
		  state = TOKEN;
		  break;
	      case TOKEN: 
	      case INQUOTE: 
		  break;
	    }
	    current += s[i];
	}
    }
    switch(state) {
    case SPACE: 
	break;
    case TOKEN: 
	tokens.push_back(current);
	break;
    case INQUOTE: 
    case ESCAPE:
	return false;
    }
    return true;
}
bool stringToStrings(const string &s, list<string> &tokens)
{
    return stringToStrings<list<string> >(s, tokens);
}
bool stringToStrings(const string &s, vector<string> &tokens)
{
    return stringToStrings<vector<string> >(s, tokens);
}

template <class T> void stringsToString(const T &tokens, string &s) 
{
    for (typename T::const_iterator it = tokens.begin();
	 it != tokens.end(); it++) {
	bool hasblanks = false;
	if (it->find_first_of(" \t\n") != string::npos)
	    hasblanks = true;
	if (it != tokens.begin())
	    s.append(1, ' ');
	if (hasblanks)
	    s.append(1, '"');
	for (unsigned int i = 0; i < it->length(); i++) {
	    char car = it->at(i);
	    if (car == '"') {
		s.append(1, '\\');
		s.append(1, car);
	    } else {
		s.append(1, car);
	    }
	}
	if (hasblanks)
	    s.append(1, '"');
    }
}
void stringsToString(const list<string> &tokens, string &s)
{
    stringsToString<list<string> >(tokens, s);
}
void stringsToString(const vector<string> &tokens, string &s)
{
    stringsToString<vector<string> >(tokens, s);
}

void stringToTokens(const string& str, list<string>& tokens,
		    const string& delims, bool skipinit)
{
    string::size_type startPos = 0, pos;

    for (pos = 0;;) { 
        // Skip initial delims, break if this eats all.
        if (skipinit && 
	    (startPos = str.find_first_not_of(delims, pos)) == string::npos)
	    break;
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);
        // Add token to the vector. Note: token cant be empty here
	if (pos == string::npos)
	    tokens.push_back(str.substr(startPos));
	else
	    tokens.push_back(str.substr(startPos, pos - startPos));
    }
}

bool stringToBool(const string &s)
{
    if (s.empty())
	return false;
    if (isdigit(s[0])) {
	int val = atoi(s.c_str());
	return val ? true : false;
    }
    if (s.find_first_of("yYtT") == 0)
	return true;
    return false;
}

void trimstring(string &s, const char *ws)
{
    string::size_type pos = s.find_first_not_of(ws);
    if (pos == string::npos) {
	s.clear();
	return;
    }
    s.replace(0, pos, string());

    pos = s.find_last_not_of(ws);
    if (pos != string::npos && pos != s.length()-1)
	s.replace(pos+1, string::npos, string());
}

// Remove some chars and replace them with spaces
string neutchars(const string &str, string delims)
{
    string out;
    string::size_type startPos, pos;

    for (pos = 0;;) { 
        // Skip initial delims, break if this eats all.
        if ((startPos = str.find_first_not_of(delims, pos)) == string::npos)
	    break;
        // Find next delimiter or end of string (end of token)
        pos = str.find_first_of(delims, startPos);
        // Add token to the output. Note: token cant be empty here
	if (pos == string::npos) {
	    out += str.substr(startPos);
	} else {
	    out += str.substr(startPos, pos - startPos) + " ";
	}
    }
    return out;
}


/* Truncate a string to a given maxlength, avoiding cutting off midword
 * if reasonably possible. Note: we could also use textsplit, stopping when
 * we have enough, this would be cleanly utf8-aware but would remove 
 * punctuation */
static const string SEPAR = " \t\n\r-:.;,/[]{}";
string truncate_to_word(const string &input, string::size_type maxlen)
{
    string output;
    if (input.length() <= maxlen) {
	output = input;
    } else {
	output = input.substr(0, maxlen);
	string::size_type space = output.find_last_of(SEPAR);
	// Original version only truncated at space if space was found after
	// maxlen/2. But we HAVE to truncate at space, else we'd need to do
	// utf8 stuff to avoid truncating at multibyte char. In any case,
	// not finding space means that the text probably has no value.
	// Except probably for Asian languages, so we may want to fix this 
	// one day
	if (space == string::npos) {
	    output.erase();
	} else {
	    output.erase(space);
	}
	output += " ...";
    }
    return output;
}

void utf8truncate(string &s, int maxlen)
{
    Utf8Iter iter(s);
    int pos = 0;
    while (iter++ != string::npos) 
	if (iter.getBpos() < string::size_type(maxlen))
	    pos = iter.getBpos();

    s.erase(pos);
}

// Escape things that would look like markup
string escapeHtml(const string &in)
{
    string out;
    for (string::size_type pos = 0; pos < in.length(); pos++) {
	switch(in.at(pos)) {
	case '<':
	    out += "&lt;";
	    break;
	case '&':
	    out += "&amp;";
	    break;
	default:
	    out += in.at(pos);
	}
    }
    return out;
}

string escapeShell(const string &in)
{
    string out;
    out += "\"";
    for (string::size_type pos = 0; pos < in.length(); pos++) {
	switch(in.at(pos)) {
	case '$':
	    out += "\\$";
	    break;
	case '`':
	    out += "\\`";
	    break;
	case '"':
	    out += "\\\"";
	    break;
	case '\n':
	    out += "\\\n";
	    break;
	case '\\':
	    out += "\\\\";
	    break;
	default:
	    out += in.at(pos);
	}
    }
    out += "\"";
    return out;
}


// Small utility to substitute printf-like percents cmds in a string
bool pcSubst(const string& in, string& out, map<char, string>& subs)
{
    string::const_iterator it;
    for (it = in.begin(); it != in.end();it++) {
	if (*it == '%') {
	    if (++it == in.end()) {
		out += '%';
		break;
	    }
	    if (*it == '%') {
		out += '%';
		continue;
	    }
	    map<char,string>::iterator tr;
	    if ((tr = subs.find(*it)) != subs.end()) {
		out += tr->second;
	    } else {
		out += *it;
	    }
	} else {
	    out += *it;
	}
    }
    return true;
}

////////////////////
// Internal redefinition of system time interface to help with dependancies
struct m_timespec {
  time_t tv_sec;
  long   tv_nsec;
};

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME 1
#endif

#define MILLIS(TV) ( (long)(((TV).tv_sec - m_secs) * 1000 + \
  ((TV).tv_nsec - m_nsecs) / 1000000))

#define MICROS(TV) ( (long)(((TV).tv_sec - m_secs) * 1000000 + \
  ((TV).tv_nsec - m_nsecs) / 1000))


// We use gettimeofday instead of clock_gettime for now and get only
// uS resolution, because clock_gettime is more configuration trouble
// than it's worth
static void gettime(int, struct m_timespec *ts)
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  ts->tv_sec = tv.tv_sec;
  ts->tv_nsec = tv.tv_usec * 1000;
}
///// End system interface

static m_timespec frozen_tv;
void Chrono::refnow()
{
  gettime(CLOCK_REALTIME, &frozen_tv);
}

Chrono::Chrono()
{
  restart();
}

// Reset and return value before rest in milliseconds
long Chrono::restart()
{
  struct m_timespec tv;
  gettime(CLOCK_REALTIME, &tv);
  long ret = MILLIS(tv);
  m_secs = tv.tv_sec;
  m_nsecs = tv.tv_nsec;
  return ret;
}

// Get current timer value, milliseconds
long Chrono::millis(int frozen)
{
  if (frozen) {
    return MILLIS(frozen_tv);
  } else {
    struct m_timespec tv;
    gettime(CLOCK_REALTIME, &tv);
    return MILLIS(tv);
  }
}

//
long Chrono::micros(int frozen)
{
  if (frozen) {
    return MICROS(frozen_tv);
  } else {
    struct m_timespec tv;
    gettime(CLOCK_REALTIME, &tv);
    return MICROS(tv);
  }
}

float Chrono::secs(int frozen)
{
  struct m_timespec tv;
  gettime(CLOCK_REALTIME, &tv);
  float secs = (float)(frozen?frozen_tv.tv_sec:tv.tv_sec - m_secs);
  float nsecs = (float)(frozen?frozen_tv.tv_nsec:tv.tv_nsec - m_nsecs); 
  //fprintf(stderr, "secs %.2f nsecs %.2f\n", secs, nsecs);
  return secs + nsecs * 1e-9;
}

#else

#include <string>
using namespace std;
#include <iostream>

#include "smallut.h"

struct spair {
    const char *s1;
    const char *s2;
};
struct spair pairs[] = {
    {"", ""},
    {"", "a"},
    {"a", ""},
    {"a", "a"},
    {"A", "a"},
    {"a", "A"},
    {"A", "A"},
    {"12", "12"},
    {"a", "ab"},
    {"ab", "a"},
    {"A", "Ab"},
    {"a", "Ab"},
};
int npairs = sizeof(pairs) / sizeof(struct spair);
struct spair suffpairs[] = {
    {"", ""},
    {"", "a"},
    {"a", ""},
    {"a", "a"},
    {"toto.txt", ".txt"},
    {"TXT", "toto.txt"},
    {"toto.txt", ".txt1"},
    {"toto.txt1", ".txt"},
};
int nsuffpairs = sizeof(suffpairs) / sizeof(struct spair);


int main(int argc, char **argv)
{
#if 0
    for (int i = 0; i < npairs; i++) {
	{
	    int c = stringicmp(pairs[i].s1, pairs[i].s2);
	    printf("'%s' %s '%s' ", pairs[i].s1, 
		   c == 0 ? "==" : c < 0 ? "<" : ">", pairs[i].s2);
	}
	{
	    int cl = stringlowercmp(pairs[i].s1, pairs[i].s2);
	    printf("L '%s' %s '%s' ", pairs[i].s1, 
		   cl == 0 ? "==" : cl < 0 ? "<" : ">", pairs[i].s2);
	}
	{
	    int cu = stringuppercmp(pairs[i].s1, pairs[i].s2);
	    printf("U '%s' %s '%s' ", pairs[i].s1, 
		   cu == 0 ? "==" : cu < 0 ? "<" : ">", pairs[i].s2);
	}
	printf("\n");
    }
#elif 0
    for (int i = 0; i < nsuffpairs; i++) {
	int c = stringisuffcmp(suffpairs[i].s1, suffpairs[i].s2);
	printf("[%s] %s [%s] \n", suffpairs[i].s1, 
	       c == 0 ? "matches" : c < 0 ? "<" : ">", suffpairs[i].s2);
    }
#elif 1
    std::string testit("\303\251l\303\251gant");
    for (int sz = 10; sz >= 0; sz--) {
	utf8truncate(testit, sz);
	cout << testit << endl;
    }
#endif

}

#endif
