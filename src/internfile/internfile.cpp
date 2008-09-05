#ifndef lint
static char rcsid[] = "@(#$Id: internfile.cpp,v 1.38.2.1 2008-09-05 11:34:42 dockes Exp $ (C) 2004 J.F.Dockes";
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

// The internal path element separator. This can't be the same as the rcldb 
// file to ipath separator : "|"
static const string isep(":");

// Execute the command to uncompress a file into a temporary one.
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
	LOGERR(("uncompressfile: doexec: status 0x%x\n", status));
	rmdir(tdir.c_str());
	return false;
    }
    if (tfile[tfile.length() - 1] == '\n')
	tfile.erase(tfile.length() - 1, 1);
    return true;
}

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

// Handler==0 on return says we're in error, will be handled when calling
// internfile
FileInterner::FileInterner(const std::string &f, const struct stat *stp,
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
	LOGDEB(("FileInterner:: no handler for %s\n", l_mime.c_str()));
	return;
    }
    df->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");

    string charset = m_cfg->getDefCharset();
    df->set_property(Dijon::Filter::DEFAULT_CHARSET, charset);
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
    m_targetMType = "text/plain";
}

FileInterner::~FileInterner()
{
    tmpcleanup();
    for (vector<Dijon::Filter*>::iterator it = m_handlers.begin();
	 it != m_handlers.end(); it++)
	delete *it;
    // m_tempfiles will take care of itself
}

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
void FileInterner::maybeExternalMissing(const string& msg)
{
    if (msg.find("RECFILTERROR") == 0) {
	list<string> lerr;
	stringToStrings(msg, lerr);
	if (lerr.size() > 2) {
	    list<string>::iterator it = lerr.begin();
	    it++;
	    if (*it == "HELPERNOTFOUND") {
		it++;
		m_missingExternal.push_back(it->c_str());
	    }
	}		    
    }
}

const list<string>& FileInterner::getMissingExternal() 
{
    m_missingExternal.sort();
    m_missingExternal.unique();
    return m_missingExternal;
}
void FileInterner::getMissingExternal(string& out) 
{
    m_missingExternal.sort();
    m_missingExternal.unique();
    stringsToString(m_missingExternal, out);
}

static inline bool getKeyValue(const map<string, string>& docdata, 
			       const string& key, string& value)
{
    map<string,string>::const_iterator it;
    it = docdata.find(key);
    if (it != docdata.end()) {
	value = it->second;
	return true;
    }
    return false;
}

static const string keyab("abstract");
static const string keyau("author");
static const string keycs("charset");
static const string keyct("content");
static const string keyds("description");
static const string keyfn("filename");
static const string keykw("keywords");
static const string keymd("modificationdate");
static const string keymt("mimetype");
static const string keyoc("origcharset");
static const string keytt("title");

bool FileInterner::dijontorcl(Rcl::Doc& doc)
{
    Dijon::Filter *df = m_handlers.back();
    const std::map<std::string, std::string>& docdata = df->get_meta_data();

    for (map<string,string>::const_iterator it = docdata.begin(); 
	 it != docdata.end(); it++) {
	if (it->first == keyct) {
	    doc.text = it->second;
	} else if (it->first == keymd) {
	    doc.dmtime = it->second;
	} else if (it->first == keyoc) {
	    doc.origcharset = it->second;
	} else if (it->first == keymt || it->first == keycs) {
	    // don't need these.
	} else {
	    doc.meta[it->first] = it->second;
	}
    }
    if (doc.meta[keyab].empty() && !doc.meta[keyds].empty()) {
	doc.meta[keyab] = doc.meta[keyds];
	doc.meta.erase(keyds);
    }
    return true;
}

// Collect the ipath stack. 
// While we're at it, we also set the mimetype and filename, which are special 
// properties: we want to get them from the topmost doc
// with an ipath, not the last one which is usually text/plain
// We also set the author and modification time from the last doc
// which has them.
void FileInterner::collectIpathAndMT(Rcl::Doc& doc, string& ipath) const
{
    bool hasipath = false;

    // If there is no ipath stack, the mimetype is the one from the file
    doc.mimetype = m_mimetype;
    LOGDEB2(("INITIAL mimetype: %s\n", doc.mimetype.c_str()));

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
	getKeyValue(docdata, keyau, doc.meta["author"]);
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
    delete m_handlers.back();
    m_handlers.pop_back();
}

FileInterner::Status FileInterner::internfile(Rcl::Doc& doc, string& ipath)
{
    LOGDEB(("FileInterner::internfile. ipath [%s]\n", ipath.c_str()));
    if (m_handlers.size() < 1) {
	// Just means the constructor failed
	LOGERR(("FileInterner::internfile: constructor failed\n"));
	return FIError;
    }

    // Ipath vector.
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

    /* Try to get doc from the topmost filter */

    // Security counter: looping happens when we stack one other
    // handler or when walking the file document tree without finding
    // something to index (typical exemple: email with multiple image
    // attachments and no image filter installed). So we need to be
    // quite generous here, especially because there is another
    // security in the form of a maximum handler stack size.
    int loop = 0;
    while (!m_handlers.empty()) {
	if (loop++ > 500) {
	    LOGERR(("FileInterner:: looping!\n"));
	    return FIError;
	}
	if (!m_handlers.back()->has_documents()) {
	    // No docs at the current top level. Pop and see if there
	    // is something at the previous one
	    popHandler();
	    continue;
	}

	// Don't stop on next_document() error. There might be ie an
	// error while decoding an attachment, but we still want to
	// process the rest of the mbox!
	if (!m_handlers.back()->next_document()) {
	    Rcl::Doc doc; string ipath;
	    collectIpathAndMT(doc, ipath);
	    m_reason = m_handlers.back()->get_error();
	    maybeExternalMissing(m_reason);
	    LOGERR(("FileInterner::internfile: next_document error [%s%s%s] %s\n",
		    m_fn.c_str(), ipath.empty()?"":"|", ipath.c_str(), 
		    m_reason.c_str()));
	    // If fetching a specific document, this is fatal
	    if (m_forPreview) {
		return FIError;
	    }
	    popHandler();
	    continue;
	}

	// Look at what we've got
	const std::map<std::string, std::string>& docdata = 
	    m_handlers.back()->get_meta_data();
	string charset, mimetype;
	getKeyValue(docdata, keycs, charset);
	getKeyValue(docdata, keymt, mimetype);

	LOGDEB(("FileInterner::internfile: next_doc is %s\n",
		mimetype.c_str()));
	// If we find a text/plain doc, we're done
	if (!stringicmp(mimetype, m_targetMType))
	    break;

	// Got a non text/plain doc. We need to stack another
	// filter. Check current size
	if (m_handlers.size() > MAXHANDLERS) {
	    // Stack too big. Skip this and go on to check if there is
	    // something else in the current back()
	    LOGINFO(("FileInterner::internfile: stack too high\n"));
	    continue;
	}

	Dijon::Filter *again = getMimeHandler(mimetype, m_cfg);
	if (!again) {
	    // If we can't find a filter, this doc can't be handled
	    // but there can be other ones so we go on
	    LOGINFO(("FileInterner::internfile: no filter for [%s]\n",
		    mimetype.c_str()));
	    continue;
	}
	again->set_property(Dijon::Filter::OPERATING_MODE, 
			    m_forPreview ? "view" : "index");
	again->set_property(Dijon::Filter::DEFAULT_CHARSET, 
			    charset);
	string ns;
	const string *txt = &ns;
	map<string,string>::const_iterator it;
	it = docdata.find("content");
	if (it != docdata.end())
	    txt = &it->second;

	bool setres = false;
	if (again->is_data_input_ok(Dijon::Filter::DOCUMENT_STRING)) {
	    setres = again->set_document_string(*txt);
	} else if (again->is_data_input_ok(Dijon::Filter::DOCUMENT_DATA)) {
	    setres = again->set_document_data(txt->c_str(), txt->length());
	}else if(again->is_data_input_ok(Dijon::Filter::DOCUMENT_FILE_NAME)) {
	    string filename;
	    if (dataToTempFile(*txt, mimetype, filename)) {
		if (!(setres = again->set_document_file(filename))) {
		    m_tmpflgs[m_handlers.size()-1] = false;
		    m_tempfiles.pop_back();
		}
	    }
	}
	if (!setres) {
	    LOGINFO(("FileInterner::internfile: set_doc failed inside %s\n", 
		    m_fn.c_str()));
	    delete again;
	    if (m_forPreview)
		return FIError;
	    continue;
	}
	// add filter and go on, maybe this one will give us text...
	m_handlers.push_back(again);
	if (!ipath.empty() &&
	    !m_handlers.back()->skip_to_document(vipath[m_handlers.size()-1])){
	    LOGERR(("FileInterner::internfile: can't skip\n"));
	    return FIError;
	}
    }

    if (m_handlers.empty()) {
	LOGERR(("FileInterner::internfile: conversion ended with no doc\n"));
	return FIError;
    }

    // If indexing compute ipath and significant mimetype Note that
    // ipath is returned through the parameter not doc.ipath We also
    // retrieve some metadata fields from the ancesters (like date or
    // author). This is useful for email attachments. The values will
    // be replaced by those found by dijontorcl if any, so the order
    // of calls is important.
    if (!m_forPreview)
	collectIpathAndMT(doc, ipath);
    // Keep this AFTER collectIpathAndMT
    dijontorcl(doc);

    // Destack what can be
    while (!m_handlers.empty() && !m_handlers.back()->has_documents()) {
	popHandler();
    }
    if (m_handlers.empty())
	return FIDone;
    else 
	return FIAgain;
}


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
bool FileInterner::idocTempFile(TempFile& otemp, RclConfig *cnf, 
				const string& fn,
				const string& ipath,
				const string& mtype)
{
    struct stat st;
    if (stat(fn.c_str(), &st) < 0) {
	LOGERR(("FileInterner::idocTempFile: can't stat [%s]\n", fn.c_str()));
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
	LOGERR(("FileInterner::idocTempFile: internfile() failed\n"));
	return false;
    }
    TempFile temp(new TempFileInternal(cnf->getSuffixFromMimeType(mtype)));
    if (!temp->ok()) {
	LOGERR(("FileInterner::idocTempFile: cannot create temporary file"));
	return false;
    }
    int fd = open(temp->filename(), O_WRONLY);
    if (fd < 0) {
	LOGERR(("FileInterner::idocTempFile: open(%s) failed errno %d\n",
		temp->filename(), errno));
	return false;
    }
    const string& dt = doc.text;
    if (write(fd, dt.c_str(), dt.length()) != (int)dt.length()) {
	close(fd);
	LOGERR(("FileInterner::idocTempFile: write to %s failed errno %d\n",
		temp->filename(), errno));
	return false;
    }
    close(fd);
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
    RclConfig *config = recollinit(0, 0, reason);

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
