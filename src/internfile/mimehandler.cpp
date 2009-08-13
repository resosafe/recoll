#ifndef lint
static char rcsid[] = "@(#$Id: mimehandler.cpp,v 1.25 2008-10-09 09:19:37 dockes Exp $ (C) 2004 J.F.Dockes";
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
#include "autoconfig.h"

#include <errno.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;

#include "mimehandler.h"
#include "debuglog.h"
#include "rclconfig.h"
#include "smallut.h"

#include "mh_exec.h"
#include "mh_html.h"
#include "mh_mail.h"
#include "mh_mbox.h"
#include "mh_text.h"
#include "mh_unknown.h"

// Pool of already known and created handlers. There can be several instance
// for a given mime type (think email attachment in email message)
static multimap<string, Dijon::Filter*>  o_handlers;

/** Create internal handler object appropriate for given mime type */
static Dijon::Filter *mhFactory(const string &mime)
{
    string lmime(mime);
    stringtolower(lmime);
    if ("text/plain" == lmime)
	return new MimeHandlerText(lmime);
    else if ("text/html" == lmime)
	return new MimeHandlerHtml(lmime);
    else if ("text/x-mail" == lmime)
	return new MimeHandlerMbox(lmime);
    else if ("message/rfc822" == lmime)
	return new MimeHandlerMail(lmime);
    else 
	return new MimeHandlerUnknown(lmime);
}

/**
 * Create a filter that executes an external program or script
 * A filter def can look like. 
 * exec someprog -v -t " h i j";charset= xx; mimetype=yy
 * We don't support ';' inside a quoted string for now. Can't see a use
 * for it
 */
MimeHandlerExec *mhExecFactory(RclConfig *cfg, const string& mtype, string& hs)
{
    list<string>semicolist;
    stringToTokens(hs, semicolist, ";");
    if (hs.size() < 1) {
	LOGERR(("mhExecFactory: bad filter def: [%s]\n", hs.c_str()));
	return 0;
    }
    string& cmd = *(semicolist.begin());

    list<string> toks;
    stringToStrings(cmd, toks);
    if (toks.size() < 2) {
	LOGERR(("mhExecFactory: bad config line for [%s]: [%s]\n", 
		mtype.c_str(), hs.c_str()));
	return 0;
    }

    MimeHandlerExec *h = new MimeHandlerExec(mtype.c_str());

    list<string>::iterator it;

    // toks size is at least 2, this has been checked by caller.
    it = toks.begin();
    it++;
    h->params.push_back(cfg->findFilter(*it++));
    h->params.insert(h->params.end(), it, toks.end());

    // Handle additional parameters
    it = semicolist.begin();
    it++;
    for (;it != semicolist.end(); it++) {
	string &line = *it;
	string::size_type eqpos = line.find("=");
	if (eqpos == string::npos)
	    continue;
	// Compute name and value, trim white space
	string nm, val;
	nm = line.substr(0, eqpos);
	trimstring(nm);
	val = line.substr(eqpos+1, string::npos);
	trimstring(val);
	if (!nm.compare("charset")) {
	    h->cfgCharset = stringtolower((const string&)val);
	} else if (!nm.compare("mimetype")) {
	    h->cfgMtype = stringtolower((const string&)val);
	}
    }

#if 0
    string sparams;
    for (it = h->params.begin(); it != h->params.end(); it++) {
	sparams += string("[") + *it + "] ";
    }
    LOGDEB(("mhExecFactory:mt [%s] cfgmt [%s] cfgcs [%s] params: [%s]\n",
	    mtype.c_str(), h->cfgMtype.c_str(), h->cfgCharset.c_str(),
	    sparams.c_str()));
#endif

    return h;
}

/* Return mime handler to pool */
void returnMimeHandler(Dijon::Filter *handler)
{
    typedef multimap<string, Dijon::Filter*>::value_type value_type;
    if (handler) {
	handler->clear();
	o_handlers.insert(value_type(handler->get_mime_type(), handler));
    }
}

/* Get handler/filter object for given mime type: */
Dijon::Filter *getMimeHandler(const string &mtype, RclConfig *cfg, 
			      bool filtertypes)
{
    if (mtype.empty())
	return false;

    // Do we already have one ?
    map<string, Dijon::Filter *>::iterator it = o_handlers.find(mtype);
    if (it != o_handlers.end()) {
	Dijon::Filter *h = it->second;
	o_handlers.erase(it);
	LOGDEB2(("getMimeHandler: found in cache\n"));
	return h;
    }
    
    // Get handler definition for mime type
    string hs;
    hs = cfg->getMimeHandlerDef(mtype, filtertypes);

    if (!hs.empty()) {
	// Break definition into type and name 
	list<string> toks;
	stringToStrings(hs, toks);
	if (toks.empty()) {
	    LOGERR(("getMimeHandler: bad mimeconf line for %s\n", 
		    mtype.c_str()));
	    return 0;
	}

	// Retrieve handler function according to type
	list<string>::iterator it = toks.begin();
	if (!stringlowercmp("internal", *it)) {
	    return mhFactory(mtype);
	} else if (!stringlowercmp("dll", *it)) {
	} else if (!stringlowercmp("exec", *it)) {
	    if (toks.size() < 2) {
		LOGERR(("getMimeHandler: bad line for %s: %s\n", 
			mtype.c_str(), hs.c_str()));
		return 0;
	    }
	    return mhExecFactory(cfg, mtype, hs);
	}
    }

    // We are supposed to get here if there was no specific error, but
    // there is no identified mime type, or no handler
    // associated. These files are either ignored or their name is
    // indexed, depending on configuration
    bool indexunknown = false;
    cfg->getConfParam("indexallfilenames", &indexunknown);
    if (indexunknown) {
	LOGDEB(("getMimeHandler: returning MimeHandlerUnknown\n"));
	return new MimeHandlerUnknown("application/octet-stream");
    } else {
	return 0;
    }
}


/// Can this mime type be interned (according to config) ?
bool canIntern(const std::string mtype, RclConfig *cfg)
{
    if (mtype.empty())
	return false;
    string hs = cfg->getMimeHandlerDef(mtype);
    if (hs.empty())
	return false;
    return true;
}
