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
/* @(#$Id: internfile.h,v 1.10.2.1 2007-01-15 13:06:57 dockes Exp $  (C) 2004 J.F.Dockes */

#include <string>
#include <vector>
using std::string;
using std::vector;

#include "pathut.h"
#include "Filter.h"

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
     * Identify and possibly decompress file, create adequate
     * handler. The mtype parameter is only set when the object is
     * created for previewing a file. Filter output may be
     * different for previewing and indexing.
     *
     * @param fn file name 
     * @param cnf Recoll configuration
     * @param td  temporary directory to use as working space if 
     *            decompression needed.
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
    const string&  get_mimetype() {return m_mimetype;}

    /** We normally always return text/plain data. A caller can request
     *  that we stop conversion at the native document type (ie: text/html) 
     */
    void setTargetMType(const string& tp) {m_targetMType = tp;}

    /** Utility function: extract internal document and make temporary file */
    static bool idocTempFile(TempFile& temp, RclConfig *cnf, const string& fn, 
			     const string& ipath, const string& mtype);

 private:
    static const unsigned int MAXHANDLERS = 20;
    RclConfig             *m_cfg;
    string                 m_fn;
    string                 m_mimetype; // Mime type for [uncompressed] file
    bool                   m_forPreview;
    string                 m_targetMType;
    // m_tdir and m_tfile are used only for decompressing input file if needed
    const string&          m_tdir; 
    string                 m_tfile;
    vector<Dijon::Filter*> m_handlers;
    bool                   m_tmpflgs[MAXHANDLERS];
    vector<TempFile>       m_tempfiles;

    void tmpcleanup();
    bool dijontorcl(Rcl::Doc&);
    void collectIpathAndMT(Rcl::Doc&, string& ipath) const;
    bool dataToTempFile(const string& data, const string& mt, string& fn);
    void popHandler();
};

 
#endif /* _INTERNFILE_H_INCLUDED_ */
