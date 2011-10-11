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

#ifndef TEST_INTERNFILE
#include "autoconfig.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <map>
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "internfile.h"
#include "rcldoc.h"
#include "mimetype.h"
#include "debuglog.h"
#include "mimehandler.h"
#include "execmd.h"
#include "pathut.h"
#include "rclconfig.h"
#include "mh_html.h"
#include "fileudi.h"
#include "beaglequeuecache.h"
#include "cancelcheck.h"
#include "copyfile.h"
#include "ptmutex.h"

#ifdef RCL_USE_XATTR
#include "pxattr.h"
#endif // RCL_USE_XATTR

static const string cstr_stxtplain("text/plain");

// The internal path element separator. This can't be the same as the rcldb 
// file to ipath separator : "|"
// We replace it with a control char if it comes out of a filter (ie:
// rclzip or rclchm can do this). If you want the SOH control char
// inside an ipath, you're out of luck (and a bit weird).
static const string cstr_isep(":");

static const char cchar_colon_repl = '\x01';
static string colon_hide(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
	out += *it == ':' ? cchar_colon_repl : *it;
    }
    return out;
}
static string colon_restore(const string& in)
{
    string out;
    for (string::const_iterator it = in.begin(); it != in.end(); it++) {
	out += *it == cchar_colon_repl ? ':' : *it;
    }
    return out;
}

#ifdef RCL_USE_XATTR
void FileInterner::reapXAttrs(const string& path)
{
    vector<string> xnames;
    if (!pxattr::list(path, &xnames)) {
	LOGERR(("FileInterner::reapXattrs: pxattr::list: errno %d\n", errno));
	return;
    }
    const map<string, string>& xtof = m_cfg->getXattrToField();
    for (vector<string>::const_iterator it = xnames.begin();
	 it != xnames.end(); it++) {
	map<string, string>::const_iterator mit;
	if ((mit = xtof.find(*it)) != xtof.end()) {
	    string value;
	    if (!pxattr::get(path, *it, &value, pxattr::PXATTR_NOFOLLOW)) {
		LOGERR(("FileInterner::reapXattrs: pxattr::get failed"
			"for %s, errno %d\n", (*it).c_str(), errno));
		continue;
	    }
	    // Encode should we ?
	    m_XAttrsFields[mit->second] = value;
	}
    }
}
#endif // RCL_USE_XATTR

// This is used when the user wants to retrieve a search result doc's parent
// (ie message having a given attachment)
bool FileInterner::getEnclosing(const string &url, const string &ipath,
				string &eurl, string &eipath, string& udi)
{
    eurl = url;
    eipath = ipath;
    string::size_type colon;
    LOGDEB(("FileInterner::getEnclosing(): url [%s] ipath [%s]\n", 
	    url.c_str(), eipath.c_str()));
    if (eipath.empty())
	return false;
    if ((colon =  eipath.find_last_of(cstr_isep)) != string::npos) {
	eipath.erase(colon);
    } else {
	eipath.erase();
    }
    make_udi(url_gpath(eurl), eipath, udi);

    LOGDEB(("FileInterner::getEnclosing() after: [%s]\n", eipath.c_str()));
    return true;
}

// Uncompress input file into a temporary one, by executing the appropriate
// script.
static bool uncompressfile(RclConfig *conf, const string& ifn, 
			   const list<string>& cmdv, TempDir& tdir, 
			   string& tfile)
{
    // Make sure tmp dir is empty. we guarantee this to filters
    if (!tdir.ok() || !tdir.wipe()) {
	LOGERR(("uncompressfile: can't clear temp dir %s\n", tdir.dirname()));
	return false;
    }
    string cmd = cmdv.front();

    // Substitute file name and temp dir in command elements
    list<string>::const_iterator it = cmdv.begin();
    ++it;
    list<string> args;
    map<char, string> subs;
    subs['f'] = ifn;
    subs['t'] = tdir.dirname();
    for (; it != cmdv.end(); it++) {
	string ns;
	pcSubst(*it, ns, subs);
	args.push_back(ns);
    }

    // Execute command and retrieve output file name, check that it exists
    ExecCmd ex;
    int status = ex.doexec(cmd, args, 0, &tfile);
    if (status || tfile.empty()) {
	LOGERR(("uncompressfile: doexec: failed for [%s] status 0x%x\n", 
		ifn.c_str(), status));
	if (!tdir.wipe()) {
	    LOGERR(("uncompressfile: wipedir failed\n"));
	}
	return false;
    }
    if (tfile[tfile.length() - 1] == '\n')
	tfile.erase(tfile.length() - 1, 1);
    return true;
}

// Delete temporary uncompressed file
void FileInterner::tmpcleanup()
{
    if (m_tfile.empty())
	return;
    if (unlink(m_tfile.c_str()) < 0) {
	LOGERR(("FileInterner::tmpcleanup: unlink(%s) errno %d\n", 
		m_tfile.c_str(), errno));
	return;
    }
}

// Constructor: identify the input file, possibly create an
// uncompressed temporary copy, and create the top filter for the
// uncompressed file type.
//
// Empty handler on return says that we're in error, this will be
// processed by the first call to internfile().
// Split into "constructor calls init()" to allow use from other constructor
FileInterner::FileInterner(const string &f, const struct stat *stp,
			   RclConfig *cnf, 
			   TempDir& td, int flags, const string *imime)
    : m_tdir(td), m_ok(false), m_missingdatap(0)
{
    initcommon(cnf, flags);
    init(f, stp, cnf, flags, imime);
}

void FileInterner::init(const string &f, const struct stat *stp, RclConfig *cnf,
                        int flags, const string *imime)
{
    m_fn = f;

    // This is used by filters which manage some kind of cache.
    // Indexing by udi makes things easier (because they sometimes get a temp 
    // as input
    string udi;
    make_udi(f, "", udi);

    cnf->setKeyDir(path_getfather(m_fn));

    string l_mime;
    bool usfci = false;
    cnf->getConfParam("usesystemfilecommand", &usfci);

    // In general, even when the input mime type is set (when
    // previewing), we can't use it: it's the type for the actual
    // document, but this can be part of a compound document, and
    // we're dealing with the top level file here.
    if (flags & FIF_doUseInputMimetype) {
        if (!imime) {
            LOGERR(("FileInterner:: told to use null imime\n"));
            return;
        }
        l_mime = *imime;
    } else {
        LOGDEB(("FileInterner:: [%s] mime [%s] preview %d\n", 
                f.c_str(), imime?imime->c_str() : "(null)", m_forPreview));

        // We need to run mime type identification in any case to check
        // for a compressed file.
        l_mime = mimetype(m_fn, stp, m_cfg, usfci);

        // If identification fails, try to use the input parameter. This
        // is then normally not a compressed type (it's the mime type from
        // the db), and is only set when previewing, not for indexing
        if (l_mime.empty() && imime)
            l_mime = *imime;
    }

    if (!l_mime.empty()) {
	// Has mime: check for a compressed file. If so, create a
	// temporary uncompressed file, and rerun the mime type
	// identification, then do the rest with the temp file.
	list<string>ucmd;
	if (m_cfg->getUncompressor(l_mime, ucmd)) {
	    // Check for compressed size limit
	    int maxkbs = -1;
	    if (!m_cfg->getConfParam("compressedfilemaxkbs", &maxkbs) ||
		maxkbs < 0 || !stp || int(stp->st_size / 1024) < maxkbs) {
		if (!uncompressfile(m_cfg, m_fn, ucmd, m_tdir, m_tfile)) {
		    return;
		}
		LOGDEB1(("FileInterner:: after ucomp: m_tdir %s, tfile %s\n", 
			 m_tdir.dirname(), m_tfile.c_str()));
		m_fn = m_tfile;
		// Note: still using the original file's stat. right ?
		l_mime = mimetype(m_fn, stp, m_cfg, usfci);
		if (l_mime.empty() && imime)
		    l_mime = *imime;
	    } else {
		LOGINFO(("FileInterner:: %s over size limit %d kbs\n",
			 m_fn.c_str(), maxkbs));
	    }
	}
    }

    if (l_mime.empty()) {
	// No mime type. We let it through as config may warrant that
	// we index all file names
	LOGDEB0(("FileInterner:: no mime: [%s]\n", m_fn.c_str()));
    }

    // Look for appropriate handler (might still return empty)
    m_mimetype = l_mime;
    Dijon::Filter *df = getMimeHandler(l_mime, m_cfg, !m_forPreview);

    if (!df or df->is_unknown()) {
	// No real handler for this type, for now :( 
	LOGDEB(("FileInterner:: unprocessed mime: [%s] [%s]\n", 
		 l_mime.c_str(), f.c_str()));
	if (!df)
	    return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");
    df->set_property(Dijon::Filter::DJF_UDI, udi);

#ifdef RCL_USE_XATTR
    // Get fields computed from extended attributes. We use the
    // original file, not the m_fn which may be the uncompressed temp
    // file
    reapXAttrs(f);
#endif //RCL_USE_XATTR

    if (!df->set_document_file(m_fn)) {
	LOGERR(("FileInterner:: error converting %s\n", m_fn.c_str()));
	return;
    }

    m_handlers.push_back(df);
    LOGDEB(("FileInterner:: init ok %s [%s]\n", l_mime.c_str(), m_fn.c_str()));
    m_ok = true;
}

// Setup from memory data (ie: out of the web cache). imime needs to be set.
FileInterner::FileInterner(const string &data, RclConfig *cnf, 
                           TempDir& td, int flags, const string& imime)
    : m_tdir(td), m_ok(false), m_missingdatap(0)
{
    initcommon(cnf, flags);
    init(data, cnf, flags, imime);
}

void FileInterner::init(const string &data, RclConfig *cnf, 
                        int flags, const string& imime)
{
    if (imime.empty()) {
	LOGERR(("FileInterner: inmemory constructor needs input mime type\n"));
        return;
    }
    m_mimetype = imime;

    // Look for appropriate handler (might still return empty)
    Dijon::Filter *df = getMimeHandler(m_mimetype, m_cfg, !m_forPreview);

    if (!df) {
	// No handler for this type, for now :( if indexallfilenames
	// is set in the config, this normally wont happen (we get mh_unknown)
	LOGDEB(("FileInterner:: unprocessed mime [%s]\n", m_mimetype.c_str()));
	return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");

    bool setres = false;
    if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
	setres = df->set_document_string(data);
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
	setres = df->set_document_data(data.c_str(), data.length());
    } else if (df->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
	string filename;
	if (dataToTempFile(data, m_mimetype, filename)) {
	    if (!(setres=df->set_document_file(filename))) {
		m_tmpflgs[0] = false;
		m_tempfiles.pop_back();
	    }
	}
    }
    if (!setres) {
	LOGINFO(("FileInterner:: set_doc failed inside for mtype %s\n", 
                 m_mimetype.c_str()));
	delete df;
	return;
    }
    m_handlers.push_back(df);
    m_ok = true;
}

void FileInterner::initcommon(RclConfig *cnf, int flags)
{
    m_cfg = cnf;
    m_forPreview = ((flags & FIF_forPreview) != 0);
    // Initialize handler stack.
    m_handlers.reserve(MAXHANDLERS);
    for (unsigned int i = 0; i < MAXHANDLERS; i++)
	m_tmpflgs[i] = false;
    m_targetMType = cstr_stxtplain;
}

// We used a single beagle cache object to access beagle data. We protect it 
// against multiple thread access.
static PTMutexInit o_beagler_mutex;

FileInterner::FileInterner(const Rcl::Doc& idoc, RclConfig *cnf, 
                           TempDir& td, int flags)
    : m_tdir(td), m_ok(false), m_missingdatap(0)
{
    LOGDEB(("FileInterner::FileInterner(idoc)\n"));
    initcommon(cnf, flags);

    // We do insist on having an url...
    if (idoc.url.empty()) {
        LOGERR(("FileInterner::FileInterner:: no url!\n"));
        return;
    }

    // This stuff will be moved to some kind of generic function:
    //   get(idoc, ofn, odata, ometa) 
    // and use some kind of backstore object factory next time we add a 
    // backend (if ever). 
    string backend;
    map<string, string>::const_iterator it;
    if ((it = idoc.meta.find(Rcl::Doc::keybcknd)) != idoc.meta.end())
        backend = it->second;
    
    if (backend.empty() || !backend.compare("FS")) {
        // Filesystem document. Intern from file.
        // The url has to be like file://
        if (idoc.url.find("file://") != 0) {
            LOGERR(("FileInterner: FS backend and non fs url: [%s]\n",
                    idoc.url.c_str()));
            return;
        }
        string fn = idoc.url.substr(7, string::npos);
        struct stat st;
        if (stat(fn.c_str(), &st) < 0) {
            LOGERR(("FileInterner:: cannot access document file: [%s]\n",
                    fn.c_str()));
            return;
        }
        init(fn, &st, cnf, flags, &idoc.mimetype);
    } else if (!backend.compare("BGL")) {
        string data;
        Rcl::Doc dotdoc;
        map<string,string>::const_iterator it = 
            idoc.meta.find(Rcl::Doc::keyudi);
        if (it == idoc.meta.end() || it->second.empty()) {
            LOGERR(("FileInterner:: no udi in idoc\n"));
            return;
        }
        string udi = it->second;

	{
	    PTMutexLocker locker(o_beagler_mutex);
	    // Retrieve from our webcache (beagle data). The beagler
	    // object is created at the first call of this routine and
	    // deleted when the program exits.
	    static BeagleQueueCache o_beagler(cnf);
	    if (!o_beagler.getFromCache(udi, dotdoc, data)) {
		LOGINFO(("FileInterner:: failed fetch from Beagle cache for [%s]\n",
			 udi.c_str()));
		return;
	    }
	}
        if (dotdoc.mimetype.compare(idoc.mimetype)) {
            LOGINFO(("FileInterner:: udi [%s], mimetp mismatch: in: [%s], bgl "
                     "[%s]\n", idoc.mimetype.c_str(), dotdoc.mimetype.c_str()));
        }
        init(data, cnf, flags, dotdoc.mimetype);
    } else {
        LOGERR(("FileInterner:: unknown backend: [%s]\n", backend.c_str()));
        return;
    }
}

FileInterner::~FileInterner()
{
    tmpcleanup();
    for (vector<Dijon::Filter*>::iterator it = m_handlers.begin();
	 it != m_handlers.end(); it++) {
        returnMimeHandler(*it);
    }
    // m_tempfiles will take care of itself
}

// Create a temporary file for a block of data (ie: attachment) found
// while walking the internal document tree, with a type for which the
// handler needs an actual file (ie : external script).
bool FileInterner::dataToTempFile(const string& dt, const string& mt, 
				  string& fn)
{
    // Find appropriate suffix for mime type
    TempFile temp(new TempFileInternal(m_cfg->getSuffixFromMimeType(mt)));
    if (temp->ok()) {
        // We are called before the handler is actually on the stack, so the
        // index is m_handlers.size(). m_tmpflgs is a static array, so this is
        // no problem
	m_tmpflgs[m_handlers.size()] = true;
	m_tempfiles.push_back(temp);
    } else {
	LOGERR(("FileInterner::dataToTempFile: cant create tempfile: %s\n",
		temp->getreason().c_str()));
	return false;
    }

    int fd = open(temp->filename(), O_WRONLY);
    if (fd < 0) {
	LOGERR(("FileInterner::dataToTempFile: open(%s) failed errno %d\n",
		temp->filename(), errno));
	return false;
    }
    if (write(fd, dt.c_str(), dt.length()) != (int)dt.length()) {
	close(fd);
	LOGERR(("FileInterner::dataToTempFile: write to %s failed errno %d\n",
		temp->filename(), errno));
	return false;
    }
    close(fd);
    fn = temp->filename();
    return true;
}

// See if the error string is formatted as a missing helper message,
// accumulate helper name if it is
void FileInterner::checkExternalMissing(const string& msg, const string& mt)
{
    if (m_missingdatap && msg.find("RECFILTERROR") == 0) {
	list<string> lerr;
	stringToStrings(msg, lerr);
	if (lerr.size() > 2) {
	    list<string>::iterator it = lerr.begin();
	    lerr.erase(it++);
	    if (*it == "HELPERNOTFOUND") {
		lerr.erase(it++);
		string s;
		stringsToString(lerr, s);
		m_missingdatap->m_missingExternal.insert(s);
		m_missingdatap->m_typesForMissing[s].insert(mt);
	    }
	}		    
    }
}

void FileInterner::getMissingExternal(FIMissingStore *st, string& out) 
{
    if (st)
	stringsToString(st->m_missingExternal, out);
}

void FileInterner::getMissingDescription(FIMissingStore *st, string& out)
{
    if (st == 0)
	return;

    out.erase();

    for (set<string>::const_iterator it = 
	     st->m_missingExternal.begin();
	 it != st->m_missingExternal.end(); it++) {
	out += *it;
	map<string, set<string> >::const_iterator it2;
	it2 = st->m_typesForMissing.find(*it);
	if (it2 != st->m_typesForMissing.end()) {
	    out += " (";
	    set<string>::const_iterator it3;
	    for (it3 = it2->second.begin(); 
		 it3 != it2->second.end(); it3++) {
		out += *it3;
		out += string(" ");
	    }
	    trimstring(out);
	    out += ")";
	}	
	out += "\n";
    }
}

// Helper for extracting a value from a map.
static inline bool getKeyValue(const map<string, string>& docdata, 
			       const string& key, string& value)
{
    map<string,string>::const_iterator it;
    it = docdata.find(key);
    if (it != docdata.end()) {
	value = it->second;
	LOGDEB2(("getKeyValue: [%s]->[%s]\n", key.c_str(), value.c_str()));
	return true;
    }
    LOGDEB2(("getKeyValue: no value for [%s]\n", key.c_str()));
    return false;
}

// These defs are for the Dijon meta array. Rcl::Doc predefined field
// names are used where appropriate. In some cases, Rcl::Doc names are
// used inside the Dijon metadata (ex: origcharset)
static const string cstr_keyau("author");
static const string cstr_keycs("charset");
static const string cstr_keyct("content");
static const string cstr_keyds("description");
static const string cstr_keyfn("filename");
static const string cstr_keymd("modificationdate");
static const string cstr_keymt("mimetype");
static const string cstr_keytt("title");

bool FileInterner::dijontorcl(Rcl::Doc& doc)
{
    Dijon::Filter *df = m_handlers.back();
    if (df == 0) {
	//??
	LOGERR(("FileInterner::dijontorcl: null top handler ??\n"));
	return false;
    }
    const map<string, string>& docdata = df->get_meta_data();

    for (map<string,string>::const_iterator it = docdata.begin(); 
	 it != docdata.end(); it++) {
	if (it->first == cstr_keyct) {
	    doc.text = it->second;
	} else if (it->first == cstr_keymd) {
	    doc.dmtime = it->second;
	} else if (it->first == Rcl::Doc::keyoc) {
	    doc.origcharset = it->second;
	} else if (it->first == cstr_keymt || it->first == cstr_keycs) {
	    // don't need/want these.
	} else {
	    doc.meta[it->first] = it->second;
	}
    }
    if (doc.meta[Rcl::Doc::keyabs].empty() && !doc.meta[cstr_keyds].empty()) {
	doc.meta[Rcl::Doc::keyabs] = doc.meta[cstr_keyds];
	doc.meta.erase(cstr_keyds);
    }
    return true;
}

// Collect the ipath from the current path in the document tree.
// While we're at it, we also set the mimetype and filename, which are special 
// properties: we want to get them from the topmost doc
// with an ipath, not the last one which is usually text/plain
// We also set the author and modification time from the last doc
// which has them.
void FileInterner::collectIpathAndMT(Rcl::Doc& doc) const
{
    LOGDEB2(("FileInterner::collectIpathAndMT\n"));
    bool hasipath = false;

#ifdef RCL_USE_XATTR
    // Set fields from extended file attributes.
    // These can be overriden by values from inside the file
    for (map<string,string>::const_iterator it = m_XAttrsFields.begin(); 
	 it != m_XAttrsFields.end(); it++) {
	doc.meta[it->first] = it->second;
    }
#endif //RCL_USE_XATTR

    // If there is no ipath stack, the mimetype is the one from the file
    doc.mimetype = m_mimetype;

    string ipathel;
    for (vector<Dijon::Filter*>::const_iterator hit = m_handlers.begin();
	 hit != m_handlers.end(); hit++) {
	const map<string, string>& docdata = (*hit)->get_meta_data();
	if (getKeyValue(docdata, "ipath", ipathel)) {
	    if (!ipathel.empty()) {
		// We have a non-empty ipath
		hasipath = true;
		getKeyValue(docdata, cstr_keymt, doc.mimetype);
		getKeyValue(docdata, cstr_keyfn, doc.utf8fn);
	    }
	    doc.ipath += colon_hide(ipathel) + cstr_isep;
	} else {
	    doc.ipath += cstr_isep;
	}
	getKeyValue(docdata, cstr_keyau, doc.meta[Rcl::Doc::keyau]);
	getKeyValue(docdata, cstr_keymd, doc.dmtime);
    }

    // Trim empty tail elements in ipath.
    if (hasipath) {
	LOGDEB2(("IPATH [%s]\n", doc.ipath.c_str()));
	string::size_type sit = doc.ipath.find_last_not_of(cstr_isep);
	if (sit == string::npos)
	    doc.ipath.erase();
	else if (sit < doc.ipath.length() -1)
	    doc.ipath.erase(sit+1);
    } else {
	doc.ipath.erase();
    }
}

// Remove handler from stack. Clean up temp file if needed.
void FileInterner::popHandler()
{
    if (m_handlers.empty())
	return;
    int i = m_handlers.size()-1;
    if (m_tmpflgs[i]) {
	m_tempfiles.pop_back();
	m_tmpflgs[i] = false;
    }
    returnMimeHandler(m_handlers.back());
    m_handlers.pop_back();
}

enum addResols {ADD_OK, ADD_CONTINUE, ADD_BREAK, ADD_ERROR};

// Just got document from current top handler. See what type it is,
// and possibly add a filter/handler to the stack
int FileInterner::addHandler()
{
    const map<string, string>& docdata = m_handlers.back()->get_meta_data();
    string charset, mimetype;
    getKeyValue(docdata, cstr_keycs, charset);
    getKeyValue(docdata, cstr_keymt, mimetype);

    LOGDEB(("FileInterner::addHandler: next_doc is %s\n", mimetype.c_str()));

    // If we find a document of the target type (text/plain in
    // general), we're done decoding. If we hit text/plain, we're done
    // in any case
    if (!stringicmp(mimetype, m_targetMType) || 
	!stringicmp(mimetype, cstr_stxtplain)) {
	m_reachedMType = mimetype;
	LOGDEB1(("FileInterner::addHandler: target reached\n"));
	return ADD_BREAK;
    }

    // We need to stack another handler. Check stack size
    if (m_handlers.size() > MAXHANDLERS) {
	// Stack too big. Skip this and go on to check if there is
	// something else in the current back()
	LOGERR(("FileInterner::addHandler: stack too high\n"));
	return ADD_CONTINUE;
    }

    Dijon::Filter *newflt = getMimeHandler(mimetype, m_cfg);
    if (!newflt) {
	// If we can't find a handler, this doc can't be handled
	// but there can be other ones so we go on
	LOGINFO(("FileInterner::addHandler: no filter for [%s]\n",
		 mimetype.c_str()));
	return ADD_CONTINUE;
    }
    newflt->set_property(Dijon::Filter::OPERATING_MODE, 
			 m_forPreview ? "view" : "index");
    if (!charset.empty())
	newflt->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);

    // Get current content: we don't use getkeyvalue() here to avoid
    // copying the text, which may be big.
    string ns;
    const string *txt = &ns;
    {
	map<string,string>::const_iterator it;
	it = docdata.find(cstr_keyct);
	if (it != docdata.end())
	    txt = &it->second;
    }
    bool setres = false;
    if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
	setres = newflt->set_document_string(*txt);
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
	setres = newflt->set_document_data(txt->c_str(), txt->length());
    } else if (newflt->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
	string filename;
	if (dataToTempFile(*txt, mimetype, filename)) {
	    if (!(setres = newflt->set_document_file(filename))) {
		m_tmpflgs[m_handlers.size()] = false;
		m_tempfiles.pop_back();
	    } else {
		// Hack here, but really helps perfs: if we happen to
		// create a temp file for, ie, an image attachment,
		// keep it around for preview reuse
		if (!mimetype.compare(0, 6, "image/")) {
		    m_imgtmp = m_tempfiles.back();
		}
	    }
	}
    }
    if (!setres) {
	LOGINFO(("FileInterner::addHandler: set_doc failed inside %s "
		 " for mtype %s\n", m_fn.c_str(), mimetype.c_str()));
	delete newflt;
	if (m_forPreview)
	    return ADD_ERROR;
	return ADD_CONTINUE;
    }
    // add handler and go on, maybe this one will give us text...
    m_handlers.push_back(newflt);
    LOGDEB1(("FileInterner::addHandler: added\n"));
    return ADD_OK;
}

// Information and debug after a next_document error
void FileInterner::processNextDocError(Rcl::Doc &doc)
{
    collectIpathAndMT(doc);
    m_reason = m_handlers.back()->get_error();
    checkExternalMissing(m_reason, doc.mimetype);
    LOGERR(("FileInterner::internfile: next_document error "
	    "[%s%s%s] %s %s\n", m_fn.c_str(), doc.ipath.empty() ? "" : "|", 
	    doc.ipath.c_str(), doc.mimetype.c_str(), m_reason.c_str()));
}

FileInterner::Status FileInterner::internfile(Rcl::Doc& doc, const string& ipath)
{
    LOGDEB(("FileInterner::internfile. ipath [%s]\n", ipath.c_str()));

    // Get rid of possible image tempfile from older call
    m_imgtmp.release();

    if (m_handlers.size() < 1) {
	// Just means the constructor failed
	LOGDEB(("FileInterner::internfile: no handler: constructor failed\n"));
	return FIError;
    }

    // Input Ipath vector when retrieving a given subdoc for previewing
    // Note that the vector is big enough for the maximum stack. All values
    // over the last significant one are ""
    // We set the ipath for the first handler here, others are set
    // when they're pushed on the stack
    vector<string> vipath;
    int vipathidx = 0;
    if (!ipath.empty()) {
	vector<string> lipath;
	stringToTokens(ipath, lipath, cstr_isep, true);
	for (vector<string>::iterator it = lipath.begin();
	     it != lipath.end(); it++) {
	    *it = colon_restore(*it);
	}
	vipath.insert(vipath.begin(), lipath.begin(), lipath.end());
	if (!m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])){
	    LOGERR(("FileInterner::internfile: can't skip\n"));
	    return FIError;
	}
    }

    // Try to get doc from the topmost handler
    // Security counter: looping happens when we stack one other 
    // handler or when walking the file document tree without finding
    // something to index (typical exemple: email with multiple image
    // attachments and no image filter installed). So we need to be
    // quite generous here, especially because there is another
    // security in the form of a maximum handler stack size.
    int loop = 0;
    while (!m_handlers.empty()) {
        CancelCheck::instance().checkCancel();
	if (loop++ > 1000) {
	    LOGERR(("FileInterner:: looping!\n"));
	    return FIError;
	}
	// If there are no more docs at the current top level we pop and
	// see if there is something at the previous one
	if (!m_handlers.back()->has_documents()) {
            // If looking for a specific doc, this is an error. Happens if
            // the index is stale, and the ipath points to the wrong message
            // for exemple (one with less attachments)
	    if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR(("FileInterner: requested document does not exist\n"));
		return FIError;
            }
	    popHandler();
	    continue;
	}

	// While indexing, don't stop on next_document() error. There
	// might be ie an error while decoding an attachment, but we
	// still want to process the rest of the mbox! For preview: fatal.
	if (!m_handlers.back()->next_document()) {
	    processNextDocError(doc);
	    if (m_forPreview) {
                m_reason += "Requested document does not exist. ";
                m_reason += m_handlers.back()->get_error();
                LOGERR(("FileInterner: requested document does not exist\n"));
		return FIError;
            }
	    popHandler();
	    continue;
	}

	// Look at the type for the next document and possibly add
	// handler to stack.
	switch (addHandler()) {
	case ADD_OK: // Just go through: handler has been stacked, use it
	    LOGDEB2(("addHandler returned OK\n"));
	    break; 
	case ADD_CONTINUE: 
	    // forget this doc and retrieve next from current handler
	    // (ipath stays same)
	    LOGDEB2(("addHandler returned CONTINUE\n"));
	    continue;
	case ADD_BREAK: 
	    // Stop looping: doc type ok, need complete its processing
	    // and return it
	    LOGDEB2(("addHandler returned BREAK\n"));
	    goto breakloop; // when you have to you have to
	case ADD_ERROR: 
	    LOGDEB2(("addHandler returned ERROR\n"));
	    return FIError;
	}

	// If we have an ipath, meaning that we are seeking a specific
	// document (ie: previewing a search result), we may have to
	// seek to the correct entry of a compound doc (ie: archive or
	// mail). When we are out of ipath entries, we stop seeking,
	// the handlers stack may still grow for translation (ie: if
	// the target doc is msword, we'll still stack the
	// word-to-text translator).
	if (!ipath.empty()) {
	    if (m_handlers.size() <= vipath.size() &&
		!m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])) {
		LOGERR(("FileInterner::internfile: can't skip\n"));
		return FIError;
	    }
	}
    }
 breakloop:
    if (m_handlers.empty()) {
	LOGDEB(("FileInterner::internfile: conversion ended with no doc\n"));
	return FIError;
    }

    // If indexing compute ipath and significant mimetype.  ipath is
    // returned through doc.ipath. We also retrieve some metadata
    // fields from the ancesters (like date or author). This is useful
    // for email attachments. The values will be replaced by those
    // internal to the document (by dijontorcl()) if any, so the order
    // of calls is important.
    if (!m_forPreview) {
	collectIpathAndMT(doc);
    } else {
	doc.mimetype = m_reachedMType;
    }
    // Keep this AFTER collectIpathAndMT
    dijontorcl(doc);

    // Possibly destack so that we can test for FIDone. While doing this
    // possibly set aside an ancestor html text (for the GUI preview)
    while (!m_handlers.empty() && !m_handlers.back()->has_documents()) {
	if (m_forPreview) {
	    MimeHandlerHtml *hth = 
		dynamic_cast<MimeHandlerHtml*>(m_handlers.back());
	    if (hth) {
		m_html = hth->get_html();
	    }
	}
	popHandler();
    }
    if (m_handlers.empty())
	return FIDone;
    else 
	return FIAgain;
}

// Temporary while we fix backend things
static string urltolocalpath(string url)
{
    return url.substr(7, string::npos);
}

// Extract subdoc out of multidoc into temporary file. 
// We do the usual internfile stuff: create a temporary directory,
// then create an interner and call internfile. The target mtype is set to
// the input mtype, so that no data conversion is performed.
// We then write the data out of the resulting document into the output file.
// There are two temporary objects:
// - The internfile temporary directory gets destroyed by its destructor
// - The output temporary file which is held in a reference-counted
//   object and will be deleted when done with.
bool FileInterner::idocToFile(TempFile& otemp, const string& tofile,
			      RclConfig *cnf, const Rcl::Doc& idoc)
{
    LOGDEB(("FileInterner::idocToFile\n"));
    idoc.dump();

    TempDir tmpdir;

    // We set FIF_forPreview for consistency with the previous version
    // which determined this by looking at mtype!=null. Probably
    // doesn't change anything in this case.
    FileInterner interner(idoc, cnf, tmpdir, FIF_forPreview);
    interner.setTargetMType(idoc.mimetype);
    Rcl::Doc doc;
    Status ret = interner.internfile(doc, idoc.ipath);
    if (ret == FileInterner::FIError) {
	LOGERR(("FileInterner::idocToFile: internfile() failed\n"));
	return false;
    }

    // Specialcase text/html. This is to work around a bug that will
    // get fixed some day: internfile initialisation does not check
    // targetmtype, so that at least one conversion is always
    // performed. A common case would be an "Open" on an html file
    // (we'd end up with text/plain content). As the html version is
    // saved in this case, use it.  
    if (!stringlowercmp("text/html", idoc.mimetype) && 
        !interner.get_html().empty()) {
        doc.text = interner.get_html();
        doc.mimetype = "text/html";
    }

    string filename;
    TempFile temp;
    if (tofile.empty()) {
	TempFile temp1(new TempFileInternal(cnf->getSuffixFromMimeType(idoc.mimetype)));
	temp = temp1;
	if (!temp->ok()) {
	    LOGERR(("FileInterner::idocToFile: cant create temporary file"));
	    return false;
	}
	filename = temp->filename();
    } else {
	filename = tofile;
    }
    int fd = open(filename.c_str(), O_WRONLY|O_CREAT, 0600);
    if (fd < 0) {
	LOGERR(("FileInterner::idocToFile: open(%s) failed errno %d\n",
		filename.c_str(), errno));
	return false;
    }
    const string& dt = doc.text;
    if (write(fd, dt.c_str(), dt.length()) != (int)dt.length()) {
	close(fd);
	LOGERR(("FileInterner::idocToFile: write to %s failed errno %d\n",
		filename.c_str(), errno));
	return false;
    }
    close(fd);
    if (tofile.empty())
	otemp = temp;
    return true;
}

bool FileInterner::isCompressed(const string& fn, RclConfig *cnf)
{
    LOGDEB(("FileInterner::isCompressed: [%s]\n", fn.c_str()));
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
        LOGERR(("FileInterner::isCompressed: can't stat [%s]\n", fn.c_str()));
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR(("FileInterner::isUncompressed: can't get mime for [%s]\n", 
                fn.c_str()));
        return false;
    }

    list<string>ucmd;
    if (cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    return false;
}

bool FileInterner::maybeUncompressToTemp(TempFile& temp, const string& fn, 
                                         RclConfig *cnf, const Rcl::Doc& doc)
{
    LOGDEB(("FileInterner::maybeUncompressToTemp: [%s]\n", fn.c_str()));
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
        LOGERR(("FileInterner::maybeUncompressToTemp: can't stat [%s]\n", 
                fn.c_str()));
        return false;
    }
    string l_mime = mimetype(fn, &st, cnf, true);
    if (l_mime.empty()) {
        LOGERR(("FileInterner::maybeUncompress.: can't id. mime for [%s]\n", 
                fn.c_str()));
        return false;
    }

    list<string>ucmd;
    if (!cnf->getUncompressor(l_mime, ucmd)) {
        return true;
    }
    // Check for compressed size limit
    int maxkbs = -1;
    if (cnf->getConfParam("compressedfilemaxkbs", &maxkbs) &&
        maxkbs >= 0 && int(st.st_size / 1024) > maxkbs) {
        LOGINFO(("FileInterner:: %s over size limit %d kbs\n",
                 fn.c_str(), maxkbs));
        return false;
    }
    TempDir tmpdir;
    temp = 
      TempFile(new TempFileInternal(cnf->getSuffixFromMimeType(doc.mimetype)));
    if (!tmpdir.ok() || !temp->ok()) {
        LOGERR(("FileInterner: cant create temporary file/dir"));
        return false;
    }

    string uncomped;
    if (!uncompressfile(cnf, fn, ucmd, tmpdir, uncomped)) {
        return false;
    }

    // uncompressfile choses the output file name, there is good
    // reason for this, but it's not nice here. Have to move, the
    // uncompressed file, hopefully staying on the same dev.
    string reason;
    if (!renameormove(uncomped.c_str(), temp->filename(), reason)) {
        LOGERR(("FileInterner::maybeUncompress: move [%s] -> [%s] "
                "failed: %s\n", 
                uncomped.c_str(), temp->filename(), reason.c_str()));
        return false;
    }
    return true;
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <sys/stat.h>

using namespace std;

#include "debuglog.h"
#include "rclinit.h"
#include "internfile.h"
#include "rclconfig.h"
#include "rcldoc.h"

static string thisprog;

static string usage =
    " internfile <filename> [ipath]\n"
    "  \n\n"
    ;

static void
Usage(void)
{
    cerr << thisprog  << ": usage:\n" << usage;
    exit(1);
}

static int        op_flags;
#define OPT_q	  0x1 

RclConfig *config;
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
    DebugLog::getdbl()->setloglevel(DEBDEB1);
    DebugLog::setfilename("stderr");

    if (argc < 1)
	Usage();
    string fn(*argv++);
    argc--;
    string ipath;
    if (argc >= 1) {
	ipath.append(*argv++);
	argc--;
    }
    string reason;
    config = recollinit(0, 0, reason);

    if (config == 0 || !config->ok()) {
	string str = "Configuration problem: ";
	str += reason;
	fprintf(stderr, "%s\n", str.c_str());
	exit(1);
    }
    struct stat st;
    if (stat(fn.c_str(), &st)) {
	perror("stat");
	exit(1);
    }
    FileInterner interner(fn, &st, config, "/tmp", 0);
    Rcl::Doc doc;
    FileInterner::Status status = interner.internfile(doc, ipath);
    switch (status) {
    case FileInterner::FIDone:
    case FileInterner::FIAgain:
	break;
    case FileInterner::FIError:
    default:
	fprintf(stderr, "internfile failed\n");
	exit(1);
    }
    
    cout << "doc.url [[[[" << doc.url << 
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.ipath [[[[" << doc.ipath <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.mimetype [[[[" << doc.mimetype <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.fmtime [[[[" << doc.fmtime <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.dmtime [[[[" << doc.dmtime <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.origcharset [[[[" << doc.origcharset <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[title] [[[[" << doc.meta["title"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[keywords] [[[[" << doc.meta["keywords"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.meta[abstract] [[[[" << doc.meta["abstract"] <<
	"]]]]\n-----------------------------------------------------\n" <<
	"doc.text [[[[" << doc.text << "]]]]\n";
}

#endif // TEST_INTERNFILE
