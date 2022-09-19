/* Copyright (C) 2016-2019 J.F.Dockes
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

#include "autoconfig.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "safefcntl.h"
#include "safeunistd.h"
#include "cstr.h"
#ifdef _WIN32
#include "safewindows.h"
#include <Shlobj.h>
#else
#include <sys/param.h>
#include <pwd.h>
#include <sys/file.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <math.h>
#include <errno.h>
#include <sys/types.h>
#include "safesysstat.h"

#include <mutex>
#include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <numeric>

#include "rclutil.h"
#include "pathut.h"
#include "wipedir.h"
#include "transcode.h"
#include "md5ut.h"
#include "log.h"
#include "smallut.h"
#include "rclconfig.h"

using namespace std;

template <class T> void map_ss_cp_noshr(T s, T *d)
{
    for (const auto& ent : s) {
        d->insert(
            pair<string, string>(string(ent.first.begin(), ent.first.end()),
                                 string(ent.second.begin(), ent.second.end())));
    }
}
template void map_ss_cp_noshr<map<string, string> >(
    map<string, string> s, map<string, string>*d);
template void map_ss_cp_noshr<unordered_map<string, string> >(
    unordered_map<string,string> s, unordered_map<string,string>*d);

// Add data to metadata field, store multiple values as CSV, avoid
// appending multiple identical instances.
template <class T> void addmeta(
    T& store, const string& nm, const string& value)
{
    auto it = store.find(nm);
    if (it == store.end() || it->second.empty()) {
        store[nm] = value;
    } else if (it->second.find(value) == string::npos) {
        store[nm] += ',';
        store[nm] += value;
    }
}
template void addmeta<map<string, string>>(
    map<string, string>&, const string&, const string&);
template void addmeta<unordered_map<string, string>>(
    unordered_map<string, string>&, const string&, const string&);

#ifdef _WIN32
static bool path_hasdrive(const string& s)
{
    if (s.size() >= 2 && isalpha(s[0]) && s[1] == ':') {
        return true;
    }
    return false;
}
static bool path_isdriveabs(const string& s)
{
    if (s.size() >= 3 && isalpha(s[0]) && s[1] == ':' && s[2] == '/') {
        return true;
    }
    return false;
}

#include <Shlwapi.h>
#pragma comment(lib, "shlwapi.lib")

string path_thisexecpath()
{
    wchar_t text[MAX_PATH];
    GetModuleFileNameW(NULL, text, MAX_PATH);
#ifdef NTDDI_WIN8_future
    PathCchRemoveFileSpec(text, MAX_PATH);
#else
    PathRemoveFileSpecW(text);
#endif
    string path;
    wchartoutf8(text, path);
    if (path.empty()) {
        path = "c:/";
    }

    return path;
}

// On Windows, we use a subdirectory named "rcltmp" inside the windows
// temp location to create the temporary files in.
static const string& path_wingetrcltmpdir()
{
    // Constant: only need to compute once
    static string tdir;
    if (tdir.empty()) {
        wchar_t dbuf[MAX_PATH + 1];
        GetTempPathW(MAX_PATH, dbuf);
        if (!wchartoutf8(dbuf, tdir)) {
            LOGERR("path_wingetrcltmpdir: wchartoutf8 failed. Using c:/Temp\n");
            tdir = "C:/Temp";
        }
        LOGDEB1("path_wingetrcltmpdir(): gettemppathw ret: " << tdir << "\n");
        tdir = path_cat(tdir, "rcltmp");
        if (!path_exists(tdir)) {
            if (!path_makepath(tdir, 0700)) {
                LOGSYSERR("path_wingettempfilename", "path_makepath", tdir);
            }
        }
    }
    return tdir;
}

static bool path_gettempfilename(string& filename, string&)
{
    string tdir = tmplocation();
    LOGDEB0("path_gettempfilename: tdir: [" << tdir << "]\n");
    wchar_t dbuf[MAX_PATH + 1];
    utf8towchar(tdir, dbuf, MAX_PATH);

    wchar_t buf[MAX_PATH + 1];
    static wchar_t prefix[]{L"rcl"};
    GetTempFileNameW(dbuf, prefix, 0, buf);
    wchartoutf8(buf, filename);

    // Windows will have created a temp file, we delete it.
    if (!DeleteFileW(buf)) {
        LOGSYSERR("path_wingettempfilename", "DeleteFileW", filename);
    } else {
        LOGDEB1("path_wingettempfilename: DeleteFile " << filename << " Ok\n");
    }
    path_slashize(filename);
    LOGDEB1("path_gettempfilename: filename: [" << filename << "]\n");
    return true;
}

#else // _WIN32 above

static bool path_gettempfilename(string& filename, string& reason)
{
    filename = path_cat(tmplocation(), "rcltmpfXXXXXX");
    char *cp = strdup(filename.c_str());
    if (!cp) {
        reason = "Out of memory (for file name !)\n";
        return false;
    }

    // Using mkstemp this way is awful (bot the suffix adding and
    // using mkstemp() instead of mktemp just to avoid the warnings)
    int fd;
    if ((fd = mkstemp(cp)) < 0) {
        free(cp);
        reason = "TempFileInternal: mkstemp failed\n";
        return false;
    }
    close(fd);
    path_unlink(cp);
    filename = cp;
    free(cp);
    return true;
}
#endif // posix

// The default place to store the default config and other stuff (e.g webqueue)
string path_homedata()
{
#ifdef _WIN32
    wchar_t *cp;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &cp);
    string dir;
    if (cp != 0) {
        wchartoutf8(cp, dir);
    }
    if (!dir.empty()) {
        dir = path_canon(dir);
    } else {
        dir = path_cat(path_home(), "AppData/Local/");
    }
    return dir;
#else
    // We should use an xdg-conforming location, but, history...
    return path_home();
#endif
}

// Check if path is either non-existing or an empty directory.
bool path_empty(const string& path)
{
    if (path_isdir(path)) {
        string reason;
        std::set<string> entries;
        if (!listdir(path, reason, entries) || entries.empty()) {
            return true;
        }
        return false;
    } else {
        return !path_exists(path);
    }
}

string path_defaultrecollconfsubdir()
{
#ifdef _WIN32
    return "Recoll";
#else
    return ".recoll";
#endif
}

// Location for sample config, filters, etc. E.g. /usr/share/recoll/ on linux
// or c:/program files (x86)/recoll/share on Windows
const string& path_pkgdatadir()
{
    static string datadir;
    if (!datadir.empty()) {
        return datadir;
    }
    const char *cdatadir = getenv("RECOLL_DATADIR");
    if (nullptr != cdatadir) {
        datadir = cdatadir;
        return datadir;
    }
    
#if defined(_WIN32)
    // Try a path relative with the exec. This works if we are
    // recoll/recollindex etc.
    // But maybe we are the python module, and execpath is the python
    // exe which could be anywhere. Try the default installation
    // directory, else tell the user to set the environment
    // variable.
    vector<string> paths{path_thisexecpath(), "c:/program files (x86)/recoll",
            "c:/program files/recoll"};
    for (const auto& path : paths) {
        datadir = path_cat(path, "Share");
        if (path_exists(datadir)) {
            return datadir;
        }
    }
    // Not found
    std::cerr << "Could not find the recoll installation data. It is usually "
        "a subfolder of the installation directory. \n"
        "Please set the RECOLL_DATADIR environment variable to point to it\n"
        "(e.g. setx RECOLL_DATADIR \"C:/Program Files (X86)/Recoll/Share)\"\n";
#elif defined(__APPLE__) && !defined(MACPORTS) && !defined(HOMEBREW)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    char *path= (char*)malloc(size+1);
    _NSGetExecutablePath(path, &size);
    datadir = path_cat(path_getfather(path_getfather(path)), "Resources");
    free(path);
#else
    // If not in environment, use the compiled-in constant.
    datadir = RECOLL_DATADIR;
#endif
    return datadir;
}

/* There is a lot of vagueness about what should be percent-encoded or
 * not in a file:// url. The constraint that we have is that we may use
 * the encoded URL to compute (MD5) a thumbnail path according to the
 * freedesktop.org thumbnail spec, which itself does not define what
 * should be escaped. We choose to exactly escape what gio does, as
 * implemented in glib/gconvert.c:g_escape_uri_string(uri, UNSAFE_PATH). 
 * Hopefully, the other desktops have the same set of escaped chars. 
 * Note that $ is not encoded, so the value is not shell-safe.
 */
string url_encode(const string& url, string::size_type offs)
{
    string out = url.substr(0, offs);
    const char *cp = url.c_str();
    for (string::size_type i = offs; i < url.size(); i++) {
        unsigned int c;
        const char *h = "0123456789ABCDEF";
        c = cp[i];
        if (c <= 0x20 ||
            c >= 0x7f ||
            c == '"' ||
            c == '#' ||
            c == '%' ||
            c == ';' ||
            c == '<' ||
            c == '>' ||
            c == '?' ||
            c == '[' ||
            c == '\\' ||
            c == ']' ||
            c == '^' ||
            c == '`' ||
            c == '{' ||
            c == '|' ||
            c == '}') {
            out += '%';
            out += h[(c >> 4) & 0xf];
            out += h[c & 0xf];
        } else {
            out += char(c);
        }
    }
    return out;
}

static inline int h2d(int c) {
    if ('0' <= c && c <= '9')
        return c - '0';
    else if ('A' <= c && c <= 'F')
        return 10 + c - 'A';
    else if ('a' <= c && c <= 'f')
        return 10 + c - 'a';
    else 
        return -1;
}

string url_decode(const string &in)
{
    if (in.size() <= 2)
        return in;
    string out;
    out.reserve(in.size());
    const char *cp = in.c_str();
    string::size_type i = 0;
    for (; i < in.size() - 2; i++) {
        if (cp[i] == '%') {
            int d1 = h2d(cp[i+1]);
            int d2 = h2d(cp[i+2]);
            if (d1 != -1 && d2 != -1) {
                out += (d1 << 4) + d2;
            } else {
                out += '%';
                out += cp[i+1];
                out += cp[i+2];
            }
            i += 2;
        } else {
            out += cp[i];
        }
    }
    while (i < in.size()) {
        out += cp[i++];
    }
    return out;
}

string url_gpath(const string& url)
{
    // Remove the access schema part (or whatever it's called)
    string::size_type colon = url.find_first_of(":");
    if (colon == string::npos || colon == url.size() - 1) {
        return url;
    }
    // If there are non-alphanum chars before the ':', then there
    // probably is no scheme. Whatever...
    for (string::size_type i = 0; i < colon; i++) {
        if (!isalnum(url.at(i))) {
            return url;
        }
    }

    // In addition we canonize the path to remove empty host parts
    // (for compatibility with older versions of recoll where file://
    // was hardcoded, but the local path was used for doc
    // identification.
    return path_canon(url.substr(colon + 1));
}

string url_parentfolder(const string& url)
{
    // In general, the parent is the directory above the full path
    string parenturl = path_getfather(url_gpath(url));
    // But if this is http, make sure to keep the host part. Recoll
    // only has file or http urls for now.
    bool isfileurl = urlisfileurl(url);
    if (!isfileurl && parenturl == "/") {
        parenturl = url_gpath(url);
    }
    return isfileurl ? string("file://") + parenturl :
        string("http://") + parenturl;
}


// Convert to file path if url is like file:
// Note: this only works with our internal pseudo-urls which are not
// encoded/escaped
string fileurltolocalpath(string url)
{
    if (url.find("file://") == 0) {
        url = url.substr(7, string::npos);
    } else {
        return string();
    }

    // If this looks like a Windows path: absolute file urls are like: file:///c:/mydir/...
    // Get rid of the initial '/'
    if (url.size() >= 3 && url[0] == '/' && isalpha(url[1]) && url[2] == ':') {
        url = url.substr(1);
    }

    // Removing the fragment part. This is exclusively used when
    // executing a viewer for the recoll manual, and we only strip the
    // part after # if it is preceded by .html
    string::size_type pos;
    if ((pos = url.rfind(".html#")) != string::npos) {
        url.erase(pos + 5);
    } else if ((pos = url.rfind(".htm#")) != string::npos) {
        url.erase(pos + 4);
    }

    return url;
}

string path_pathtofileurl(const string& path)
{
    // We're supposed to receive a canonic absolute path, but on windows we
    // may need to add a '/' in front of the drive spec
    string url(cstr_fileu);
    if (path.empty() || path[0] != '/') {
        url.push_back('/');
    }
    url += path;
    return url;
}

bool urlisfileurl(const string& url)
{
    return url.find("file://") == 0;
}

// Printable url: this is used to transcode from the system charset
// into either utf-8 if transcoding succeeds, or url-encoded
bool printableUrl(const string& fcharset, const string& in, string& out)
{
#ifdef _WIN32
    PRETEND_USE(fcharset);
    // On windows our paths are always utf-8
    out = in;
#else
    int ecnt = 0;
    if (!transcode(in, out, fcharset, "UTF-8", &ecnt) || ecnt) {
        out = url_encode(in, 7);
    }
#endif
    return true;
}

#ifdef _WIN32
// Convert X:/path to /X/path for path splitting inside the index
string path_slashdrive(const string& path)
{
    string npath;
    if (path_hasdrive(path)) {
        npath.append(1, '/');
        npath.append(1, path[0]);
        if (path_isdriveabs(path)) {
            npath.append(path.substr(2));
        } else {
            // This should be an error really
            npath.append(1, '/');
            npath.append(path.substr(2));
        }
    } else {
        npath = path; ///??
    }
    return npath;
}
#endif // _WIN32

string url_gpathS(const string& url)
{
#ifdef _WIN32
    return path_slashdrive(url_gpath(url));
#else
    return url_gpath(url);
#endif
}

std::string utf8datestring(const std::string& format, struct tm *tm)
{
    string u8date;
#ifdef _WIN32
    wchar_t wformat[200];
    utf8towchar(format, wformat, 199);
    wchar_t wdate[250];
    wcsftime(wdate, 250, wformat, tm);
    wchartoutf8(wdate, u8date);
#else
    char datebuf[200];
    strftime(datebuf, 199, format.c_str(), tm);
    transcode(datebuf, u8date, RclConfig::getLocaleCharset(), "UTF-8");
#endif
    return u8date;
}

const string& tmplocation()
{
    static string stmpdir;
    if (stmpdir.empty()) {
        const char *tmpdir = getenv("RECOLL_TMPDIR");

#ifndef _WIN32
        /* Don't use these under windows because they will return
         * non-ascii non-unicode stuff (would have to call _wgetenv()
         * instead. path_wingetrcltmpdir() will manage */
        if (tmpdir == 0) {
            tmpdir = getenv("TMPDIR");
        }
        if (tmpdir == 0) {
            tmpdir = getenv("TMP");
        }
        if (tmpdir == 0) {
            tmpdir = getenv("TEMP");
        }
#endif

        if (tmpdir == 0) {
#ifdef _WIN32
            stmpdir = path_wingetrcltmpdir();
#else
            stmpdir = "/tmp";
#endif
        } else {
            stmpdir = tmpdir;
        }
        stmpdir = path_canon(stmpdir);
    }

    return stmpdir;
}

bool maketmpdir(string& tdir, string& reason)
{
#ifndef _WIN32
    tdir = path_cat(tmplocation(), "rcltmpXXXXXX");

    char *cp = strdup(tdir.c_str());
    if (!cp) {
        reason = "maketmpdir: out of memory (for file name !)\n";
        tdir.erase();
        return false;
    }

    // There is a race condition between name computation and
    // mkdir. try to make sure that we at least don't shoot ourselves
    // in the foot
#if !defined(HAVE_MKDTEMP)
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);
#endif

    if (!
#ifdef HAVE_MKDTEMP
        mkdtemp(cp)
#else
        mktemp(cp)
#endif // HAVE_MKDTEMP
        ) {
        free(cp);
        reason = "maketmpdir: mktemp failed for [" + tdir + "] : " +
            strerror(errno);
        tdir.erase();
        return false;
    }
    tdir = cp;
    free(cp);
#else // _WIN32
    // There is a race condition between name computation and
    // mkdir. try to make sure that we at least don't shoot ourselves
    // in the foot
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);
    if (!path_gettempfilename(tdir, reason)) {
        return false;
    }
#endif

    // At this point the directory does not exist yet except if we used
    // mkdtemp

#if !defined(HAVE_MKDTEMP) || defined(_WIN32)
    if (mkdir(tdir.c_str(), 0700) < 0) {
        reason = string("maketmpdir: mkdir ") + tdir + " failed";
        tdir.erase();
        return false;
    }
#endif

    return true;
}


class TempFile::Internal {
public:
    Internal(const std::string& suffix);
    ~Internal();
    friend class TempFile;
private:
    std::string m_filename;
    std::string m_reason;
    bool m_noremove{false};
};

TempFile::TempFile(const string& suffix)
    : m(new Internal(suffix))
{
}

TempFile::TempFile()
{
    m = std::shared_ptr<Internal>();
}

const char *TempFile::filename() const
{
    return m ? m->m_filename.c_str() : "";
}

const std::string& TempFile::getreason() const
{
    static string fatal{"fatal error"};
    return m ? m->m_reason : fatal;
}

void TempFile::setnoremove(bool onoff)
{
    if (m)
        m->m_noremove = onoff;
}

bool TempFile::ok() const
{
    return m ? !m->m_filename.empty() : false;
}

TempFile::Internal::Internal(const string& suffix)
{
    // Because we need a specific suffix, can't use mkstemp
    // well. There is a race condition between name computation and
    // file creation. try to make sure that we at least don't shoot
    // our own selves in the foot. maybe we'll use mkstemps one day.
    static std::mutex mmutex;
    std::unique_lock<std::mutex> lock(mmutex);

    if (!path_gettempfilename(m_filename, m_reason)) {
        return;
    }
    m_filename += suffix;
    std::fstream fout;
    if (!path_streamopen(m_filename, ios::out|ios::trunc, fout)) {
        m_reason = string("Open/create error. errno : ") +
            lltodecstr(errno) + " file name: " + m_filename;
        LOGSYSERR("Tempfile::Internal::Internal", "open/create", m_filename);
        m_filename.erase();
    }
}

const std::string& TempFile::rcltmpdir()
{
    return tmplocation();
}

#ifdef _WIN32
static list<string> remainingTempFileNames;
static std::mutex remTmpFNMutex;
#endif

TempFile::Internal::~Internal()
{
    if (!m_filename.empty() && !m_noremove) {
        LOGDEB1("TempFile:~: unlinking " << m_filename << endl);
        if (!path_unlink(m_filename)) {
            LOGSYSERR("TempFile:~", "unlink", m_filename);
#ifdef _WIN32
            {
                std::unique_lock<std::mutex> lock(remTmpFNMutex);
                remainingTempFileNames.push_back(m_filename);
            }
#endif
        } else {
            LOGDEB1("TempFile:~: unlink " << m_filename << " Ok\n");
        }
    }
}

// On Windows we sometimes fail to remove temporary files because
// they are open. It's difficult to make sure this does not
// happen, so we add a cleaning pass after clearing the input
// handlers cache (which should kill subprocesses etc.)
void TempFile::tryRemoveAgain()
{
#ifdef _WIN32
    LOGDEB1("TempFile::tryRemoveAgain. List size: " <<
            remainingTempFileNames.size() << endl);
    std::unique_lock<std::mutex> lock(remTmpFNMutex);
    std::list<string>::iterator pos = remainingTempFileNames.begin();
    while (pos != remainingTempFileNames.end()) {
        if (!path_unlink(*pos)) {
            LOGSYSERR("TempFile::tryRemoveAgain", "unlink", *pos);
            pos++;
        } else {
            pos = remainingTempFileNames.erase(pos);
        }
    }
#endif
}

TempDir::TempDir()
{
    if (!maketmpdir(m_dirname, m_reason)) {
        m_dirname.erase();
        return;
    }
    LOGDEB("TempDir::TempDir: -> " << m_dirname << endl);
}

TempDir::~TempDir()
{
    if (!m_dirname.empty()) {
        LOGDEB("TempDir::~TempDir: erasing " << m_dirname << endl);
        (void)wipedir(m_dirname, true, true);
        m_dirname.erase();
    }
}

bool TempDir::wipe()
{
    if (m_dirname.empty()) {
        m_reason = "TempDir::wipe: no directory !\n";
        return false;
    }
    if (wipedir(m_dirname, false, true)) {
        m_reason = "TempDir::wipe: wipedir failed\n";
        return false;
    }
    return true;
}

// Freedesktop standard paths for cache directory (thumbnails are now in there)
static const string& xdgcachedir()
{
    static string xdgcache;
    if (xdgcache.empty()) {
        const char *cp = getenv("XDG_CACHE_HOME");
        if (cp == 0) {
            xdgcache = path_cat(path_home(), ".cache");
        } else {
            xdgcache = string(cp);
        }
    }
    return xdgcache;
}

static const string& thumbnailsdir()
{
    static string thumbnailsd;
    if (thumbnailsd.empty()) {
        thumbnailsd = path_cat(xdgcachedir(), "thumbnails");
        if (access(thumbnailsd.c_str(), 0) != 0) {
            thumbnailsd = path_cat(path_home(), ".thumbnails");
        }
    }
    return thumbnailsd;
}

// Place for 1024x1024 files
static const string thmbdirxxlarge = "xx-large";
// Place for 512x512 files
static const string thmbdirxlarge = "x-large";
// Place for 256x256 files
static const string thmbdirlarge = "large";
// 128x128
static const string thmbdirnormal = "normal";

static const vector<string> thmbdirs{thmbdirxxlarge, thmbdirxlarge, thmbdirlarge, thmbdirnormal};

static void thumbname(const string& url, string& name)
{
    string digest;
    string l_url = url_encode(url);
    MD5String(l_url, digest);
    MD5HexPrint(digest, name);
    name += ".png";
}

bool thumbPathForUrl(const string& url, int size, string& path)
{
    string name, path128, path256, path512, path1024;
    thumbname(url, name);
    if (size <= 128) {
        path = path_cat(thumbnailsdir(), thmbdirnormal);
        path = path_cat(path, name);
        path128 = path;
    } else if (size <= 256) {
        path = path_cat(thumbnailsdir(), thmbdirlarge);
        path = path_cat(path, name);
        path256 = path;
    } else if (size <= 512) {
        path = path_cat(thumbnailsdir(), thmbdirxlarge);
        path = path_cat(path, name);
        path512 = path;
    } else {
        path = path_cat(thumbnailsdir(), thmbdirxxlarge);
        path = path_cat(path, name);
        path1024 = path;
    }
    if (access(path.c_str(), R_OK) == 0) {
        return true;
    }

    // Not found in requested size. Try to find any size and return it. Let the client scale.
    for (const auto& tdir : thmbdirs) {
        path = path_cat(thumbnailsdir(), tdir);
        path = path_cat(path, name);
        if (access(path.c_str(), R_OK) == 0) {
            return true;
        }
    }

    // File does not exist. Return appropriate path anyway.
    if (size <= 128) {
        path = path128;
    } else if (size <= 256) {
        path = path256;
    } else if (size <= 512) {
        path = path512;
    } else {
        path = path1024;
    }
    return false;
}

// Compare charset names, removing the more common spelling variations
bool samecharset(const string& cs1, const string& cs2)
{
    auto mcs1 = std::accumulate(cs1.begin(), cs1.end(), "", [](const char* m, char i) { return (i != '_' && i != '-') ? m + ::tolower(i) : m; });
    auto mcs2 = std::accumulate(cs2.begin(), cs2.end(), "", [](const char* m, char i) { return (i != '_' && i != '-') ? m + ::tolower(i) : m; });
    return mcs1 == mcs2;
}

static const std::unordered_map<string, string> lang_to_code {
    {"be", "cp1251"},
    {"bg", "cp1251"},
    {"cs", "iso-8859-2"},
    {"el", "iso-8859-7"},
    {"he", "iso-8859-8"},
    {"hr", "iso-8859-2"},
    {"hu", "iso-8859-2"},
    {"ja", "eucjp"},
    {"kk", "pt154"},
    {"ko", "euckr"},
    {"lt", "iso-8859-13"},
    {"lv", "iso-8859-13"},
    {"pl", "iso-8859-2"},
    {"rs", "iso-8859-2"},
    {"ro", "iso-8859-2"},
    {"ru", "koi8-r"},
    {"sk", "iso-8859-2"},
    {"sl", "iso-8859-2"},
    {"sr", "iso-8859-2"},
    {"th", "iso-8859-11"},
    {"tr", "iso-8859-9"},
    {"uk", "koi8-u"},
        };

string langtocode(const string& lang)
{
    const auto it = lang_to_code.find(lang);

    // Use cp1252 by default...
    if (it == lang_to_code.end()) {
        return cstr_cp1252;
    }

    return it->second;
}

string localelang()
{
    const char *lang = getenv("LANG");

    if (lang == nullptr || *lang == 0 || !strcmp(lang, "C") ||
        !strcmp(lang, "POSIX")) {
        return "en";
    }
    string locale(lang);
    string::size_type under = locale.find_first_of('_');
    if (under == string::npos) {
        return locale;
    }
    return locale.substr(0, under);
}

void rclutil_init_mt()
{
    path_pkgdatadir();
    tmplocation();
    thumbnailsdir();
    // Init langtocode() static table
    langtocode("");
}
