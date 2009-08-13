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
#ifndef _INTERNFILE_H_INCLUDED_
#define _INTERNFILE_H_INCLUDED_
/* @(#$Id: internfile.h,v 1.21 2008-10-08 16:15:22 dockes Exp $  (C) 2004 J.F.Dockes */

#include <string>
#include <vector>
#include <map>
#include <set>
using std::string;
using std::vector;
using std::map;
using std::set;

#include "pathut.h"
#include "Filter.h"
// Beware: the class changes according to RCL_USE_XATTR, so any file
// including this needs autoconfig.h
#include "autoconfig.h"

class RclConfig;
namespace Rcl {
class Doc;
}

struct stat;

/** 
 * A class to convert a file into possibly multiple documents in internal 
 * representation.
 */
class FileInterner {
 public:
    /**
     * Get immediate parent for document. 
     *
     * This is not in general the same as the "parent" document used 
     * with Rcl::Db::addOrUpdate(). The latter is generally the enclosing file. 
     */
    static bool getEnclosing(const string &url, const string &ipath,
			     string &eurl, string &eipath);
    /**
     * Identify and possibly decompress file, create adequate
     * handler. The mtype parameter is only set when the object is
     * created for previewing a file. Filter output may be
     * different for previewing and indexing.
     *
     * @param fn file name 
     * @param stp pointer to updated stat struct.
     * @param cnf Recoll configuration
     * @param td  temporary directory to use as working space if 
     *   decompression needed. Must be private and will be wiped clean.
     * @param mtype mime type if known. For a compressed file this is the 
     *   mime type for the uncompressed version. This currently doubles up 
     *   to indicate that this object is for previewing (not indexing).
     */
    FileInterner(const string &fn, const struct stat *stp, 
		 RclConfig *cnf, const string& td,
		 const string *mtype = 0);

    ~FileInterner();

    /// Return values for internfile()
    enum Status {FIError, FIDone, FIAgain};

    /** 
     * Turn file or file part into Recoll document.
     * 
     * For multidocument files (ie: mail folder), this must be called multiple
     * times to retrieve the subdocuments
     * @param doc output document
     * @param ipath internal path. If set by caller, the specified subdoc will
     *  be returned. Else the next document according to current state will 
     *  be returned, and the internal path will be set.
     * @return FIError and FIDone are self-explanatory. If FIAgain is returned,
     *  this is a multi-document file, with more subdocs, and internfile() 
     * should be called again to get the following one(s).
     */
    Status internfile(Rcl::Doc& doc, string &ipath);

    /** Return the file's mimetype (useful for container files) */
    const string&  getMimetype() {return m_mimetype;}

    /** We normally always return text/plain data. A caller can request
     *  that we stop conversion at the native document type (ie: extracting
     *  an email attachment and starting an external viewer)
     */
    void setTargetMType(const string& tp) {m_targetMType = tp;}

    /* In case we see an html version, it's set aside and can be recovered */
    const string& get_html() {return m_html;}

    /** Extract internal document into temporary file. 
     *  This is used mainly for starting an external viewer for a
     *  subdocument (ie: mail attachment).
     * @return true for success.
     * @param temp output reference-counted temp file object (goes
     *   away magically). Only used if tofile.empty()
     * @param tofile output file if not null
     * @param cnf The recoll config
     * @param fn  The main document from which to extract
     * @param ipath The internal path to the subdoc
     * @param mtype The target mime type (we don't want to decode to text!)
     */
    static bool idocToFile(TempFile& temp, const string& tofile, 
			   RclConfig *cnf, const string& fn, 
			   const string& ipath, const string& mtype);

    const string& getReason() const {return m_reason;}
    static void getMissingExternal(string& missing);
    static void getMissingDescription(string& desc);

 private:
    static const unsigned int MAXHANDLERS = 20;
    RclConfig             *m_cfg;
    string                 m_fn;
    string                 m_mimetype; // Mime type for [uncompressed] file
    bool                   m_forPreview;
    string                 m_html; // Possibly set-aside html text for preview
    string                 m_targetMType;
    string                 m_reachedMType; // target or text/plain
    // m_tdir and m_tfile are used only for decompressing input file if needed
    const string&          m_tdir; 
    string                 m_tfile;
#ifdef RCL_USE_XATTR
    // Fields found in file extended attributes. This is kept here,
    // not in the file-level handler because we are only interested in
    // the top-level file, not any temp file necessitated by
    // processing the internal doc hierarchy.
    map<string, string> m_XAttrsFields;
#endif // RCL_USE_XATTR

    // Filter stack, path to the current document from which we're
    // fetching subdocs
    vector<Dijon::Filter*> m_handlers;
    // Temporary files used for decoding the current stack
    bool                   m_tmpflgs[MAXHANDLERS];
    vector<TempFile>       m_tempfiles;
    // Error data if any
    string                 m_reason;
    // Missing external programs
    static set<string>           o_missingExternal;
    static map<string, set<string> > o_typesForMissing;

    void tmpcleanup();
    bool dijontorcl(Rcl::Doc&);
    void collectIpathAndMT(Rcl::Doc&, string& ipath) const;
    bool dataToTempFile(const string& data, const string& mt, string& fn);
    void popHandler();
    int addHandler();
    void checkExternalMissing(const string& msg, const string& mt);
    void processNextDocError(Rcl::Doc &doc, string& ipath);
#ifdef RCL_USE_XATTR
    void reapXAttrs(const string& fn);
#endif
};

 
#endif /* _INTERNFILE_H_INCLUDED_ */
