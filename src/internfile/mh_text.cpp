#ifndef lint
static char rcsid[] = "@(#$Id: mh_text.cpp,v 1.2.2.1 2005-12-16 10:06:12 dockes Exp $ (C) 2005 J.F.Dockes";
#endif

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

// Process a plain text file
MimeHandler::Status MimeHandlerText::mkDoc(RclConfig *conf, const string &fn, 
			     const string &mtype, Rcl::Doc &docout, string&)
{
    string otext;
    if (!file_to_string(fn, otext))
	return MimeHandler::MHError;
	
    // Try to guess charset, then convert to utf-8, and fill document
    // fields The charset guesser really doesnt work well in general
    // and should be avoided (especially for short documents)
    string charset;
    if (conf->getGuessCharset()) {
	charset = csguess(otext, conf->getDefCharset());
    } else
	charset = conf->getDefCharset();
    string utf8;
    LOGDEB1(("textPlainToDoc: transcod from %s to %s\n", charset.c_str(), "UTF-8"));

    if (!transcode(otext, utf8, charset, "UTF-8")) {
	cerr << "textPlainToDoc: transcode failed: charset '" << charset
	     << "' to UTF-8: "<< utf8 << endl;
	otext.erase();
	return MimeHandler::MHError;
    }

    Rcl::Doc out;
    out.origcharset = charset;
    out.text = utf8;
    docout = out;
    return MimeHandler::MHDone;
}
