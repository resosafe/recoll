#ifndef lint
static char rcsid[] = "@(#$Id: mh_mail.cpp,v 1.20.4.1 2006-12-07 07:05:36 dockes Exp $ (C) 2005 J.F.Dockes";
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

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <regex.h>

#include <map>
#include <sstream>

#include "mimehandler.h"
#include "debuglog.h"
#include "csguess.h"
#include "readfile.h"
#include "transcode.h"
#include "mimeparse.h"
#include "indextext.h"
#include "mh_mail.h"
#include "debuglog.h"
#include "smallut.h"
#include "mimeparse.h"
#include "mh_html.h"

// binc imap mime definitions
#include "mime.h"

#ifndef NO_NAMESPACES
using namespace std;
#endif /* NO_NAMESPACES */

static const int maxdepth = 20;

MimeHandlerMail::~MimeHandlerMail()
{
    if (m_vfp) {
	fclose((FILE *)m_vfp);
	m_vfp = 0;
    }
}

// We are called for two different file types: mbox-type folders
// holding multiple messages, and maildir-type files with one message
// ipath is non empty only when we are called for retrieving a single message
// for preview. It is always empty during indexing, and we fill it up with 
// the message number for the returned doc
MimeHandler::Status 
MimeHandlerMail::mkDoc(RclConfig *cnf, const string &fn, 
			const string &mtype, Rcl::Doc &docout, string& ipath)
{
    LOGDEB2(("MimeHandlerMail::mkDoc: %s [%s]\n", mtype.c_str(), fn.c_str()));
    m_conf = cnf;

    if (!stringlowercmp("message/rfc822", mtype)) {
	ipath = "";
	int fd;
	if ((fd = open(fn.c_str(), 0)) < 0) {
	    LOGERR(("MimeHandlerMail::mkDoc: open(%s) errno %d\n",
		    fn.c_str(), errno));
	    return MimeHandler::MHError;
	}
	Binc::MimeDocument doc;
	doc.parseFull(fd);
	if (!doc.isHeaderParsed() && !doc.isAllParsed()) {
	    LOGERR(("MimeHandlerMail::mkDoc: mime parse error for %s\n",
		    fn.c_str()));
	    return MimeHandler::MHError;
	}
	MimeHandler::Status ret = processMsg(docout, doc, 0);
	close(fd);
	return ret;
    } else  if (!stringlowercmp("text/x-mail", mtype)) {
	return processmbox(fn, docout, ipath);
    } else // hu ho
	return MimeHandler::MHError;
}

static const  char *frompat = "^From .* [1-2][0-9][0-9][0-9]\n$";
static regex_t fromregex;
static bool regcompiled;

MimeHandler::Status 
MimeHandlerMail::processmbox(const string &fn, Rcl::Doc &docout, string& ipath)
{
    int mtarg = 0;
    if (ipath != "") {
	sscanf(ipath.c_str(), "%d", &mtarg);
    }
    LOGDEB2(("MimeHandlerMail::processmbox: fn %s, mtarg %d\n", fn.c_str(),
	    mtarg));

    FILE *fp;
    // Open the file on first call, then save/reuse the file pointer
    if (!m_vfp) {
	fp = fopen(fn.c_str(), "r");
	if (fp == 0) {
	    LOGERR(("MimeHandlerMail::processmbox: error opening %s\n", 
		    fn.c_str()));
	    return MimeHandler::MHError;
	}
	m_vfp = fp;
    } else {
	fp = (FILE *)m_vfp;
    }
    if (!regcompiled) {
	regcomp(&fromregex, frompat, REG_NOSUB);
	regcompiled = true;
    }

    // If we are called to retrieve a specific message, seek to bof
    // (then scan up to the message). This is for the case where the
    // same object is reused to fetch several messages (else the fp is
    // just opened no need for a seek).  We could also check if the
    // current message number is lower than the requested one and
    // avoid rereading the whole thing in this case. But I'm not sure
    // we're ever used in this way (multiple retrieves on same
    // object).  So:
    if (mtarg > 0) {
	fseek(fp, 0, SEEK_SET);
	m_msgnum = 0;
    }

    off_t start, end;
    bool iseof = false;
    bool hademptyline = true;
    string msgtxt;
    do  {
	// Look for next 'From ' Line, start of message. Set start to
	// line after this
	char line[501];
	for (;;) {
	    if (!fgets(line, 500, fp)) {
		// Eof hit while looking for 'From ' -> file done. We'd need
		// another return code here
		return MimeHandler::MHError;
	    }
	    if (line[0] == '\n') {
		hademptyline = true;
		continue;
	    }
	    if (hademptyline && !regexec(&fromregex, line, 0, 0, 0)) {
		start = ftello(fp);
		m_msgnum++;
		break;
	    }
	    hademptyline = false;
	}

	// Look for next 'From ' line or eof, end of message.
	for (;;) {
	    end = ftello(fp);
	    if (!fgets(line, 500, fp)) {
		if (ferror(fp) || feof(fp))
		    iseof = true;
		break;
	    }
	    if (hademptyline && !regexec(&fromregex, line, 0, 0, 0)) {
		break;
	    }
	    if (mtarg <= 0 || m_msgnum == mtarg) {
		msgtxt += line;
	    }
	    if (line[0] == '\n') {
		hademptyline = true;
	    } else {
		hademptyline = false;
	    }
	}
	fseek(fp, end, SEEK_SET);
    } while (mtarg > 0 && m_msgnum < mtarg);

    stringstream s(msgtxt);
    LOGDEB2(("Message text: [%s]\n", msgtxt.c_str()));
    Binc::MimeDocument doc;
    doc.parseFull(s);
    if (!doc.isHeaderParsed() && !doc.isAllParsed()) {
	LOGERR(("MimeHandlerMail::processMbox: mime parse error for %s\n",
		fn.c_str()));
	return MimeHandler::MHError;
    }

    MimeHandler::Status ret = processMsg(docout, doc, 0);

    if (ret == MimeHandler::MHError)
	return ret;
    char buf[20];
    sprintf(buf, "%d", m_msgnum);
    ipath = buf;
    return iseof ? MimeHandler::MHDone : 
	(mtarg > 0) ? MimeHandler::MHDone : MimeHandler::MHAgain;
}


// Transform a single message into a document. The subject becomes the
// title, and any simple body part with a content-type of text or html
// and content-disposition inline gets concatenated as text.
// 
// If depth is not zero, we're called recursively for an
// message/rfc822 part and we must not touch the doc fields except the
// text
MimeHandler::Status 
MimeHandlerMail::processMsg(Rcl::Doc &docout, Binc::MimePart& doc, 
			    int depth)
{
    LOGDEB2(("MimeHandlerMail::processMsg: depth %d\n", depth));
    if (depth++ >= maxdepth) {
	// Have to stop somewhere
	LOGDEB(("MimeHandlerMail::processMsg: maxdepth %d exceeded\n", 
		maxdepth));
	return MimeHandler::MHDone;
    }
	
    // Handle some headers. 
    Binc::HeaderItem hi;
    string transcoded;
    if (doc.h.getFirstHeader("From", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	docout.text += string("From: ") + transcoded + string("\n");
    }
    if (doc.h.getFirstHeader("To", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	docout.text += string("To: ") + transcoded + string("\n");
    }
    if (doc.h.getFirstHeader("Date", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (depth == 1) {
	    time_t t = rfc2822DateToUxTime(transcoded);
	    if (t != (time_t)-1) {
		char ascuxtime[100];
		sprintf(ascuxtime, "%ld", (long)t);
		docout.dmtime = ascuxtime;
	    } else {
		// Leave mtime field alone, ftime will be used instead.
		LOGDEB(("rfc2822Date...: failed: [%s]\n", transcoded.c_str()));
	    }
	}
	docout.text += string("Date: ") + transcoded + string("\n");
    }
    if (doc.h.getFirstHeader("Subject", hi)) {
	rfc2047_decode(hi.getValue(), transcoded);
	if (depth == 1)
	    docout.title = transcoded;
	docout.text += string("Subject: ") + transcoded + string("\n");
    }
    docout.text += '\n';

    LOGDEB2(("MimeHandlerMail::processMsg:ismultipart %d mime subtype '%s'\n",
	    doc.isMultipart(), doc.getSubType().c_str()));
    walkmime(docout, doc, depth);

    LOGDEB2(("MimeHandlerMail::processMsg:text:[%s]\n", docout.text.c_str()));
    return MimeHandler::MHDone;
}

// Recursively walk the message mime parts and concatenate all the
// inline html or text that we find anywhere.  
//
// RFC2046 reminder: 
// Top level media types: 
//      Simple:    text, image, audio, video, application, 
//      Composite: multipart, message.
// 
// multipart can be mixed, alternative, parallel, digest.
// message/rfc822 may also be of interest.

void MimeHandlerMail::walkmime(Rcl::Doc& docout, Binc::MimePart& doc,int depth)
{
    LOGDEB2(("MimeHandlerMail::walkmime: depth %d\n", depth));
    if (depth++ >= maxdepth) {
	LOGINFO(("walkmime: max depth (%d) exceeded\n", maxdepth));
	return;
    }

    string &out = docout.text;

    if (doc.isMultipart()) {
	LOGDEB2(("walkmime: ismultipart %d subtype '%s'\n", 
		doc.isMultipart(), doc.getSubType().c_str()));
	// We only handle alternative, related and mixed (no digests). 
	std::vector<Binc::MimePart>::iterator it;

	if (!stringicmp("mixed", doc.getSubType()) || 
	    !stringicmp("related", doc.getSubType())) {
	    // Multipart mixed and related:  process each part.
	    for (it = doc.members.begin(); it != doc.members.end();it++) {
		walkmime(docout, *it, depth);
	    }

	} else if (!stringicmp("alternative", doc.getSubType())) {
	    // Multipart/alternative: look for a text/plain part, then html.
	    // Process if found
	    std::vector<Binc::MimePart>::iterator ittxt, ithtml;
	    ittxt = ithtml = doc.members.end();
	    int i = 1;
	    for (it = doc.members.begin(); it != doc.members.end();it++, i++) {
		// Get and parse content-type header
		Binc::HeaderItem hi;
		if (!it->h.getFirstHeader("Content-Type", hi)) {
		    LOGDEB(("No content-type header for part %d\n", i));
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
	    if (ittxt != doc.members.end()) {
		LOGDEB2(("walkmime: alternative: chose text/plain part\n"))
		walkmime(docout, *ittxt, depth);
	    } else if (ithtml != doc.members.end()) {
		LOGDEB2(("walkmime: alternative: chose text/html part\n"))
		walkmime(docout, *ithtml, depth);
	    }
	}
	return;
    } 
    
    // Part is not multipart: it must be either simple or message. Take
    // a look at interesting headers and a possible filename parameter

    // Get and parse content-type header.
    Binc::HeaderItem hi;
    string ctt = "text/plain";
    if (doc.h.getFirstHeader("Content-Type", hi)) {
	ctt = hi.getValue();
    }
    LOGDEB2(("walkmime:content-type: %s\n", ctt.c_str()));
    MimeHeaderValue content_type;
    parseMimeHeaderValue(ctt, content_type);
	    
    // Get and parse Content-Disposition header
    string ctd = "inline";
    if (doc.h.getFirstHeader("Content-Disposition", hi)) {
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

    if (doc.isMessageRFC822()) {
	LOGDEB2(("walkmime: message/RFC822 part\n"));
	
	// The first part is the already parsed message.  Call
	// processMsg instead of walkmime so that mail headers get
	// printed. The depth will tell it what to do
	if (doc.members.empty()) {
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
	processMsg(docout, doc.members[0], depth);
	return;
    }

    // "Simple" part. 
    LOGDEB2(("walkmime: simple  part\n"));

    // If the Content-Disposition is not inline, we treat it as
    // attachment, as per rfc2183. We don't process attachments
    // for now, except for indexing/displaying the file name
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
	// We're done with this part
	return;
    }

    // We are dealing with an inline part of text/plain or text/html type

    // Normally the default charset is us-ascii. But it happens that
    // 8 bit chars exist in a message that is stated as us-ascii. Ie the 
    // mailer used by yahoo support ('KANA') does this. We could convert 
    // to iso-8859 only if the transfer-encoding is 8 bit, or test for
    // actual 8 bit chars, but what the heck, le'ts use 8859-1 as default
    string charset = "iso-8859-1";
    it = content_type.params.find(string("charset"));
    if (it != content_type.params.end())
	charset = it->second;
    if (charset.empty() || 
	!stringlowercmp("us-ascii", charset) || 
	!stringlowercmp("default", charset) || 
	!stringlowercmp("x-user-defined", charset) || 
	!stringlowercmp("x-unknown", charset) || 
	!stringlowercmp("unknown", charset) ) {
	charset = "iso-8859-1";
    }

    // Content transfer encoding
    string cte = "7bit";
    if (doc.h.getFirstHeader("Content-Transfer-Encoding", hi)) {
	cte = hi.getValue();
    } 

    LOGDEB2(("walkmime: final: body start offset %d, length %d\n", 
	     doc.getBodyStartOffset(), doc.getBodyLength()));
    string body;
    doc.getBody(body, 0, doc.bodylength);

    // Decode according to content transfer encoding
    if (!stringlowercmp("quoted-printable", cte)) {
	string decoded;
	if (!qp_decode(body, decoded)) {
	    LOGERR(("walkmime: quoted-printable decoding failed !\n"));
	    return;
	}
	body = decoded;
    } else if (!stringlowercmp("base64", cte)) {
	string decoded;
	if (!base64_decode(body, decoded)) {
	    LOGERR(("walkmime: base64 decoding failed !\n"));
#if 0
	    FILE *fp = fopen("/tmp/recoll_decodefail", "w");
	    if (fp) {
		fprintf(fp, "%s", body.c_str());
		fclose(fp);
	    }
#endif
	    return;
	}
	body = decoded;
    }

    // Handle html stripping and transcoding to utf8
    string utf8;
    if (!stringlowercmp("text/html", content_type.value)) {
	MimeHandlerHtml mh;
	Rcl::Doc hdoc;
	mh.charsethint = charset;
	mh.mkDoc(m_conf, "", body, content_type.value,  hdoc);
	utf8 = hdoc.text;
    } else {
	// Transcode to utf-8 
	if (!transcode(body, utf8, charset, "UTF-8")) {
	    LOGERR(("walkmime: transcode failed from cs '%s' to UTF-8\n",
		    charset.c_str()));
	    utf8 = body;
	}
    }

    out += utf8;
    if (out.length() && out[out.length()-1] != '\n')
	out += '\n';
    
    LOGDEB2(("walkmime: out now: [%s]\n", out.c_str()));
}
