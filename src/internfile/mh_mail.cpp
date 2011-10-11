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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <cstdlib>

#include <map>
#include <sstream>

#include "mimehandler.h"
#include "readfile.h"
#include "transcode.h"
#include "mimeparse.h"
#include "mh_mail.h"
#include "debuglog.h"
#include "smallut.h"
#include "mh_html.h"
#include "rclconfig.h"
#include "mimetype.h"
#include "md5.h"

// binc imap mime definitions
#include "mime.h"

using namespace std;

static const int maxdepth = 20;
static const string cstr_mimetype = "mimetype";
static const string cstr_content = "content";
static const string cstr_author = "author";
static const string cstr_recipient = "recipient";
static const string cstr_modificationdate = "modificationdate";
static const string cstr_title = "title";
static const string cstr_msgid = "msgid";
static const string cstr_abstract = "abstract";

static const string cstr_newline = "\n";

MimeHandlerMail::MimeHandlerMail(RclConfig *cnf, const string &mt) 
    : RecollFilter(cnf, mt), m_bincdoc(0), m_fd(-1), m_stream(0), m_idx(-1)
{

    // Look for additional headers to be processed as per config:
    list<string> hdrnames = 
        m_config->getFieldSectNames("mail");
    if (hdrnames.empty())
        return;
    for (list<string>::const_iterator it = hdrnames.begin();
         it != hdrnames.end(); it++) {
        (void)m_config->getFieldConfParam(*it, "mail", m_addProcdHdrs[*it]);
    }
}

MimeHandlerMail::~MimeHandlerMail() 
{
    clear();
}
void MimeHandlerMail::clear()
{
    delete m_bincdoc; m_bincdoc = 0;
    if (m_fd >= 0) {
	close(m_fd);
	m_fd = -1;
    }
    delete m_stream; m_stream = 0;
    m_idx = -1;
    m_subject.erase();
    for (vector<MHMailAttach*>::iterator it = m_attachments.begin(); 
	 it != m_attachments.end(); it++) {
	delete *it;
    }
    m_attachments.clear();
    RecollFilter::clear();
}

bool MimeHandlerMail::set_document_file(const string &fn)
{
    LOGDEB(("MimeHandlerMail::set_document_file(%s)\n", fn.c_str()));
    RecollFilter::set_document_file(fn);
    if (m_fd >= 0) {
	close(m_fd);
	m_fd = -1;
    }

    // Yes, we read the file twice. It would be possible in theory to add
    // the md5 computation to the mime analysis, but ...
    string md5, xmd5, reason;
    if (MD5File(fn, md5, &reason)) {
	m_metaData["md5"] = MD5HexPrint(md5, xmd5);
    } else {
	LOGERR(("MimeHandlerMail: cant compute md5 for [%s]: %s\n", fn.c_str(),
		reason.c_str()));
    }

    m_fd = open(fn.c_str(), 0);
    if (m_fd < 0) {
	LOGERR(("MimeHandlerMail::set_document_file: open(%s) errno %d\n",
		fn.c_str(), errno));
	return false;
    }
    delete m_bincdoc;
    m_bincdoc = new Binc::MimeDocument;
    m_bincdoc->parseFull(m_fd);
    if (!m_bincdoc->isHeaderParsed() && !m_bincdoc->isAllParsed()) {
	LOGERR(("MimeHandlerMail::mkDoc: mime parse error for %s\n",
		fn.c_str()));
	return false;
    }
    m_havedoc = true;
    return true;
}

bool MimeHandlerMail::set_document_string(const string &msgtxt)
{
    LOGDEB1(("MimeHandlerMail::set_document_string\n"));
    LOGDEB2(("Message text: [%s]\n", msgtxt.c_str()));
    delete m_stream;

    string md5, xmd5;
    MD5String(msgtxt, md5);
    m_metaData["md5"] = MD5HexPrint(md5, xmd5);

    m_stream = new stringstream(msgtxt);
    delete m_bincdoc;
    m_bincdoc = new Binc::MimeDocument;
    m_bincdoc->parseFull(*m_stream);
    if (!m_bincdoc->isHeaderParsed() && !m_bincdoc->isAllParsed()) {
	LOGERR(("MimeHandlerMail::set_document_string: mime parse error\n"));
	return false;
    }
    m_havedoc = true;
    return true;
}

bool MimeHandlerMail::skip_to_document(const string& ipath) 
{
    LOGDEB(("MimeHandlerMail::skip_to_document(%s)\n", ipath.c_str()));
    if (m_idx == -1) {
	// No decoding done yet. If ipath is null need do nothing
	if (ipath.empty() || ipath == "-1")
	    return true;
	// ipath points to attachment: need to decode message
	if (!next_document()) {
	    LOGERR(("MimeHandlerMail::skip_to_doc: next_document failed\n"));
	    return false;
	}
    }
    m_idx = atoi(ipath.c_str());
    return true;
}

bool MimeHandlerMail::next_document()
{
    LOGDEB(("MimeHandlerMail::next_document m_idx %d m_havedoc %d\n",
	    m_idx, m_havedoc));
    if (!m_havedoc)
	return false;
    bool res = false;

    if (m_idx == -1) {
	m_metaData[cstr_mimetype] = "text/plain";
	res = processMsg(m_bincdoc, 0);
	LOGDEB1(("MimeHandlerMail::next_document: mimetype %s\n",
		m_metaData[cstr_mimetype].c_str()));
        const string& txt = m_metaData[cstr_content];
        if (m_startoftext < txt.size())
            m_metaData[cstr_abstract] = 
                truncate_to_word(txt.substr(m_startoftext), 250);
    } else {
        m_metaData[cstr_abstract] = "";
	res = processAttach();
    }
    m_idx++;
    m_havedoc = m_idx < (int)m_attachments.size();
    if (!m_havedoc) {
        m_reason = "Subdocument index too high";
    }
    return res;
}

// Decode according to content transfer encoding. May actually do nothing,
// which will be indicated by the *respp argument pointing to the original 
// text on exit
static bool decodeBody(const string& cte, // Content transfer encoding
		       const string& body, // Source text
		       string& decoded,   // Decoded text if actual decoding
		       const string** respp // Decoding Indicator 
		       )
{
    // By default, there is no encoding (7bit,8bit,raw). Also in case of 
    // decoding error
    *respp = &body;

    if (!stringlowercmp("quoted-printable", cte)) {
	if (!qp_decode(body, decoded)) {
	    LOGERR(("decodeBody: quoted-printable decoding failed !\n"));
	    LOGDEB(("      Body: \n%s\n", body.c_str()));
	    return false;
	}
	*respp = &decoded;
    } else if (!stringlowercmp("base64", cte)) {
	if (!base64_decode(body, decoded)) {
	    // base64 encoding errors are actually relatively common
	    LOGERR(("decodeBody: base64 decoding failed !\n"));
	    LOGDEB(("      Body: \n%s\n", body.c_str()));
	    return false;
	}
	*respp = &decoded;
    }
    return true;
}

bool MimeHandlerMail::processAttach()
{
    LOGDEB(("MimeHandlerMail::processAttach() m_idx %d\n", m_idx));
    if (!m_havedoc)
	return false;
    if (m_idx >= (int)m_attachments.size()) {
	m_havedoc = false;
	return false;
    }
    MHMailAttach *att = m_attachments[m_idx];

    m_metaData[cstr_mimetype] = att->m_contentType;
    m_metaData["charset"] = att->m_charset;
    m_metaData["filename"] = att->m_filename;
    // Change the title to something helpul
    m_metaData[cstr_title] = att->m_filename + "  (" + m_subject + ")";
    LOGDEB1(("  processAttach:ct [%s] cs [%s] fn [%s]\n", 
	    att->m_contentType.c_str(),
	    att->m_charset.c_str(),
	    att->m_filename.c_str()));

    m_metaData[cstr_content] = string();
    string& body = m_metaData[cstr_content];
    att->m_part->getBody(body, 0, att->m_part->bodylength);
    string decoded;
    const string *bdp;
    if (!decodeBody(att->m_contentTransferEncoding, body, decoded, &bdp)) {
	return false;
    }
    if (bdp != &body)
	body = decoded;

    // Special case for text/plain content. Internfile should deal
    // with this but it expects text/plain to be utf-8 already, so we
    // handle the transcoding if needed
    if (m_metaData[cstr_mimetype] == "text/plain" && 
	stringicmp(m_metaData["charset"], "UTF-8")) {
	string utf8;
	if (!transcode(body, utf8, m_metaData["charset"], "UTF-8")) {
	    LOGERR(("  processAttach: transcode to utf-8 failed "
		    "for charset [%s]\n", m_metaData["charset"].c_str()));
	    // Just let it through and hope for the best...
	} else {
	    body = utf8;
	}
    }

    // Special case for application/octet-stream: try to better
    // identify content, using file name if set
    if (m_metaData[cstr_mimetype] == "application/octet-stream" &&
	!m_metaData["filename"].empty()) {
	string mt = mimetype(m_metaData["filename"], 0,	
			     m_config, false);
	if (!mt.empty()) 
	    m_metaData[cstr_mimetype] = mt;
    }

    // Ipath
    char nbuf[20];
    sprintf(nbuf, "%d", m_idx);
    m_metaData["ipath"] = nbuf;

    return true;
}

// Transform a single message into a document. The subject becomes the
// title, and any simple body part with a content-type of text or html
// and content-disposition inline gets concatenated as text.
// 
// If depth is not zero, we're called recursively for an
// message/rfc822 part and we must not touch the doc fields except the
// text
bool MimeHandlerMail::processMsg(Binc::MimePart *doc, int depth)
{
    LOGDEB2(("MimeHandlerMail::processMsg: depth %d\n", depth));
    if (depth++ >= maxdepth) {
	// Have to stop somewhere
	LOGINFO(("MimeHandlerMail::processMsg: maxdepth %d exceeded\n", 
		maxdepth));
	// Return true anyway, better to index partially than not at all
	return true;
    }
	
    // Handle some headers. 
    string& text = m_metaData[cstr_content];
    Binc::HeaderItem hi;
    string transcoded;
    if (doc->h.getFirstHeader("From", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (preview())
	    text += string("From: ");
	text += transcoded + cstr_newline;
	if (depth == 1) {
	    m_metaData[cstr_author] = transcoded;
	}
    }
    if (doc->h.getFirstHeader("To", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (preview())
	    text += string("To: ");
	text += transcoded + cstr_newline;
	if (depth == 1) {
	    m_metaData[cstr_recipient] = transcoded;
	}
    }
    if (doc->h.getFirstHeader("Cc", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (preview())
	    text += string("Cc: ");
	text += transcoded + cstr_newline;
	if (depth == 1) {
	    m_metaData[cstr_recipient] += " " + transcoded;
	}
    }
    if (doc->h.getFirstHeader("Message-Id", hi)) {
	if (depth == 1) {
	    m_metaData[cstr_msgid] =  hi.getValue();
            trimstring(m_metaData[cstr_msgid], "<>");
	}
    }
    if (doc->h.getFirstHeader("Date", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (depth == 1) {
	    time_t t = rfc2822DateToUxTime(transcoded);
	    if (t != (time_t)-1) {
		char ascuxtime[100];
		sprintf(ascuxtime, "%ld", (long)t);
		m_metaData[cstr_modificationdate] = ascuxtime;
	    } else {
		// Leave mtime field alone, ftime will be used instead.
		LOGDEB(("rfc2822Date...: failed: [%s]\n", transcoded.c_str()));
	    }
	}
	if (preview())
	    text += string("Date: ");
	text += transcoded + cstr_newline;
    }
    if (doc->h.getFirstHeader("Subject", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (depth == 1) {
	    m_metaData[cstr_title] = transcoded;
	    m_subject = transcoded;
	}
	if (preview())
	    text += string("Subject: ");
	text += transcoded + cstr_newline;
    }

    // Check for the presence of configured additional headers and possibly
    // add them to the metadata (with appropriate field name).
    if (!m_addProcdHdrs.empty()) {
        for (map<string, string>::const_iterator it = m_addProcdHdrs.begin();
             it != m_addProcdHdrs.end(); it++) {
            if (!it->second.empty()) {
                string hval;
                if (doc->h.getFirstHeader(it->first, hi)) {
                    m_metaData[it->second] = hi.getValue();
                }
            }
        }
    }

    text += '\n';
    m_startoftext = text.size();
    LOGDEB2(("MimeHandlerMail::processMsg:ismultipart %d mime subtype '%s'\n",
	    doc->isMultipart(), doc->getSubType().c_str()));
    walkmime(doc, depth);

    LOGDEB2(("MimeHandlerMail::processMsg:text:[%s]\n", 
	    m_metaData[cstr_content].c_str()));
    return true;
}

// Recursively walk the message mime parts and concatenate all the
// inline html or text that we find anywhere.  
//
// RFC2046 reminder: 
// Top level media types: 
//      Simple:    text, image, audio, video, application, 
//      Composite: multipart, message.
// 
// multipart can be mixed, signed, alternative, parallel, digest.
// message/rfc822 may also be of interest.
void MimeHandlerMail::walkmime(Binc::MimePart* doc, int depth)
{
    LOGDEB2(("MimeHandlerMail::walkmime: depth %d\n", depth));
    if (depth++ >= maxdepth) {
	LOGINFO(("walkmime: max depth (%d) exceeded\n", maxdepth));
	return;
    }

    string& out = m_metaData[cstr_content];

    if (doc->isMultipart()) {
	LOGDEB2(("walkmime: ismultipart %d subtype '%s'\n", 
		doc->isMultipart(), doc->getSubType().c_str()));
	// We only handle alternative, related and mixed (no digests). 
	std::vector<Binc::MimePart>::iterator it;

	if (!stringicmp("mixed", doc->getSubType()) || 
	    !stringicmp("signed", doc->getSubType()) ||
	    !stringicmp("related", doc->getSubType())) {
	    // Multipart mixed and related:  process each part.
	    for (it = doc->members.begin(); it != doc->members.end();it++) {
		walkmime(&(*it), depth);
	    }

	} else if (!stringicmp("alternative", doc->getSubType())) {
	    // Multipart/alternative: look for a text/plain part, then html.
	    // Process if found
	    std::vector<Binc::MimePart>::iterator ittxt, ithtml;
	    ittxt = ithtml = doc->members.end();
	    int i = 1;
	    for (it = doc->members.begin(); 
		 it != doc->members.end(); it++, i++) {
		// Get and parse content-type header
		Binc::HeaderItem hi;
		if (!it->h.getFirstHeader("Content-Type", hi)) {
		    LOGDEB(("walkmime:no ctent-type header for part %d\n", i));
		    continue;
		}
		MimeHeaderValue content_type;
		parseMimeHeaderValue(hi.getValue(), content_type);
		LOGDEB2(("walkmime: C-type: %s\n",content_type.value.c_str()));
		if (!stringlowercmp("text/plain", content_type.value))
		    ittxt = it;
		else if (!stringlowercmp("text/html", content_type.value)) 
		    ithtml = it;
	    }
	    if (ittxt != doc->members.end()) {
		LOGDEB2(("walkmime: alternative: chose text/plain part\n"))
		    walkmime(&(*ittxt), depth);
	    } else if (ithtml != doc->members.end()) {
		LOGDEB2(("walkmime: alternative: chose text/html part\n"))
		    walkmime(&(*ithtml), depth);
	    }
	}
	return;
    } 
    
    // Part is not multipart: it must be either simple or message. Take
    // a look at interesting headers and a possible filename parameter

    // Get and parse content-type header.
    Binc::HeaderItem hi;
    string ctt = "text/plain";
    if (doc->h.getFirstHeader("Content-Type", hi)) {
	ctt = hi.getValue();
    }
    LOGDEB2(("walkmime:content-type: %s\n", ctt.c_str()));
    MimeHeaderValue content_type;
    parseMimeHeaderValue(ctt, content_type);
	    
    // Get and parse Content-Disposition header
    string ctd = "inline";
    if (doc->h.getFirstHeader("Content-Disposition", hi)) {
	ctd = hi.getValue();
    }
    MimeHeaderValue content_disposition;
    parseMimeHeaderValue(ctd, content_disposition);
    LOGDEB2(("Content_disposition:[%s]\n", content_disposition.value.c_str()));
    string dispindic;
    if (stringlowercmp("inline", content_disposition.value))
	dispindic = "Attachment";
    else 
	dispindic = "Inline";

    // See if we have a filename.
    string filename;
    map<string,string>::const_iterator it;
    it = content_disposition.params.find(string("filename"));
    if (it != content_disposition.params.end())
	filename = it->second;

    if (doc->isMessageRFC822()) {
	LOGDEB2(("walkmime: message/RFC822 part\n"));
	
	// The first part is the already parsed message.  Call
	// processMsg instead of walkmime so that mail headers get
	// printed. The depth will tell it what to do
	if (doc->members.empty()) {
	    //??
	    return;
	}
	out += "\n";
	if (m_forPreview)
	    out += "[" + dispindic + " " + content_type.value + ": ";
	out += filename;
	if (m_forPreview)
	    out += "]";
	out += "\n\n";
	processMsg(&doc->members[0], depth);
	return;
    }

    // "Simple" part. 
    LOGDEB2(("walkmime: simple  part\n"));
    // Normally the default charset is us-ascii. But it happens that
    // 8 bit chars exist in a message that is stated as us-ascii. Ie the 
    // mailer used by yahoo support ('KANA') does this. We could convert 
    // to iso-8859 only if the transfer-encoding is 8 bit, or test for
    // actual 8 bit chars, but what the heck, le'ts use 8859-1 as default
    string charset;
    it = content_type.params.find(string("charset"));
    if (it != content_type.params.end())
	charset = it->second;
    if (charset.empty() || 
	!stringlowercmp("us-ascii", charset) || 
	!stringlowercmp("default", charset) || 
	!stringlowercmp("x-user-defined", charset) || 
	!stringlowercmp("x-unknown", charset) || 
	!stringlowercmp("unknown", charset) ) {
        m_config->getConfParam("maildefcharset", charset);
        if (charset.empty())
            charset = "iso-8859-1";
    }

    // Content transfer encoding
    string cte = "7bit";
    if (doc->h.getFirstHeader("Content-Transfer-Encoding", hi)) {
	cte = hi.getValue();
    } 

    // If the Content-Disposition is not inline, we treat it as
    // attachment, as per rfc2183. 
    // If it is inline but not text or html, same thing.
    if (stringlowercmp("inline", content_disposition.value) ||
	(stringlowercmp("text/plain", content_type.value) && 
	 stringlowercmp("text/html", content_type.value)) ) {
	if (!filename.empty()) {
	    out += "\n";
	    if (m_forPreview)
		out += "[" + dispindic + " " + content_type.value + ": ";
	    out += filename;
	    if (m_forPreview)
		out += "]";
	    out += "\n\n";
	}
	MHMailAttach *att = new MHMailAttach;
	if (att == 0) {
	    LOGERR(("Out of memory\n"));
	    return;
	}
	att->m_contentType = content_type.value;
	stringtolower(att->m_contentType);
	att->m_filename = filename;
	att->m_charset = charset;
	att->m_contentTransferEncoding = cte;
	att->m_part = doc;
	LOGDEB(("walkmime: attachmnt: ct [%s] cte [%s] cs [%s] fn [%s]\n", 
		att->m_contentType.c_str(),
		att->m_contentTransferEncoding.c_str(),
		att->m_charset.c_str(),
		filename.c_str()));
	m_attachments.push_back(att);
	return;
    }

    // We are dealing with an inline part of text/plain or text/html type


    LOGDEB2(("walkmime: final: body start offset %d, length %d\n", 
	     doc->getBodyStartOffset(), doc->getBodyLength()));
    string body;
    doc->getBody(body, 0, doc->bodylength);

    string decoded;
    const string *bdp;
    if (!decodeBody(cte, body, decoded, &bdp)) {
	LOGERR(("MimeHandlerMail::walkmime: failed decoding body\n"));
    }
    if (bdp != &body)
	body = decoded;

    // Handle html stripping and transcoding to utf8
    string utf8;
    const string *putf8 = 0;
    if (!stringlowercmp("text/html", content_type.value)) {
	MimeHandlerHtml mh(m_config, "text/html");
	mh.set_property(Dijon::Filter::OPERATING_MODE, 
			m_forPreview ? "view" : "index");
	mh.set_property(Dijon::Filter::DEFAULT_CHARSET, charset);
	mh.set_document_string(body);
	mh.next_document();
	map<string, string>::const_iterator it = 
	    mh.get_meta_data().find(cstr_content);
	if (it != mh.get_meta_data().end())
	    out += it->second;
    } else {
	// Transcode to utf-8 
	LOGDEB1(("walkmime: transcoding from %s to UTF-8\n", charset.c_str()));
	if (!transcode(body, utf8, charset, "UTF-8")) {
	    LOGERR(("walkmime: transcode failed from cs '%s' to UTF-8\n",
		    charset.c_str()));
	    out += body;
	} else {
	    out += utf8;
	}
    }

    if (out.length() && out[out.length()-1] != '\n')
	out += '\n';
    
    LOGDEB2(("walkmime: out now: [%s]\n", out.c_str()));
}
