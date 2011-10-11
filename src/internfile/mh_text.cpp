/* Copyright (C) 2005 J.F.Dockes 
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
#include "autoconfig.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include <iostream>
#include <string>
#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

#include "mh_text.h"
#include "csguess.h"
#include "debuglog.h"
#include "readfile.h"
#include "transcode.h"
#include "md5.h"
#include "rclconfig.h"

const int MB = 1024*1024;
const int KB = 1024;

// Process a plain text file
bool MimeHandlerText::set_document_file(const string &fn)
{
    LOGDEB(("MimeHandlerText::set_document_file: [%s]\n", fn.c_str()));

    RecollFilter::set_document_file(fn);
    m_fn = fn;

    // file size for oversize check
    struct stat st;
    if (stat(m_fn.c_str(), &st) < 0) {
        LOGERR(("MimeHandlerText::set_document_file: stat(%s) errno %d\n",
                m_fn.c_str(), errno));
        return false;
    }

    // Max file size parameter: texts over this size are not indexed
    int maxmbs = 20;
    m_config->getConfParam("textfilemaxmbs", &maxmbs);

    if (maxmbs == -1 || st.st_size / MB <= maxmbs) {
        // Text file page size: if set, we split text files into
        // multiple documents
        int ps = 1000;
        m_config->getConfParam("textfilepagekbs", &ps);
        if (ps != -1) {
            ps *= KB;
            m_paging = true;
        }
        m_pagesz = size_t(ps);
        string reason;
	LOGDEB(("calling file_to_string\n"));
        // file_to_string() takes pagesz == size_t(-1) to mean read all.
        if (!file_to_string(fn, m_text, 0, m_pagesz, &reason)) {
            LOGERR(("MimeHandlerText: can't read file: %s\n", reason.c_str()));
            return false;
        }
	LOGDEB(("file_to_string OK\n"));
        m_offs = m_text.length();
    }

    string md5, xmd5;
    MD5String(m_text, md5);
    m_metaData["md5"] = MD5HexPrint(md5, xmd5);
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::set_document_string(const string& otext)
{
    m_text = otext;
    string md5, xmd5;
    MD5String(m_text, md5);
    m_metaData["md5"] = MD5HexPrint(md5, xmd5);
    m_havedoc = true;
    return true;
}

bool MimeHandlerText::skip_to_document(const string& ipath)
{
    long long t;
    if (sscanf(ipath.c_str(), "%lld", &t) != 1) {
	LOGERR(("MimeHandlerText::skip_to_document: bad ipath offs [%s]\n",
		ipath.c_str()));
	return false;
    }
    m_offs = (off_t)t;
    readnext();
    return true;
}

bool MimeHandlerText::next_document()
{
    LOGDEB(("MimeHandlerText::next_document: m_havedoc %d\n", int(m_havedoc)));

    if (m_havedoc == false)
	return false;

    // We transcode even if defcharset is already utf-8: 
    // this validates the encoding.
    LOGDEB1(("MimeHandlerText::mkDoc: transcod from %s to utf-8\n", 
	     m_dfltInputCharset.c_str()));
    int ecnt;
    bool ret;
    string& itext = m_metaData["content"];
    if (!(ret=transcode(m_text, itext, m_dfltInputCharset, "UTF-8", &ecnt)) || 
	ecnt > int(itext.size() / 4)) {
	LOGERR(("MimeHandlerText::mkDoc: transcode to utf-8 failed "
		"for input charset [%s] ret %d ecnt %d\n", 
		m_dfltInputCharset.c_str(), ret, ecnt));
	itext.erase();
	return false;
    }
    m_metaData["origcharset"] = m_dfltInputCharset;
    m_metaData["charset"] = "utf-8";
    m_metaData["mimetype"] = "text/plain";

    // If text length is 0 (the file is empty or oversize), or we have
    // read all at once, we're done
    if (m_text.length() == 0 || !m_paging) {
        m_havedoc = false;
        return true;
    } else {
        // Paging: set ipath then read next chunk. 

        // Don't set ipath for the first chunk to avoid having 2
        // records for small files (one for the file, one for the
        // first chunk). This is a hack. The right thing to do would
        // be to use a different mtype for files over the page size,
        // and keep text/plain only for smaller files.
        char buf[30];
        sprintf(buf, "%lld", (long long)(m_offs - m_text.length()));
        if (m_offs - m_text.length() != 0)
            m_metaData["ipath"] = buf;
        readnext();
        return true;
    }
}

bool MimeHandlerText::readnext()
{
    string reason;
    m_text.erase();
    if (!file_to_string(m_fn, m_text, m_offs, m_pagesz, &reason)) {
        LOGERR(("MimeHandlerText: can't read file: %s\n", reason.c_str()));
        m_havedoc = false;
        return false;
    }
    if (m_text.length() == 0) {
        // EOF
        m_havedoc = false;
        return true;
    }

    // If possible try to adjust the chunk to end right after a line 
    // Don't do this for the last chunk
    if (m_text.length() == m_pagesz) {
        string::size_type pos = m_text.find_last_of("\n\r");
        if (pos != string::npos && pos != 0) {
            m_text.erase(pos);
        }
    }
    m_offs += m_text.length();
    return true;
}
