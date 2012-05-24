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

#include <time.h>
#include <stdlib.h>

#include <qapplication.h>
#include <qvariant.h>
#include <qevent.h>
#include <qmenu.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qimage.h>
#include <qclipboard.h>
#include <qscrollbar.h>
#include <QTextBlock>
#ifndef __APPLE__
#include <qx11info_x11.h>
#endif

#include "debuglog.h"
#include "smallut.h"
#include "recoll.h"
#include "guiutils.h"
#include "pathut.h"
#include "docseq.h"
#include "pathut.h"
#include "mimehandler.h"
#include "plaintorich.h"
#include "refcntr.h"
#include "internfile.h"
#include "indexer.h"

#include "reslist.h"
#include "moc_reslist.cpp"
#include "rclhelp.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

#ifndef RESLIST_TEXTBROWSER
#include <QWebFrame>
#include <QWebElement>
#include <QWebSettings>
#endif

class QtGuiResListPager : public ResListPager {
public:
    QtGuiResListPager(ResList *p, int ps) 
	: ResListPager(ps), m_parent(p) 
    {}
    virtual bool append(const string& data);
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc);
    virtual string trans(const string& in);
    virtual string detailsLink();
    virtual const string &parFormat();
    virtual const string &dateFormat();
    virtual string nextUrl();
    virtual string prevUrl();
    virtual string headerContent();
    virtual void suggest(const vector<string>uterms, 
			 map<string, vector<string> >& sugg);
    virtual string absSep() {return (const char *)(prefs.abssep.toUtf8());}
    virtual string iconUrl(RclConfig *, Rcl::Doc& doc);
private:
    ResList *m_parent;
};

#if 0
FILE *fp;
void logdata(const char *data)
{
    if (fp == 0)
	fp = fopen("/tmp/recolltoto.html", "a");
    if (fp)
	fprintf(fp, "%s", data);
}
#else
#define logdata(X)
#endif

//////////////////////////////
// /// QtGuiResListPager methods:
bool QtGuiResListPager::append(const string& data)
{
    LOGDEB2(("QtGuiReslistPager::appendString   : %s\n", data.c_str()));
    logdata(data.c_str());
    m_parent->append(QString::fromUtf8(data.c_str()));
    return true;
}

bool QtGuiResListPager::append(const string& data, int docnum, 
			       const Rcl::Doc&)
{
    LOGDEB2(("QtGuiReslistPager::appendDoc: blockCount %d, %s\n",
	    m_parent->document()->blockCount(), data.c_str()));
    logdata(data.c_str());
#ifdef RESLIST_TEXTBROWSER
    int blkcnt0 = m_parent->document()->blockCount();
    m_parent->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
    m_parent->textCursor().insertBlock();
    m_parent->insertHtml(QString::fromUtf8(data.c_str()));
    m_parent->moveCursor(QTextCursor::Start, QTextCursor::MoveAnchor);
    m_parent->ensureCursorVisible();
    int blkcnt1 = m_parent->document()->blockCount();
    for (int block = blkcnt0; block < blkcnt1; block++) {
	m_parent->m_pageParaToReldocnums[block] = docnum;
    }
#else
    QString sdoc = QString("<div class=\"rclresult\" rcldocnum=\"%1\">").arg(docnum);
    m_parent->append(sdoc);
    m_parent->append(QString::fromUtf8(data.c_str()));
    m_parent->append("</div>");
#endif
    return true;
}

string QtGuiResListPager::trans(const string& in)
{
    return string((const char*)ResList::tr(in.c_str()).toUtf8());
}

string QtGuiResListPager::detailsLink()
{
    string chunk = "<a href=\"H-1\">";
    chunk += trans("(show query)");
    chunk += "</a>";
    return chunk;
}

const string& QtGuiResListPager::parFormat()
{
    return prefs.creslistformat;
}
const string& QtGuiResListPager::dateFormat()
{
    return prefs.creslistdateformat;
}

string QtGuiResListPager::nextUrl()
{
    return "n-1";
}

string QtGuiResListPager::prevUrl()
{
    return "p-1";
}

string QtGuiResListPager::headerContent() 
{
    return (const char *)prefs.reslistheadertext.toUtf8();
}

void QtGuiResListPager::suggest(const vector<string>uterms, 
				map<string, vector<string> >& sugg)
{
    sugg.clear();
#ifdef RCL_USE_ASPELL
    bool noaspell = false;
    theconfig->getConfParam("noaspell", &noaspell);
    if (noaspell)
        return;
    if (!aspell) {
        LOGERR(("QtGuiResListPager:: aspell not initialized\n"));
        return;
    }
    for (vector<string>::const_iterator uit = uterms.begin();
         uit != uterms.end(); uit++) {
        list<string> asuggs;
        string reason;

	// If the term is in the index, we don't suggest alternatives. 
	// Actually, we may want to check the frequencies and propose something
	// anyway if a possible variation is much more common (as google does)
        if (aspell->check(*rcldb, *uit, reason))
            continue;
        else if (!reason.empty())
            return;
        if (!aspell->suggest(*rcldb, *uit, asuggs, reason)) {
            LOGERR(("QtGuiResListPager::suggest: aspell failed: %s\n", 
                    reason.c_str()));
            continue;
        }
        if (!asuggs.empty()) {
            sugg[*uit] = vector<string>(asuggs.begin(), asuggs.end());
	    if (sugg[*uit].size() > 5)
		sugg[*uit].resize(5);
	    // Set up the links as a <href="Sold|new">. 
	    for (vector<string>::iterator it = sugg[*uit].begin();
		 it != sugg[*uit].end(); it++) {
		*it = string("<a href=\"S") + *uit + "|" + *it + "\">" +
		    *it + "</a>";
	    }
        }
    }
#endif

}

string QtGuiResListPager::iconUrl(RclConfig *config, Rcl::Doc& doc)
{
    if (doc.ipath.empty()) {
	vector<Rcl::Doc> docs;
	docs.push_back(doc);
	vector<string> paths;
	ConfIndexer::docsToPaths(docs, paths);
	if (!paths.empty()) {
	    string path;
	    LOGDEB0(("ResList::iconUrl: source path [%s]\n", paths[0].c_str()));
	    if (thumbPathForUrl(cstr_fileu + paths[0], 128, path)) {
		LOGDEB0(("ResList::iconUrl: icon path [%s]\n", path.c_str()));
		return cstr_fileu + path;
	    } else {
		LOGDEB0(("ResList::iconUrl: no icon: path [%s]\n", 
			 path.c_str()));
	    }
	} else {
	    LOGDEB(("ResList::iconUrl: docsToPaths failed\n"));
	}
    }
    return ResListPager::iconUrl(config, doc);
}

/////// /////// End reslistpager methods

class PlainToRichQtReslist : public PlainToRich {
public:
    virtual ~PlainToRichQtReslist() {}
    virtual string startMatch() {
	return string("<span class='rclmatch' style='color: ")
	    + string((const char *)prefs.qtermcolor.toAscii()) + string("'>");
    }
    virtual string endMatch() {return string("</span>");}
};
static PlainToRichQtReslist g_hiliter;

/////////////////////////////////////

ResList::ResList(QWidget* parent, const char* name)
    : RESLIST_PARENTCLASS(parent)
{
    if (!name)
	setObjectName("resList");
    else 
	setObjectName(name);
#ifdef RESLIST_TEXTBROWSER
    LOGDEB(("Reslist: using QTextBrowser\n"));
    setReadOnly(TRUE);
    setUndoRedoEnabled(FALSE);
    setOpenLinks(FALSE);
    setTabChangesFocus(true);
    // signals and slots connections
    connect(this, SIGNAL(anchorClicked(const QUrl &)), 
	    this, SLOT(linkWasClicked(const QUrl &)));
#else
    LOGDEB(("Reslist: using QWebView\n"));
    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QUrl &)), 
	    this, SLOT(linkWasClicked(const QUrl &)));
    page()->setLinkDelegationPolicy(QWebPage::DelegateAllLinks);
    settings()->setAttribute(QWebSettings::JavascriptEnabled, true);
#endif
    setFont();
    languageChange();

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)this->objectName().toAscii(), 
			   "RCL.SEARCH.RESLIST");

#if 0
    // See comments in "highlighted
    connect(this, SIGNAL(highlighted(const QString &)), 
	    this, SLOT(highlighted(const QString &)));
#endif
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(createPopupMenu(const QPoint&)));

    m_curPvDoc = -1;
    m_lstClckMod = 0;
    m_listId = 0;
    m_pager = new QtGuiResListPager(this, prefs.respagesize);
    m_pager->setHighLighter(&g_hiliter);
}

ResList::~ResList()
{
    // These have to exist somewhere for translations to work
#ifdef __GNUC__
    __attribute__((unused))
#endif
    static const char* strings[] = {
	QT_TR_NOOP("<p><b>No results found</b><br>"),
	QT_TR_NOOP("Documents"),
	QT_TR_NOOP("out of at least"),
	QT_TR_NOOP("for"),
	QT_TR_NOOP("Previous"),
	QT_TR_NOOP("Next"),
	QT_TR_NOOP("Unavailable document"),
	QT_TR_NOOP("Preview"),
	QT_TR_NOOP("Open"),
	QT_TR_NOOP("(show query)"),
        QT_TR_NOOP("<p><i>Alternate spellings (accents suppressed): </i>"),
    };
}

void ResList::setFont()
{
#ifdef RESLIST_TEXTBROWSER
    if (prefs.reslistfontfamily.length()) {
	QFont nfont(prefs.reslistfontfamily, prefs.reslistfontsize);
	QTextBrowser::setFont(nfont);
    } else {
	QTextBrowser::setFont(QFont());
    }
#else
    QWebSettings *websettings = settings();
    if (prefs.reslistfontfamily.length()) {
	websettings->setFontSize(QWebSettings::DefaultFontSize, 
				 prefs.reslistfontsize);
	websettings->setFontFamily(QWebSettings::StandardFont, 
			       prefs.reslistfontfamily);
    } else {
	websettings->resetFontSize(QWebSettings::DefaultFontSize);
	websettings->resetFontFamily(QWebSettings::StandardFont);
    }
#endif
}

int ResList::newListId()
{
    static int id;
    return ++id;
}

extern "C" int XFlush(void *);

void ResList::setDocSource(RefCntr<DocSequence> nsource)
{
    LOGDEB(("ResList::setDocSource()\n"));
    m_source = RefCntr<DocSequence>(new DocSource(theconfig, nsource));
}

// A query was executed, or the filtering/sorting parameters changed,
// re-read the results.
void ResList::readDocSource()
{
    LOGDEB(("ResList::readDocSource()\n"));
    resetView();
    if (m_source.isNull())
	return;
    m_listId = newListId();

    // Reset the page size in case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->setDocSource(m_source);
    resultPageNext();
    emit hasResults(m_source->getResCnt());
}

void ResList::resetList() 
{
    LOGDEB(("ResList::resetList()\n"));
    setDocSource(RefCntr<DocSequence>());
    resetView();
}

void ResList::resetView() 
{
    m_curPvDoc = -1;
    // There should be a progress bar for long searches but there isn't 
    // We really want the old result list to go away, otherwise, for a
    // slow search, the user will wonder if anything happened. The
    // following helps making sure that the textedit is really
    // blank. Else, there are often icons or text left around
#ifdef RESLIST_TEXTBROWSER
    m_pageParaToReldocnums.clear();
    clear();
    QTextBrowser::append(".");
    clear();
#ifndef __APPLE__
    XFlush(QX11Info::display());
#endif
#else
    m_text = "";
    setHtml("<html><body></body></html>");
#endif

}

bool ResList::displayingHistory()
{
    // We want to reset the displayed history if it is currently
    // shown. Using the title value is an ugly hack
    string htstring = string((const char *)tr("Document history").toUtf8());
    if (m_source.isNull() || m_source->title().empty())
	return false;
    return m_source->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setWindowTitle(tr("Result list"));
}

#ifdef RESLIST_TEXTBROWSER    
// Get document number from text block number
int ResList::docnumfromparnum(int block)
{
    if (m_pager->pageNumber() < 0)
	return -1;

    // Try to find the first number < input and actually in the map
    // (result blocks can be made of several text blocks)
    std::map<int,int>::iterator it;
    do {
	it = m_pageParaToReldocnums.find(block);
	if (it != m_pageParaToReldocnums.end())
	    return pageFirstDocNum() + it->second;
    } while (--block >= 0);
    return -1;
}

// Get range of paragraph numbers which make up the result for document number
pair<int,int> ResList::parnumfromdocnum(int docnum)
{
    LOGDEB(("parnumfromdocnum: docnum %d\n", docnum));
    if (m_pager->pageNumber() < 0) {
	LOGDEB(("parnumfromdocnum: no page return -1,-1\n"));
	return pair<int,int>(-1,-1);
    }
    int winfirst = pageFirstDocNum();
    if (docnum - winfirst < 0) {
	LOGDEB(("parnumfromdocnum: docnum %d < winfirst %d return -1,-1\n",
		docnum, winfirst));
	return pair<int,int>(-1,-1);
    }
    docnum -= winfirst;
    for (std::map<int,int>::iterator it = m_pageParaToReldocnums.begin();
	 it != m_pageParaToReldocnums.end(); it++) {
	if (docnum == it->second) {
	    int first = it->first;
	    int last = first+1;
	    std::map<int,int>::iterator it1;
	    while ((it1 = m_pageParaToReldocnums.find(last)) != 
		   m_pageParaToReldocnums.end() && it1->second == docnum) {
		last++;
	    }
	    LOGDEB(("parnumfromdocnum: return %d,%d\n", first, last));
	    return pair<int,int>(first, last);
	}
    }
    LOGDEB(("parnumfromdocnum: not found return -1,-1\n"));
    return pair<int,int>(-1,-1);
}
#endif // TEXTBROWSER

// Return doc from current or adjacent result pages. We can get called
// for a document not in the current page if the user browses through
// results inside a result window (with shift-arrow). This can only
// result in a one-page change.
bool ResList::getDoc(int docnum, Rcl::Doc &doc)
{
    LOGDEB(("ResList::getDoc: docnum %d winfirst %d\n", docnum, 
	    pageFirstDocNum()));
    int winfirst = pageFirstDocNum();
    int winlast = m_pager->pageLastDocNum();
    if (docnum < 0 ||  winfirst < 0 || winlast < 0)
	return false;

    // Is docnum in current page ? Then all Ok
    if (docnum >= winfirst && docnum <= winlast) {
	return m_source->getDoc(docnum, doc);
    }

    // Else we accept to page down or up but not further
    if (docnum < winfirst && docnum >= winfirst - prefs.respagesize) {
	resultPageBack();
    } else if (docnum < winlast + 1 + prefs.respagesize) {
	resultPageNext();
    }
    winfirst = pageFirstDocNum();
    winlast = m_pager->pageLastDocNum();
    if (docnum >= winfirst && docnum <= winlast) {
	return m_source->getDoc(docnum, doc);
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if ((e->modifiers() & Qt::ShiftModifier)) {
	if (e->key() == Qt::Key_PageUp) {
	    // Shift-PageUp -> first page of results
	    resultPageFirst();
	    return;
	} 
    } else {
	if (e->key() == Qt::Key_PageUp || e->key() == Qt::Key_Backspace) {
	    resPageUpOrBack();
	    return;
	} else if (e->key() == Qt::Key_PageDown || e->key() == Qt::Key_Space) {
	    resPageDownOrNext();
	    return;
	}
    }
    RESLIST_PARENTCLASS::keyPressEvent(e);
}

void ResList::mouseReleaseEvent(QMouseEvent *e)
{
    m_lstClckMod = 0;
    if (e->modifiers() & Qt::ControlModifier) {
	m_lstClckMod |= Qt::ControlModifier;
    } 
    if (e->modifiers() & Qt::ShiftModifier) {
	m_lstClckMod |= Qt::ShiftModifier;
    }
    RESLIST_PARENTCLASS::mouseReleaseEvent(e);
}

void ResList::highlighted(const QString& )
{
    // This is supposedly called when a link is preactivated (hover or tab
    // traversal, but is not actually called for tabs. We would have liked to
    // give some kind of visual feedback for tab traversal
}

// Page Up/Down: we don't try to check if current paragraph is last or
// first. We just page up/down and check if viewport moved. If it did,
// fair enough, else we go to next/previous result page.
void ResList::resPageUpOrBack()
{
#ifdef RESLIST_TEXTBROWSER
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
    if (vpos == verticalScrollBar()->value())
	resultPageBack();
#else
    if (scrollIsAtTop()) {
	resultPageBack();
    } else {
	QWebFrame *frame = page()->mainFrame();
	frame->scroll(0, -int(0.9*geometry().height()));
    }
    setupArrows();
#endif
}

void ResList::resPageDownOrNext()
{
#ifdef RESLIST_TEXTBROWSER
    int vpos = verticalScrollBar()->value();
    verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
    LOGDEB(("ResList::resPageDownOrNext: vpos before %d, after %d\n",
	    vpos, verticalScrollBar()->value()));
    if (vpos == verticalScrollBar()->value()) 
	resultPageNext();
#else
    if (scrollIsAtBottom()) {
	resultPageNext();
    } else {
	QWebFrame *frame = page()->mainFrame();
	frame->scroll(0, int(0.9*geometry().height()));
    }
    setupArrows();
#endif
}

void ResList::setupArrows()
{
    emit prevPageAvailable(m_pager->hasPrev() || !scrollIsAtTop());
    emit nextPageAvailable(m_pager->hasNext() || !scrollIsAtBottom());
}

bool ResList::scrollIsAtBottom()
{
#ifdef RESLIST_TEXTBROWSER
    return false;
#else
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
	ret = true;
    } else {
	int max = frame->scrollBarMaximum(Qt::Vertical);
	int cur = frame->scrollBarValue(Qt::Vertical); 
	ret = (max != 0) && (cur == max);
	LOGDEB2(("Scrollatbottom: cur %d max %d\n", cur, max));
    }
    LOGDEB2(("scrollIsAtBottom: returning %d\n", ret));
    return ret;
#endif
}

bool ResList::scrollIsAtTop()
{
#ifdef RESLIST_TEXTBROWSER
    return false;
#else
    QWebFrame *frame = page()->mainFrame();
    bool ret;
    if (!frame || frame->scrollBarGeometry(Qt::Vertical).isEmpty()) {
	ret = true;
    } else {
	int cur = frame->scrollBarValue(Qt::Vertical);
	int min = frame->scrollBarMinimum(Qt::Vertical);
	LOGDEB(("Scrollattop: cur %d min %d\n", cur, min));
	ret = (cur == min);
    }
#endif
    LOGDEB2(("scrollIsAtTop: returning %d\n", ret));
    return ret;
}

// Show previous page of results. We just set the current number back
// 2 pages and show next page.
void ResList::resultPageBack()
{
    if (m_pager->hasPrev()) {
	m_pager->resultPageBack();
	displayPage();
    }
}

// Go to the first page
void ResList::resultPageFirst()
{
    // In case the preference was changed
    m_pager->setPageSize(prefs.respagesize);
    m_pager->resultPageFirst();
    displayPage();
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    if (m_pager->hasNext()) {
	m_pager->resultPageNext();
	displayPage();
    }
}

void ResList::resultPageFor(int docnum)
{
    m_pager->resultPageFor(docnum);
    displayPage();
}

void ResList::append(const QString &text)
{
    LOGDEB2(("QtGuiReslistPager::appendQString  : %s\n", 
	    (const char*)text.toUtf8()));
#ifdef RESLIST_TEXTBROWSER
    QTextBrowser::append(text);
#else
    m_text += text;
#endif
}

void ResList::displayPage()
{
    resetView();

    m_pager->displayPage(theconfig);

#ifndef RESLIST_TEXTBROWSER
    setHtml(m_text);
#endif

    LOGDEB0(("ResList::displayPg: hasNext %d atBot %d hasPrev %d at Top %d \n",
	     m_pager->hasPrev(), scrollIsAtBottom(), 
	     m_pager->hasNext(), scrollIsAtTop()));
    setupArrows();

    // Possibly color paragraph of current preview if any
    previewExposed(m_curPvDoc);

}

// Color paragraph (if any) of currently visible preview
void ResList::previewExposed(int docnum)
{
    LOGDEB(("ResList::previewExposed: doc %d\n", docnum));

    // Possibly erase old one to white
    if (m_curPvDoc != -1) {
#ifdef RESLIST_TEXTBROWSER
	pair<int,int> blockrange = parnumfromdocnum(m_curPvDoc);
	if (blockrange.first != -1) {
	    for (int blockn = blockrange.first;
		 blockn < blockrange.second; blockn++) {
		QTextBlock block = document()->findBlockByNumber(blockn);
		QTextCursor cursor(block);
		QTextBlockFormat format = cursor.blockFormat();
		format.clearBackground();
		cursor.setBlockFormat(format);
	    }
	}
#else
	QString sel = 
	   QString("div[rcldocnum=\"%1\"]").arg(m_curPvDoc - pageFirstDocNum());
	LOGDEB2(("Searching for element, selector: [%s]\n", 
		 (const char *)sel.toAscii()));
	QWebElement elt = page()->mainFrame()->findFirstElement(sel);
	if (!elt.isNull()) {
	    LOGDEB2(("Found\n"));
	    elt.removeAttribute("style");
	} else {
	    LOGDEB2(("Not Found\n"));
	}
#endif
	m_curPvDoc = -1;
    }

    // Set background for active preview's doc entry
    m_curPvDoc = docnum;

#ifdef RESLIST_TEXTBROWSER
    pair<int,int>  blockrange = parnumfromdocnum(docnum);

    // Maybe docnum is -1 or not in this window, 
    if (blockrange.first < 0)
	return;
    // Color the new active paragraph
    QColor color("LightBlue");
    for (int blockn = blockrange.first+1;
	 blockn < blockrange.second; blockn++) {
	QTextBlock block = document()->findBlockByNumber(blockn);
	QTextCursor cursor(block);
	QTextBlockFormat format;
	format.setBackground(QBrush(color));
	cursor.mergeBlockFormat(format);
	setTextCursor(cursor);
	ensureCursorVisible();
    }
#else
    QString sel = 
	QString("div[rcldocnum=\"%1\"]").arg(docnum - pageFirstDocNum());
    LOGDEB2(("Searching for element, selector: [%s]\n", 
	    (const char *)sel.toAscii()));
    QWebElement elt = page()->mainFrame()->findFirstElement(sel);
    if (!elt.isNull()) {
	LOGDEB2(("Found\n"));
	elt.setAttribute("style", "background: LightBlue;}");
    } else {
	LOGDEB2(("Not Found\n"));
    }
#endif
}

// Double click in res list: add selection to simple search
void ResList::mouseDoubleClickEvent(QMouseEvent *event)
{
    RESLIST_PARENTCLASS::mouseDoubleClickEvent(event);
#ifdef RESLIST_TEXTBROWSER
    if (textCursor().hasSelection())
	emit(wordSelect(textCursor().selectedText()));
#else
    emit(wordSelect(selectedText()));
#endif
}

void ResList::linkWasClicked(const QUrl &url)
{
    string ascurl = (const char *)url.toString().toAscii();;
    LOGDEB(("ResList::linkWasClicked: [%s]\n", ascurl.c_str()));

    int what = ascurl[0];
    switch (what) {
    case 'H': 
	emit headerClicked(); 
	break;
    case 'P': 
    case 'E': 
    {
	int i = atoi(ascurl.c_str()+1) - 1;
	Rcl::Doc doc;
	if (!getDoc(i, doc)) {
	    LOGERR(("ResList::linkWasClicked: can't get doc for %d\n", i));
	    return;
	}
	if (what == 'P')
	    emit docPreviewClicked(i, doc, m_lstClckMod);
	else
	    emit docEditClicked(doc);
    }
    break;
    case 'n':
	resultPageNext();
	break;
    case 'p':
	resultPageBack();
	break;
    case 'S':
    {
	QString s = url.toString();
	if (!s.isEmpty())
	    s = s.right(s.size()-1);
	int bar = s.indexOf("|");
	if (bar != -1 && bar < s.size()-1) {
	    QString o = s.left(bar);
	    QString n = s.right(s.size() - (bar+1));
	    emit wordReplace(o, n);
	}
    }
    break;
    default: 
	LOGERR(("ResList::linkWasClicked: bad link [%s]\n", ascurl.c_str()));
	break;// ?? 
    }
}

void ResList::createPopupMenu(const QPoint& pos)
{
    LOGDEB(("ResList::createPopupMenu(%d, %d)\n", pos.x(), pos.y()));
#ifdef RESLIST_TEXTBROWSER
    QTextCursor cursor = cursorForPosition(pos);
    int blocknum = cursor.blockNumber();
    LOGDEB(("ResList::createPopupMenu(): block %d\n", blocknum));
    m_popDoc = docnumfromparnum(blocknum);
#else
    QWebHitTestResult htr = page()->mainFrame()->hitTestContent(pos);
    if (htr.isNull())
	return;
    QWebElement el = htr.enclosingBlockElement();
    while (!el.isNull() && !el.hasAttribute("rcldocnum"))
	el = el.parent();
    if (el.isNull())
	return;
    QString snum = el.attribute("rcldocnum");
    m_popDoc = pageFirstDocNum() + snum.toInt();
#endif

    if (m_popDoc < 0) 
	return;
    QMenu *popup = new QMenu(this);
    popup->addAction(tr("&Preview"), this, SLOT(menuPreview()));

    Rcl::Doc doc;
    bool havedoc = getDoc(m_popDoc, doc);
    string apptag;
    if (havedoc)
	doc.getmeta(Rcl::Doc::keyapptg, &apptag);

    if (havedoc && !theconfig->getMimeViewerDef(doc.mimetype, apptag).empty()) {
	popup->addAction(tr("&Open"), this, SLOT(menuEdit()));
    }
    popup->addAction(tr("Copy &File Name"), this, SLOT(menuCopyFN()));
    popup->addAction(tr("Copy &URL"), this, SLOT(menuCopyURL()));
    if (havedoc && !doc.ipath.empty()) {
	popup->addAction(tr("&Write to File"), this, SLOT(menuSaveToFile()));
    }

    popup->addAction(tr("Find &similar documents"), this, SLOT(menuExpand()));
    popup->addAction(tr("Preview P&arent document/folder"), 
		      this, SLOT(menuPreviewParent()));
    popup->addAction(tr("&Open Parent document/folder"), 
		     this, SLOT(menuOpenParent()));
    popup->popup(mapToGlobal(pos));
}

void ResList::menuPreview()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docPreviewClicked(m_popDoc, doc, 0);
}

void ResList::menuSaveToFile()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docSaveToFileClicked(doc);
}

void ResList::menuPreviewParent()
{
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc) || m_source.isNull()) 
	return;
    Rcl::Doc pdoc;
    if (m_source->getEnclosing(doc, pdoc)) {
	emit previewRequested(pdoc);
    } else {
	// No parent doc: show enclosing folder with app configured for
	// directories
	pdoc.url = path_getfather(doc.url);
	pdoc.mimetype = "application/x-fsdirectory";
	emit editRequested(pdoc);
    }
}

void ResList::menuOpenParent()
{
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc) || m_source.isNull()) 
	return;
    Rcl::Doc pdoc;
    if (m_source->getEnclosing(doc, pdoc)) {
	emit editRequested(pdoc);
    } else {
	// No parent doc: show enclosing folder with app configured for
	// directories
	pdoc.url = path_getfather(doc.url);
	pdoc.mimetype = "application/x-fsdirectory";
	emit editRequested(pdoc);
    }
}

void ResList::menuEdit()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docEditClicked(doc);
}

void ResList::menuCopyFN()
{
    LOGDEB(("menuCopyFN\n"));
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
	LOGDEB(("menuCopyFN: Got doc, fn: [%s]\n", doc.url.c_str()));
	// Our urls currently always begin with "file://" 
        //
        // Problem: setText expects a QString. Passing a (const char*)
        // as we used to do causes an implicit conversion from
        // latin1. File are binary and the right approach would be no
        // conversion, but it's probably better (less worse...) to
        // make a "best effort" tentative and try to convert from the
        // locale's charset than accept the default conversion.
        QString qfn = QString::fromLocal8Bit(doc.url.c_str()+7);
	QApplication::clipboard()->setText(qfn, QClipboard::Selection);
	QApplication::clipboard()->setText(qfn, QClipboard::Clipboard);
    }
}

void ResList::menuCopyURL()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
	string url =  url_encode(doc.url, 7);
	QApplication::clipboard()->setText(url.c_str(), 
					   QClipboard::Selection);
	QApplication::clipboard()->setText(url.c_str(), 
					   QClipboard::Clipboard);
    }
}

void ResList::menuExpand()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc))
	emit docExpand(doc);
}
int ResList::pageFirstDocNum()
{
    return m_pager->pageFirstDocNum();
}
