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

#include <list>

#include <QMessageBox>

#include "qxtconfirmationmessage.h"

#include "debuglog.h"
#include "fileudi.h"
#include "execmd.h"
#include "transcode.h"
#include "docseqhist.h"
#include "docseqdb.h"
#include "internfile.h"
#include "rclmain_w.h"
#include "rclzg.h"

using namespace std;

// Start native viewer or preview for input Doc. This is used to allow
// using recoll from another app (e.g. Unity Scope) to view embedded
// result docs (docs with an ipath). . We act as a proxy to extract
// the data and start a viewer.  The Url are encoded as
// file://path#ipath
void RclMain::viewUrl()
{
    if (m_urltoview.isEmpty() || !rcldb)
	return;

    QUrl qurl(m_urltoview);
    LOGDEB(("RclMain::viewUrl: Path [%s] fragment [%s]\n", 
	    (const char *)qurl.path().toLocal8Bit(),
	    (const char *)qurl.fragment().toLocal8Bit()));

    /* In theory, the url might not be for a file managed by the fs
       indexer so that the make_udi() call here would be
       wrong(). When/if this happens we'll have to hide this part
       inside internfile and have some url magic to indicate the
       appropriate indexer/identification scheme */
    string udi;
    make_udi((const char *)qurl.path().toLocal8Bit(),
	     (const char *)qurl.fragment().toLocal8Bit(), udi);
    
    Rcl::Doc doc;
    Rcl::Doc idxdoc; // idxdoc.idxi == 0 -> works with base index only
    if (!rcldb->getDoc(udi, idxdoc, doc) || doc.pc == -1)
	return;

    // StartNativeViewer needs a db source to call getEnclosing() on.
    Rcl::Query *query = new Rcl::Query(rcldb);
    DocSequenceDb *src = 
	new DocSequenceDb(RefCntr<Rcl::Query>(query), "", 
			  RefCntr<Rcl::SearchData>(new Rcl::SearchData));
    m_source = RefCntr<DocSequence>(src);


    // Start a native viewer if the mimetype has one defined, else a
    // preview.
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    string viewer = theconfig->getMimeViewerDef(doc.mimetype, apptag, 
						prefs.useDesktopOpen);
    if (viewer.empty()) {
	startPreview(doc);
    } else {
	hide();
	startNativeViewer(doc);
	// We have a problem here because xdg-open will exit
	// immediately after starting the command instead of waiting
	// for it, so we can't wait either and we don't know when we
	// can exit (deleting the temp file). As a bad workaround we
	// sleep some time then exit. The alternative would be to just
	// prevent the temp file deletion completely, leaving it
	// around forever. Better to let the user save a copy if he
	// wants I think.
	sleep(30);
	fileExit();
    }
}

/* Look for html browser. We make a special effort for html because it's
 * used for reading help. This is only used if the normal approach 
 * (xdg-open etc.) failed */
static bool lookForHtmlBrowser(string &exefile)
{
    static const char *htmlbrowserlist = 
	"opera google-chrome chromium-browser konqueror iceweasel firefox "
        "mozilla netscape epiphany";
    vector<string> blist;
    stringToTokens(htmlbrowserlist, blist, " ");

    const char *path = getenv("PATH");
    if (path == 0)
	path = "/bin:/usr/bin:/usr/bin/X11:/usr/X11R6/bin:/usr/local/bin";

    // Look for each browser 
    for (vector<string>::const_iterator bit = blist.begin(); 
	 bit != blist.end(); bit++) {
	if (ExecCmd::which(*bit, exefile, path)) 
	    return true;
    }
    exefile.clear();
    return false;
}

void RclMain::openWith(Rcl::Doc doc, string cmdspec)
{
    LOGDEB(("RclMain::openWith: %s\n", cmdspec.c_str()));

    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmdspec, lcmd)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Bad desktop app spec for %1: [%2]\n"
				"Please check the desktop file")
			     .arg(QString::fromUtf8(doc.mimetype.c_str()))
			     .arg(QString::fromLocal8Bit(cmdspec.c_str())));
	return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execname = lcmd.front();
    lcmd.erase(lcmd.begin());
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);

    // Try to keep the letters used more or less consistent with the reslist
    // paragraph format.
    map<string, string> subs;
    subs["F"] = fn;
    subs["f"] = fn;
    subs["U"] = url;
    subs["u"] = url;

    execViewer(subs, false, execname, lcmd, cmdspec, doc);
}

void RclMain::startNativeViewer(Rcl::Doc doc, int pagenum, QString term)
{
    string apptag;
    doc.getmeta(Rcl::Doc::keyapptg, &apptag);
    LOGDEB(("RclMain::startNativeViewer: mtype [%s] apptag [%s] page %d "
	    "term [%s] url [%s] ipath [%s]\n", 
	    doc.mimetype.c_str(), apptag.c_str(), pagenum, 
	    (const char *)(term.toUtf8()), doc.url.c_str(), doc.ipath.c_str()
	       ));

    // Look for appropriate viewer
    string cmdplusattr = theconfig->getMimeViewerDef(doc.mimetype, apptag, 
						     prefs.useDesktopOpen);
    if (cmdplusattr.empty()) {
	QMessageBox::warning(0, "Recoll", 
			     tr("No external viewer configured for mime type [")
			     + doc.mimetype.c_str() + "]");
	return;
    }

    // Separate command string and viewer attributes (if any)
    ConfSimple viewerattrs;
    string cmd;
    theconfig->valueSplitAttributes(cmdplusattr, cmd, viewerattrs);
    bool ignoreipath = false;
    if (viewerattrs.get("ignoreipath", cmdplusattr))
        ignoreipath = stringToBool(cmdplusattr);

    // Split the command line
    vector<string> lcmd;
    if (!stringToStrings(cmd, lcmd)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Bad viewer command line for %1: [%2]\n"
				"Please check the mimeview file")
			     .arg(QString::fromUtf8(doc.mimetype.c_str()))
			     .arg(QString::fromLocal8Bit(cmd.c_str())));
	return;
    }

    // Look for the command to execute in the exec path and the filters 
    // directory
    string execpath;
    if (!ExecCmd::which(lcmd.front(), execpath)) {
	execpath = theconfig->findFilter(lcmd.front());
	// findFilter returns its input param if the filter is not in
	// the normal places. As we already looked in the path, we
	// have no use for a simple command name here (as opposed to
	// mimehandler which will just let execvp do its thing). Erase
	// execpath so that the user dialog will be started further
	// down.
	if (!execpath.compare(lcmd.front())) 
	    execpath.erase();

	// Specialcase text/html because of the help browser need
	if (execpath.empty() && !doc.mimetype.compare("text/html") && 
	    apptag.empty()) {
	    if (lookForHtmlBrowser(execpath)) {
		lcmd.clear();
		lcmd.push_back(execpath);
		lcmd.push_back("%u");
	    }
	}
    }

    // Command not found: start the user dialog to help find another one:
    if (execpath.empty()) {
	QString mt = QString::fromUtf8(doc.mimetype.c_str());
	QString message = tr("The viewer specified in mimeview for %1: %2"
			     " is not found.\nDo you want to start the "
			     " preferences dialog ?")
	    .arg(mt).arg(QString::fromLocal8Bit(lcmd.front().c_str()));

	switch(QMessageBox::warning(0, "Recoll", message, 
				    "Yes", "No", 0, 0, 1)) {
	case 0: 
	    showUIPrefs();
	    if (uiprefs)
		uiprefs->showViewAction(mt);
	    break;
	case 1:
	    break;
	}
        // The user will have to click on the link again to try the
        // new command.
	return;
    }
    // Get rid of the command name. lcmd is now argv[1...n]
    lcmd.erase(lcmd.begin());

    // Process the command arguments to determine if we need to create
    // a temporary file.

    // If the command has a %i parameter it will manage the
    // un-embedding. Else if ipath is not empty, we need a temp file.
    // This can be overridden with the "ignoreipath" attribute
    bool groksipath = (cmd.find("%i") != string::npos) || ignoreipath;

    // wantsfile: do we actually need a local file ? The only other
    // case here is an url %u (ie: for web history).
    bool wantsfile = cmd.find("%f") != string::npos && urlisfileurl(doc.url);
    bool wantsparentfile = cmd.find("%F") != string::npos && 
	urlisfileurl(doc.url);

    if (wantsfile && wantsparentfile) {
	QMessageBox::warning(0, "Recoll", 
			     tr("Viewer command line for %1 specifies both "
				"file and parent file value: unsupported")
			     .arg(QString::fromUtf8(doc.mimetype.c_str())));
	return;
    }
	
    string url = doc.url;
    string fn = fileurltolocalpath(doc.url);
    Rcl::Doc pdoc;
    if (wantsparentfile) {
	// We want the path for the parent document. For example to
	// open the chm file, not the internal page. Note that we just
	// override the other file name in this case.
	if (m_source.isNull() || !m_source->getEnclosing(doc, pdoc)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Cannot find parent document"));
	    return;
	}
	// Override fn with the parent's : 
	fn = fileurltolocalpath(pdoc.url);

	// If the parent document has an ipath too, we need to create
	// a temp file even if the command takes an ipath
	// parameter. We have no viewer which could handle a double
	// embedding. Will have to change if such a one appears.
	if (!pdoc.ipath.empty()) {
	    groksipath = false;
	}
    }

    bool enterHistory = false;
    bool istempfile = false;
    
    LOGDEB(("RclMain::startNV: groksipath %d wantsf %d wantsparentf %d\n", 
	    groksipath, wantsfile, wantsparentfile));

    // If the command wants a file but this is not a file url, or
    // there is an ipath that it won't understand, we need a temp file:
    theconfig->setKeyDir(path_getfather(fn));
    if (!doc.isFsFile() || 
        ((wantsfile || wantsparentfile) && fn.empty()) ||
	(!groksipath && !doc.ipath.empty()) ) {
	TempFile temp;
	Rcl::Doc& thedoc = wantsparentfile ? pdoc : doc;
	if (!FileInterner::idocToFile(temp, string(), theconfig, thedoc)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Cannot extract document or create "
				    "temporary file"));
	    return;
	}
	enterHistory = true;
        istempfile = true;
	rememberTempFile(temp);
	fn = temp->filename();
	url = string("file://") + fn;
    }

    // If using an actual file, check that it exists, and if it is
    // compressed, we may need an uncompressed version
    if (!fn.empty() && theconfig->mimeViewerNeedsUncomp(doc.mimetype)) {
        if (access(fn.c_str(), R_OK) != 0) {
            QMessageBox::warning(0, "Recoll", 
                                 tr("Can't access file: ") + 
                                 QString::fromLocal8Bit(fn.c_str()));
            return;
        }
        TempFile temp;
        if (FileInterner::isCompressed(fn, theconfig)) {
            if (!FileInterner::maybeUncompressToTemp(temp, fn, theconfig,  
                                                     doc)) {
                QMessageBox::warning(0, "Recoll", 
                                     tr("Can't uncompress file: ") + 
                                     QString::fromLocal8Bit(fn.c_str()));
                return;
            }
        }
        if (!temp.isNull()) {
            istempfile = true;
	    rememberTempFile(temp);
            fn = temp->filename();
            url = string("file://") + fn;
        }
    }

    if (istempfile) {
        QxtConfirmationMessage confirm(
            QMessageBox::Warning,
            "Recoll",
            tr("Opening a temporary copy. Edits will be lost if you don't save"
               "<br/>them to a permanent location."),
            "Do not show this warning next time "
            "(open GUI preferences to restore)");
        confirm.setSettingsPath("Recoll/prefs");
        confirm.setOverrideSettingsKey("showTempFileWarning");
        confirm.exec();
    }

    // If we are not called with a page number (which would happen for a call
    // from the snippets window), see if we can compute a page number anyway.
    if (pagenum == -1) {
	pagenum = 1;
	string lterm;
	if (m_source.isNotNull())
	    pagenum = m_source->getFirstMatchPage(doc, lterm);
	if (pagenum == -1)
	    pagenum = 1;
	else // We get the match term used to compute the page
	    term = QString::fromUtf8(lterm.c_str());
    }
    char cpagenum[20];
    sprintf(cpagenum, "%d", pagenum);


    // Substitute %xx inside arguments
    string efftime;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
        efftime = doc.dmtime.empty() ? doc.fmtime : doc.dmtime;
    } else {
        efftime = "0";
    }
    // Try to keep the letters used more or less consistent with the reslist
    // paragraph format.
    map<string, string> subs;
    subs["D"] = efftime;
    subs["f"] = fn;
    subs["F"] = fn;
    subs["i"] = FileInterner::getLastIpathElt(doc.ipath);
    subs["M"] = doc.mimetype;
    subs["p"] = cpagenum;
    subs["s"] = (const char*)term.toLocal8Bit();
    subs["U"] = url;
    subs["u"] = url;
    // Let %(xx) access all metadata.
    for (map<string,string>::const_iterator it = doc.meta.begin();
         it != doc.meta.end(); it++) {
        subs[it->first] = it->second;
    }
    execViewer(subs, enterHistory, execpath, lcmd, cmd, doc);
}

void RclMain::execViewer(const map<string, string>& subs, bool enterHistory,
                         const string& execpath,
                         const vector<string>& _lcmd, const string& cmd,
                         Rcl::Doc doc)
{
    string ncmd;
    vector<string> lcmd;
    for (vector<string>::const_iterator it = _lcmd.begin(); 
         it != _lcmd.end(); it++) {
        pcSubst(*it, ncmd, subs);
        LOGDEB(("%s->%s\n", it->c_str(), ncmd.c_str()));
        lcmd.push_back(ncmd);
    }

    // Also substitute inside the unsplitted command line and display
    // in status bar
    pcSubst(cmd, ncmd, subs);
    ncmd += " &";
    QStatusBar *stb = statusBar();
    if (stb) {
	string fcharset = theconfig->getDefCharset(true);
	string prcmd;
	transcode(ncmd, prcmd, fcharset, "UTF-8");
	QString msg = tr("Executing: [") + 
	    QString::fromUtf8(prcmd.c_str()) + "]";
	stb->showMessage(msg, 10000);
    }

    if (!enterHistory)
	historyEnterDoc(g_dynconf, doc.meta[Rcl::Doc::keyudi]);
    
    // Do the zeitgeist thing
    zg_send_event(ZGSEND_OPEN, doc);

    // We keep pushing back and never deleting. This can't be good...
    ExecCmd *ecmd = new ExecCmd;
    m_viewers.push_back(ecmd);
    ecmd->startExec(execpath, lcmd, false, false);
}

void RclMain::startManual()
{
    startManual(string());
}

void RclMain::startManual(const string& index)
{
    Rcl::Doc doc;
    doc.url = "file://";
    doc.url = path_cat(doc.url, theconfig->getDatadir());
    doc.url = path_cat(doc.url, "doc");
    doc.url = path_cat(doc.url, "usermanual.html");
    LOGDEB(("RclMain::startManual: help index is %s\n", 
	    index.empty()?"(null)":index.c_str()));
    if (!index.empty()) {
	doc.url += "#";
	doc.url += index;
    }
    doc.mimetype = "text/html";
    startNativeViewer(doc);
}
