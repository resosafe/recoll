/* Copyright (C) 2004 J.F.Dockes
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
#ifndef _PATHUT_H_INCLUDED_
#define _PATHUT_H_INCLUDED_
#include <unistd.h>

#include <string>
#include <vector>
#include "refcntr.h"

#ifndef NO_NAMESPACES
using std::string;
using std::vector;
#endif

/// Add a / at the end if none there yet.
extern void path_catslash(string &s);
/// Concatenate 2 paths
extern string path_cat(const string &s1, const string &s2);
/// Get the simple file name (get rid of any directory path prefix
extern string path_getsimple(const string &s);
/// Simple file name + optional suffix stripping
extern string path_basename(const string &s, const string &suff=string());
/// Get the father directory
extern string path_getfather(const string &s);
/// Get the current user's home directory
extern string path_home();
/// Expand ~ at the beginning of string 
extern string path_tildexpand(const string &s);
/// Use getcwd() to make absolute path if needed. Beware: ***this can fail***
/// we return an empty path in this case.
extern string path_absolute(const string &s);
/// Clean up path by removing duplicated / and resolving ../ + make it absolute
extern string path_canon(const string &s);
/// Use glob(3) to return the file names matching pattern inside dir
extern vector<string> path_dirglob(const string &dir, 
				   const string pattern);
/// Encode according to rfc 1738
extern string url_encode(const string& url, 
			      string::size_type offs = 0);
/// Transcode to utf-8 if possible or url encoding, for display.
extern bool printableUrl(const string &fcharset, 
			 const string &in, string &out);
//// Convert to file path if url is like file://. This modifies the
//// input (and returns a copy for convenience)
extern string fileurltolocalpath(string url);
/// Test for file:/// url
extern bool urlisfileurl(const string& url);

/// Return the host+path part of an url. This is not a general
/// routine, it does the right thing only in the recoll context
extern string url_gpath(const string& url);

/// Stat parameter and check if it's a directory
extern bool path_isdir(const string& path);

/** A small wrapper around statfs et al, to return percentage of disk
    occupation */
bool fsocc(const string &path, int *pc, // Percent occupied
	   long *avmbs = 0 // Mbs available to non-superuser
	   );

/// Create temporary directory
extern bool maketmpdir(string& tdir, string& reason);

/// mkdir -p
extern bool makepath(const string& path);

/// Temporary file class
class TempFileInternal {
public:
    TempFileInternal(const string& suffix);
    ~TempFileInternal();
    const char *filename() {return m_filename.c_str();}
    const string &getreason() {return m_reason;}
    bool ok() {return !m_filename.empty();}
private:
    string m_filename;
    string m_reason;
};

typedef RefCntr<TempFileInternal> TempFile;

/// Temporary directory class
class TempDir {
public:
    TempDir();
    ~TempDir();
    const char *dirname() {return m_dirname.c_str();}
    const string &getreason() {return m_reason;}
    bool ok() {return !m_dirname.empty();}
    bool wipe();
private:
    string m_dirname;
    string m_reason;
};

/// Lock/pid file class. This is quite close to the pidfile_xxx
/// utilities in FreeBSD with a bit more encapsulation. I'd have used
/// the freebsd code if it was available elsewhere
class Pidfile {
public:
    Pidfile(const string& path)	: m_path(path), m_fd(-1) {}
    ~Pidfile();
    /// Open/create the pid file.
    /// @return 0 if ok, > 0 for pid of existing process, -1 for other error.
    pid_t open();
    /// Write pid into the pid file
    /// @return 0 ok, -1 error
    int write_pid();
    /// Close the pid file (unlocks)
    int close();
    /// Delete the pid file
    int remove();
    const string& getreason() {return m_reason;}
private:
    string m_path;
    int    m_fd;
    string m_reason;
    pid_t read_pid();
    int flopen();
};



// Freedesktop thumbnail standard path routine
// On return, path will have the appropriate value in all cases,
// returns true if the file already exists
extern bool thumbPathForUrl(const string& url, int size, string& path);

// Must be called in main thread before starting other threads
extern void pathut_init_mt();

#endif /* _PATHUT_H_INCLUDED_ */
