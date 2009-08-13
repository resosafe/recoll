#ifndef lint
static char rcsid[] = "@(#$Id: internfile.cpp,v 1.46 2008-10-10 08:04:54 dockes Exp $ (C) 2004 J.F.Dockes";
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

#ifndef TEST_INTERNFILE
#include "autoconfig.h"

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
#include "wipedir.h"
#include "rclconfig.h"
#include "mh_html.h"
#ifdef RCL_USE_XATTR
#include "pxattr.h"
#endif // RCL_USE_XATTR

// The internal path element separator. This can't be the same as the rcldb 
// file to ipath separator : "|"
static const string isep(":");
static const string stxtplain("text/plain");

set<string> FileInterner::o_missingExternal;
map<string, set<string> >  FileInterner::o_typesForMissing;

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
				string &eurl, string &eipath)
{
    eurl = url;
    eipath = ipath;
    string::size_type colon;
    LOGDEB(("FileInterner::getEnclosing(): [%s]\n", eipath.c_str()));
    if (eipath.empty())
	return false;
    if ((colon =  eipath.find_last_of(isep)) != string::npos) {
	eipath.erase(colon);
    } else {
	eipath.erase();
    }
    LOGDEB(("FileInterner::getEnclosing() after: [%s]\n", eipath.c_str()));
    return true;
}

// Uncompress input file into a temporary one, by executing the appropriate
// script.
static bool uncompressfile(RclConfig *conf, const string& ifn, 
			   const list<string>& cmdv, const string& tdir, 
			   string& tfile)
{
    // Make sure tmp dir is empty. we guarantee this to filters
    if (wipedir(tdir) != 0) {
	LOGERR(("uncompressfile: can't clear temp dir %s\n", tdir.c_str()));
	return false;
    }
    string cmd = cmdv.front();

    // Substitute file name and temp dir in command elements
    list<string>::const_iterator it = cmdv.begin();
    ++it;
    list<string> args;
    map<char, string> subs;
    subs['f'] = ifn;
    subs['t'] = tdir;
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
	if (wipedir(tdir.c_str())) {
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
    if (m_tdir.empty() || m_tfile.empty())
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
FileInterner::FileInterner(const string &f, const struct stat *stp,
			   RclConfig *cnf, 
			   const string& td, const string *imime)
    : m_cfg(cnf), m_fn(f), m_forPreview(imime?true:false), m_tdir(td)
{
    bool usfci = false;
    cnf->getConfParam("usesystemfilecommand", &usfci);
    LOGDEB(("FileInterner::FileInterner: [%s] mime [%s] preview %d\n", 
	    f.c_str(), imime?imime->c_str() : "(null)", m_forPreview));

    // We need to run mime type identification in any case to check
    // for a compressed file.
    string l_mime = mimetype(m_fn, stp, m_cfg, usfci);

    // If identification fails, try to use the input parameter. This
    // is then normally not a compressed type (it's the mime type from
    // the db), and is only set when previewing, not for indexing
    if (l_mime.empty() && imime)
	l_mime = *imime;

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
		LOGDEB1(("internfile: after ucomp: m_tdir %s, tfile %s\n", 
			 m_tdir.c_str(), m_tfile.c_str()));
		m_fn = m_tfile;
		// Note: still using the original file's stat. right ?
		l_mime = mimetype(m_fn, stp, m_cfg, usfci);
		if (l_mime.empty() && imime)
		    l_mime = *imime;
	    } else {
		LOGINFO(("internfile: %s over size limit %d kbs\n",
			 m_fn.c_str(), maxkbs));
	    }
	}
    }

    if (l_mime.empty()) {
	// No mime type. We let it through as config may warrant that
	// we index all file names
	LOGDEB(("internfile: (no mime) [%s]\n", m_fn.c_str()));
    }

    // Look for appropriate handler (might still return empty)
    m_mimetype = l_mime;
    Dijon::Filter *df = getMimeHandler(l_mime, m_cfg, !m_forPreview);

    if (!df) {
	// No handler for this type, for now :( if indexallfilenames
	// is set in the config, this normally wont happen (we get mh_unknown)
	LOGERR(("FileInterner:: ignored: [%s] mime [%s]\n", 
		f.c_str(), l_mime.c_str()));
	return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");

    string charset = m_cfg->getDefCharset();
    df->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);

#ifdef RCL_USE_XATTR
    // Get fields computed from extended attributes. We use the
    // original file, not the m_fn which may be the uncompressed temp
    // file
    reapXAttrs(f);
#endif //RCL_USE_XATTR

    if (!df->set_document_file(m_fn)) {
	LOGERR(("FileInterner:: error parsing %s\n", m_fn.c_str()));
	return;
    }
    m_handlers.reserve(MAXHANDLERS);
    for (unsigned int i = 0; i < MAXHANDLERS; i++)
	m_tmpflgs[i] = false;
    m_handlers.push_back(df);
    LOGDEB(("FileInterner::FileInterner: %s [%s]\n", l_mime.c_str(), 
	     m_fn.c_str()));
    m_targetMType = stxtplain;
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
	m_tmpflgs[m_handlers.size()-1] = true;
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
    if (msg.find("RECFILTERROR") == 0) {
	list<string> lerr;
	stringToStrings(msg, lerr);
	if (lerr.size() > 2) {
	    list<string>::iterator it = lerr.begin();
	    lerr.erase(it++);
	    if (*it == "HELPERNOTFOUND") {
		lerr.erase(it++);
		string s;
		stringsToString(lerr, s);
		o_missingExternal.insert(s);
		o_typesForMissing[s].insert(mt);
	    }
	}		    
    }
}

void FileInterner::getMissingExternal(string& out) 
{
    stringsToString(o_missingExternal, out);
}

void FileInterner::getMissingDescription(string& out)
{
    out.erase();

    for (set<string>::const_iterator it = o_missingExternal.begin();
	     it != o_missingExternal.end(); it++) {
	out += *it;
	map<string, set<string> >::const_iterator it2;
	it2 = o_typesForMissing.find(*it);
	if (it2 != o_typesForMissing.end()) {
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
static const string keyau("author");
static const string keycs("charset");
static const string keyct("content");
static const string keyds("description");
static const string keyfn("filename");
static const string keymd("modificationdate");
static const string keymt("mimetype");
static const string keytt("title");

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
	if (it->first == keyct) {
	    doc.text = it->second;
	} else if (it->first == keymd) {
	    doc.dmtime = it->second;
	} else if (it->first == Rcl::Doc::keyoc) {
	    doc.origcharset = it->second;
	} else if (it->first == keymt || it->first == keycs) {
	    // don't need/want these.
	} else {
	    doc.meta[it->first] = it->second;
	}
    }
    if (doc.meta[Rcl::Doc::keyabs].empty() && !doc.meta[keyds].empty()) {
	doc.meta[Rcl::Doc::keyabs] = doc.meta[keyds];
	doc.meta.erase(keyds);
    }
    return true;
}

// Collect the ipath from the current path in the document tree.
// While we're at it, we also set the mimetype and filename, which are special 
// properties: we want to get them from the topmost doc
// with an ipath, not the last one which is usually text/plain
// We also set the author and modification time from the last doc
// which has them.
void FileInterner::collectIpathAndMT(Rcl::Doc& doc, string& ipath) const
{
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
		getKeyValue(docdata, keymt, doc.mimetype);
		getKeyValue(docdata, keyfn, doc.utf8fn);
	    }
	    ipath += ipathel + isep;
	} else {
	    ipath += isep;
	}
	getKeyValue(docdata, keyau, doc.meta[Rcl::Doc::keyau]);
	getKeyValue(docdata, keymd, doc.dmtime);
    }

    // Trim empty tail elements in ipath.
    if (hasipath) {
	LOGDEB2(("IPATH [%s]\n", ipath.c_str()));
	string::size_type sit = ipath.find_last_not_of(isep);
	if (sit == string::npos)
	    ipath.erase();
	else if (sit < ipath.length() -1)
	    ipath.erase(sit+1);
    } else {
	ipath.erase();
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
    const map<string, string>& docdata = 
	m_handlers.back()->get_meta_data();
    string charset, mimetype;
    getKeyValue(docdata, keycs, charset);
    getKeyValue(docdata, keymt, mimetype);

    LOGDEB(("FileInterner::addHandler: next_doc is %s\n", mimetype.c_str()));

    // If we find a document of the target type (text/plain in
    // general), we're done decoding. If we hit text/plain, we're done
    // in any case
    if (!stringicmp(mimetype, m_targetMType) || 
	!stringicmp(mimetype, stxtplain)) {
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
    newflt->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);

    // Get current content: we don't use getkeyvalue() here to avoid
    // copying the text, which may be big.
    string ns;
    const string *txt = &ns;
    {
	map<string,string>::const_iterator it;
	it = docdata.find(keyct);
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
		m_tmpflgs[m_handlers.size()-1] = false;
		m_tempfiles.pop_back();
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
void FileInterner::processNextDocError(Rcl::Doc &doc, string& ipath)
{
    collectIpathAndMT(doc, ipath);
    m_reason = m_handlers.back()->get_error();
    checkExternalMissing(m_reason, doc.mimetype);
    LOGERR(("FileInterner::internfile: next_document error "
	    "[%s%s%s] %s %s\n", m_fn.c_str(), ipath.empty() ? "" : "|", 
	    ipath.c_str(), doc.mimetype.c_str(), m_reason.c_str()));
}

FileInterner::Status FileInterner::internfile(Rcl::Doc& doc, string& ipath)
{
    LOGDEB(("FileInterner::internfile. ipath [%s]\n", ipath.c_str()));
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
    vector<string> vipath(MAXHANDLERS);
    int vipathidx = 0;
    if (!ipath.empty()) {
	list<string> lipath;
	stringToTokens(ipath, lipath, isep, true);
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
	if (loop++ > 1000) {
	    LOGERR(("FileInterner:: looping!\n"));
	    return FIError;
	}
	// If there are no more docs at the current top level we pop and
	// see if there is something at the previous one
	if (!m_handlers.back()->has_documents()) {
	    popHandler();
	    continue;
	}

	// While indexing, don't stop on next_document() error. There
	// might be ie an error while decoding an attachment, but we
	// still want to process the rest of the mbox! For preview: fatal.
	if (!m_handlers.back()->next_document()) {
	    processNextDocError(doc, ipath);
	    if (m_forPreview) 
		return FIError;
	    popHandler();
	    continue;
	}

	// Look at the type for the next document and possibly add
	// handler to stack.
	switch (addHandler()) {
	case ADD_OK: // Just go through: handler has been stacked, use it
	    break; 
	case ADD_CONTINUE: 
	    // forget this doc and retrieve next from current handler
	    // (ipath stays same)
	    continue;
	case ADD_BREAK: 
	    // Stop looping: doc type ok, need complete its processing
	    // and return it
	    goto breakloop; // when you have to you have to
	case ADD_ERROR: return FIError;
	}

	if (!ipath.empty() &&
	    !m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])){
	    LOGERR(("FileInterner::internfile: can't skip\n"));
	    return FIError;
	}
    }
 breakloop:
    if (m_handlers.empty()) {
	LOGDEB(("FileInterner::internfile: conversion ended with no doc\n"));
	return FIError;
    }

    // If indexing compute ipath and significant mimetype.  ipath is
    // returned through the parameter not doc.ipath We also retrieve
    // some metadata fields from the ancesters (like date or
    // author). This is useful for email attachments. The values will
    // be replaced by those internal to the document (by dijontorcl())
    // if any, so the order of calls is important.
    if (!m_forPreview)
	collectIpathAndMT(doc, ipath);
    else
	doc.mimetype = m_reachedMType;

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

// Automatic cleanup of iDocToFile's temp dir
class DirWiper {
 public:
    string dir;
    bool do_it;
    DirWiper(string d) : dir(d), do_it(true) {}
    ~DirWiper() {
	if (do_it) {
	    wipedir(dir);
	    rmdir(dir.c_str());
	}
    }
};

// Extract subdoc out of multidoc into temporary file. 
// We do the usual internfile stuff: create a temporary directory,
// then create an interner and call internfile. 
// We then write the data out of the resulting document into the output file.
// There are two temporary objects:
// - The internfile temporary directory gets destroyed before we
//   return by the DirWiper object
// - The output temporary file which is held in a reference-counted
//   object and will be deleted when done with.
bool FileInterner::idocToFile(TempFile& otemp, const string& tofile,
			      RclConfig *cnf, 
			      const string& fn,
			      const string& ipath,
			      const string& mtype)
{
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
	LOGERR(("FileInterner::idocToFile: can't stat [%s]\n", fn.c_str()));
	return false;
    }

    string tmpdir, reason;
    if (!maketmpdir(tmpdir, reason))
	return false;
    DirWiper wiper(tmpdir);

    FileInterner interner(fn, &st, cnf, tmpdir, &mtype);
    interner.setTargetMType(mtype);
    Rcl::Doc doc;
    string mipath = ipath;
    Status ret = interner.internfile(doc, mipath);
    if (ret == FileInterner::FIError) {
	LOGERR(("FileInterner::idocToFile: internfile() failed\n"));
	return false;
    }

    string filename;
    TempFile temp;
    if (tofile.empty()) {
	TempFile temp1(new TempFileInternal(cnf->getSuffixFromMimeType(mtype)));
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
    FileInterner interner(fn, &st, config, "/tmp");
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
