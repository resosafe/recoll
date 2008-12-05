#ifndef lint
static char rcsid[] = "@(#$Id: reslist.cpp,v 1.47.2.1 2008-12-05 13:10:38 dockes Exp $ (C) 2005 J.F.Dockes";
#endif

#include <time.h>
#include <stdlib.h>

#include <qapplication.h>
#include <qvariant.h>
#include <qevent.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qimage.h>
#include <qclipboard.h>
#include <qscrollbar.h>

#if (QT_VERSION < 0x040000)
#include <qpopupmenu.h>
#else
#ifndef __APPLE__
#include <qx11info_x11.h>
#endif
#include <q3popupmenu.h>
#include <q3stylesheet.h>
#include <q3mimefactory.h>
#define QStyleSheetItem Q3StyleSheetItem
#define QMimeSourceFactory Q3MimeSourceFactory
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

#include "reslist.h"
#include "moc_reslist.cpp"

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif


class PlainToRichQtReslist : public PlainToRich {
public:
    virtual ~PlainToRichQtReslist() {}
    virtual string startMatch() {return string("<termtag>");}
    virtual string endMatch() {return string("</termtag>");}
};

ResList::ResList(QWidget* parent, const char* name)
    : QTEXTBROWSER(parent, name)
{
    if (!name)
	setName("resList");
    setReadOnly(TRUE);
    setUndoRedoEnabled(FALSE);
    languageChange();

    setTabChangesFocus(true);

    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QString &, int)), 
	    this, SLOT(linkWasClicked(const QString &, int)));
    connect(this, SIGNAL(headerClicked()), this, SLOT(showQueryDetails()));
    connect(this, SIGNAL(doubleClicked(int,int)), 
	    this, SLOT(doubleClicked(int, int)));
#if (QT_VERSION >= 0x040000)
    connect(this, SIGNAL(selectionChanged()), this, SLOT(selecChanged()));
#endif
    m_winfirst = -1;
    m_curPvDoc = -1;
    m_lstClckMod = 0;
    m_listId = 0;
}

ResList::~ResList()
{
}

int ResList::newListId()
{
    static int id;
    return ++id;
}

extern "C" int XFlush(void *);

void ResList::setDocSource(RefCntr<DocSequence> ndocsource)
{
    m_baseDocSource = ndocsource;
    setDocSource();
}

// Reapply parameters. Sort params probably changed
void ResList::setDocSource()
{
    if (m_baseDocSource.isNull())
	return;
    resetList();
    m_listId = newListId();
    m_docSource = m_baseDocSource;

    // Filtering must be done before sorting, (which usually
    // truncates the original list)
    if (m_baseDocSource->canFilter()) {
	m_baseDocSource->setFiltSpec(m_filtspecs);
    } else {
	if (m_filtspecs.isNotNull()) {
	    string title = m_baseDocSource->title() + " (" + 
		string((const char*)tr("filtered").utf8()) + ")";
	    m_docSource = 
		RefCntr<DocSequence>(new DocSeqFiltered(m_docSource,m_filtspecs,
							title));
	} 
    }

    if (m_sortspecs.isNotNull()) {
	string title = m_baseDocSource->title() + " (" + 
	    string((const char *)tr("sorted").utf8()) + ")";
	m_docSource = RefCntr<DocSequence>(new DocSeqSorted(m_docSource, 
							    m_sortspecs,
							    title));
    }
    resultPageNext();
}

void ResList::setSortParams(const DocSeqSortSpec& spec)
{
    LOGDEB(("ResList::setSortParams\n"));
    m_sortspecs = spec;
}

void ResList::setFilterParams(const DocSeqFiltSpec& spec)
{
    LOGDEB(("ResList::setFilterParams\n"));
    m_filtspecs = spec;
}

void ResList::resetList() 
{
    m_winfirst = -1;
    m_curPvDoc = -1;
    // There should be a progress bar for long searches but there isn't 
    // We really want the old result list to go away, otherwise, for a
    // slow search, the user will wonder if anything happened. The
    // following helps making sure that the textedit is really
    // blank. Else, there are often icons or text left around
    clear();
    QTEXTBROWSER::append(".");
    clear();
#if (QT_VERSION < 0x040000)
    XFlush(qt_xdisplay());
#else
#ifndef __APPLE__
    XFlush(QX11Info::display());
#endif
#endif
}

bool ResList::displayingHistory()
{
    // We want to reset the displayed history if it is currently
    // shown. Using the title value is an ugly hack
    string htstring = string((const char *)tr("Document history").utf8());
    return m_docSource->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setCaption(tr("Result list"));
}

bool ResList::getTerms(vector<string>& terms, 
		       vector<vector<string> >& groups, vector<int>& gslks)
{
    // We could just
    return m_baseDocSource->getTerms(terms, groups, gslks);
}

list<string> ResList::expand(Rcl::Doc& doc)
{
    if (m_baseDocSource.isNull())
	return list<string>();
    return m_baseDocSource->expand(doc);
}

// Get document number from paragraph number
int ResList::docnumfromparnum(int par)
{
    if (m_winfirst == -1)
	return -1;
    std::map<int,int>::iterator it = m_pageParaToReldocnums.find(par);
    int dn;
    if (it != m_pageParaToReldocnums.end()) {
        dn = m_winfirst + it->second;
    } else {
        dn = -1;
    }
    return dn;
}

// Get paragraph number from document number
int ResList::parnumfromdocnum(int docnum)
{
    if (m_winfirst == -1 || docnum - m_winfirst < 0)
	return -1;
    docnum -= m_winfirst;
    for (std::map<int,int>::iterator it = m_pageParaToReldocnums.begin();
	 it != m_pageParaToReldocnums.end(); it++) {
	if (docnum == it->second)
	    return it->first;
    }
    return -1;
}

// Return doc from current or adjacent result pages. We can get called
// for a document not in the current page if the user browses through
// results inside a result window (with shift-arrow). This can only
// result in a one-page change.
bool ResList::getDoc(int docnum, Rcl::Doc &doc)
{
    LOGDEB(("ResList::getDoc: docnum %d m_winfirst %d\n", docnum, m_winfirst));
    if (docnum < 0)
	return false;
    // Is docnum in current page ? Then all Ok
    if (docnum >= int(m_winfirst) && 
	docnum < int(m_winfirst + m_curDocs.size())) {
	doc = m_curDocs[docnum - m_winfirst];
	return true;
    }

    // Else we accept to page down or up but not further
    if (docnum < int(m_winfirst) && 
	docnum >= int(m_winfirst) - prefs.respagesize) {
	resultPageBack();
    } else if (docnum < 
	       int(m_winfirst + m_curDocs.size()) + prefs.respagesize) {
	resultPageNext();
    }
    if (docnum >= int(m_winfirst) && 
	docnum < int(m_winfirst + m_curDocs.size())) {
	doc = m_curDocs[docnum - m_winfirst];
	return true;
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if (e->key() == Qt::Key_Q && (e->state() & Qt::ControlButton)) {
	recollNeedsExit = 1;
	return;
    } else if (e->key() == Qt::Key_Prior) {
	resPageUpOrBack();
	return;
    } else if (e->key() == Qt::Key_Next) {
	resPageDownOrNext();
	return;
    }
    QTEXTBROWSER::keyPressEvent(e);
}

void ResList::contentsMouseReleaseEvent(QMouseEvent *e)
{
    m_lstClckMod = 0;
    if (e->state() & Qt::ControlButton) {
	m_lstClckMod |= Qt::ControlButton;
    } 
    if (e->state() & Qt::ShiftButton) {
	m_lstClckMod |= Qt::ShiftButton;
    }
    QTEXTBROWSER::contentsMouseReleaseEvent(e);
}

// Return total result list count
int ResList::getResCnt()
{
    if (m_docSource.isNull())
	return -1;
    return m_docSource->getResCnt();
}


#if 1 || (QT_VERSION < 0x040000)
#define SCROLLYPOS contentsY()
#else
#define SCROLLYPOS verticalScrollBar()->value()
#endif

// Page Up/Down: we don't try to check if current paragraph is last or
// first. We just page up/down and check if viewport moved. If it did,
// fair enough, else we go to next/previous result page.
void ResList::resPageUpOrBack()
{
    int vpos = SCROLLYPOS;
    moveCursor(QTEXTBROWSER::MovePgUp, false);
    if (vpos == SCROLLYPOS)
	resultPageBack();
}

void ResList::resPageDownOrNext()
{
    int vpos = SCROLLYPOS;
    moveCursor(QTEXTBROWSER::MovePgDown, false);
    LOGDEB(("ResList::resPageDownOrNext: vpos before %d, after %d\n",
	    vpos, SCROLLYPOS));
    if (vpos == SCROLLYPOS) 
	resultPageNext();
}

// Show previous page of results. We just set the current number back
// 2 pages and show next page.
void ResList::resultPageBack()
{
    if (m_winfirst <= 0)
	return;
    m_winfirst -= 2 * prefs.respagesize;
    resultPageNext();
}

// Go to the first page
void ResList::resultPageFirst()
{
    m_winfirst = -1;
    resultPageNext();
}

// Convert byte count into unit (KB/MB...) appropriate for display
static string displayableBytes(long size)
{
    char sizebuf[30];
    const char * unit = " B ";

    if (size > 1024 && size < 1024*1024) {
	unit = " KB ";
	size /= 1024;
    } else if (size  >= 1024*1204) {
	unit = " MB ";
	size /= (1024*1024);
    }
    sprintf(sizebuf, "%ld%s", size, unit);
    return string(sizebuf);
}

void ResList::append(const QString &text)
{
    QTEXTBROWSER::append(text);
#if 0
    {
	FILE *fp = fopen("/tmp/debugreslist", "a");
	fprintf(fp, "%s\n", (const char *)text.utf8());
	fclose(fp);
    }
#endif
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    if (m_docSource.isNull())
	return;

    int resCnt = m_docSource->getResCnt();
    m_pageParaToReldocnums.clear();

    LOGDEB(("resultPageNext: rescnt %d, winfirst %d\n", resCnt,
	    m_winfirst));

    bool hasPrev = false;
    if (m_winfirst < 0) {
	m_winfirst = 0;
    } else {
	m_winfirst += prefs.respagesize;
    }
    if (m_winfirst)
	hasPrev = true;
    emit prevPageAvailable(hasPrev);

    // Get the next page of results.
    vector<ResListEntry> respage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, 
					   prefs.respagesize, respage);

    // If page was truncated, there is no next
    bool hasNext = pagelen == prefs.respagesize;
    emit nextPageAvailable(hasNext);

    if (pagelen <= 0) {
	// No results ? This can only happen on the first page or if the
	// actual result list size is a multiple of the page pref (else
	// there would have been no Next on the last page)
	if (m_winfirst) {
	    // Have already results. Let them show, just disable the
	    // Next button. We'd need to remove the Next link from the page
	    // too.
	    // Restore the m_winfirst value
	    m_winfirst -= prefs.respagesize;
	    return;
	}
	clear();
	QString chunk = "<qt><head></head><body><p>";
	chunk += "<p><font size=+1><b>";
	chunk += QString::fromUtf8(m_docSource->title().c_str());
	chunk += "</b></font><br>";
	chunk += "<a href=\"H-1\">";
	chunk += tr("Show query details");
	chunk += "</a><br>";
	append(chunk);
	append(tr("<p><b>No results found</b><br>"));
	if (m_winfirst < 0)
	    m_winfirst = -1;
	return;
    }

    clear();
    m_curDocs.clear();

    // Query term colorization
    QStyleSheetItem *item = new QStyleSheetItem(styleSheet(), "termtag" );
    item->setColor(prefs.qtermcolor);

    // Result paragraph format
    string sformat = string(prefs.reslistformat.utf8());
    LOGDEB(("resultPageNext: format: [%s]\n", sformat.c_str()));

    // Display list header
    // We could use a <title> but the textedit doesnt display
    // it prominently
    // Note: have to append text in chunks that make sense
    // html-wise. If we break things up to much, the editor
    // gets confused. Hence the use of the 'chunk' text
    // accumulator
    // Also note that there can be results beyond the estimated resCnt.
    QString chunk = "<qt><head></head><body><p>";

    chunk += "<font size=+1><b>";
    chunk += QString::fromUtf8(m_docSource->title().c_str());
    chunk += ".</b></font>";

    chunk += "&nbsp;&nbsp;&nbsp;";

    if (m_winfirst + pagelen < resCnt) {
	chunk +=
	    tr("Documents <b>%1-%2</b> out of at least <b>%3</b> for ")
	    .arg(m_winfirst+1)
	    .arg(m_winfirst+pagelen)
	    .arg(resCnt);
    } else {
	chunk += tr("Documents <b>%1-%2</b> for ")
	    .arg(m_winfirst+1)
	    .arg(m_winfirst+pagelen);
    }

    chunk += "<a href=\"H-1\">";
    chunk += tr("(show query)");
    chunk += "</a>";

    append(chunk);

    HiliteData hdata;
    m_docSource->getTerms(hdata.terms, hdata.groups, hdata.gslks);

    // Insert results in result list window. We have to actually send
    // the text to the widget (instead of setting the whole at the
    // end), because we need the paragraph number each time we add a
    // result paragraph (its diffult and error-prone to compute the
    // paragraph numbers in parallel). We would like to disable
    // updates while we're doing this, but couldn't find a way to make
    // it work, the widget seems to become confused if appended while
    // updates are disabled
    //      setUpdatesEnabled(false);
    for (int i = 0; i < pagelen; i++) {

	Rcl::Doc &doc(respage[i].doc);
	string& sh(respage[i].subHeader);
	int percent;
	if (doc.pc == -1) {
	    percent = 0;
	    // Document not available, maybe other further, will go on.
	    doc.meta[Rcl::Doc::keyabs] = string(tr("Unavailable document").utf8());
	} else {
	    percent = doc.pc;
	}
	// Percentage of 'relevance'
	char perbuf[10];
	sprintf(perbuf, "%3d%% ", percent);

	// Determine icon to display if any
	string img_name;
	string iconpath;
	string iconname = rclconfig->getMimeIconName(doc.mimetype, &iconpath);
	LOGDEB1(("Img file; %s\n", iconpath.c_str()));
	QImage image(iconpath.c_str());
	if (!image.isNull()) {
	    img_name = string("img_") + iconname;
	    QMimeSourceFactory::defaultFactory()->
		setImage(img_name.c_str(), image);
	}

	// Printable url: either utf-8 if transcoding succeeds, or url-encoded
	string url;
	printableUrl(rclconfig->getDefCharset(), doc.url, url);

	// Make title out of file name if none yet
	if (doc.meta[Rcl::Doc::keytt].empty()) {
	    doc.meta[Rcl::Doc::keytt] = path_getsimple(url);
	}

	// Result number
	char numbuf[20];
	int docnumforlinks = m_winfirst + 1 + i;
	sprintf(numbuf, "%d", docnumforlinks);

	// Document date: either doc or file modification time
	char datebuf[100];
	datebuf[0] = 0;
	if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
	    time_t mtime = doc.dmtime.empty() ?
		atol(doc.fmtime.c_str()) : atol(doc.dmtime.c_str());
	    struct tm *tm = localtime(&mtime);
#ifndef sun
	    strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z", tm);
#else
	    strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%Z", tm);
#endif
	}

	// Size information. We print both doc and file if they differ a lot
	long fsize = -1, dsize = -1;
	if (!doc.dbytes.empty())
	    dsize = atol(doc.dbytes.c_str());
	if (!doc.fbytes.empty())
	    fsize = atol(doc.fbytes.c_str());
	string sizebuf;
	if (dsize > 0) {
	    sizebuf = displayableBytes(dsize);
	    if (fsize > 10 * dsize && fsize - dsize > 1000)
		sizebuf += string(" / ") + displayableBytes(fsize);
	} else if (fsize >= 0) {
	    sizebuf = displayableBytes(fsize);
	}

	string abstract;
	if (prefs.queryBuildAbstract && 
	    (doc.syntabs || prefs.queryReplaceAbstract)) {
	    abstract = m_docSource->getAbstract(doc);
	} else {
	    abstract = doc.meta[Rcl::Doc::keyabs];
	}
	// No need to call escapeHtml(), plaintorich handles it
	list<string> lr;
	PlainToRichQtReslist ptr;
	ptr.plaintorich(abstract, lr, hdata);
	string richabst = lr.front();

	// Links;
	string linksbuf;
	char vlbuf[100];
	if (canIntern(doc.mimetype, rclconfig)) { 
	    sprintf(vlbuf, "\"P%d\"", docnumforlinks);
	    linksbuf += string("<a href=") + vlbuf + ">" + "Preview" + "</a>" 
		+ "&nbsp;&nbsp;";
	}
	if (!rclconfig->getMimeViewerDef(doc.mimetype).empty()) {
	    sprintf(vlbuf, "E%d", docnumforlinks);
	    linksbuf += string("<a href=") + vlbuf + ">" + "Open" + "</a>";
	}

	// Build the result list paragraph:
	chunk = "";

	// Subheader: this is used by history
	if (!sh.empty())
	    chunk += "<p><b>" + QString::fromUtf8(sh.c_str()) + "</p>\n<p>";
	else
	    chunk += "<p>";

	// Configurable stuff
	map<char,string> subs;
	subs['A'] = !richabst.empty() ? richabst + "<br>" : "";
	subs['D'] = datebuf;
	subs['I'] = img_name;
	subs['K'] = !doc.meta[Rcl::Doc::keykw].empty() ? 
	    escapeHtml(doc.meta[Rcl::Doc::keykw]) + "<br>" : "";
	subs['L'] = linksbuf;
	subs['N'] = numbuf;
	subs['M'] = doc.mimetype;
	subs['R'] = perbuf;
	subs['S'] = sizebuf;
	subs['T'] = escapeHtml(doc.meta[Rcl::Doc::keytt]);
	subs['U'] = url;

	string formatted;
	pcSubst(sformat, formatted, subs);
	chunk += QString::fromUtf8(formatted.c_str());

	chunk += "</p>\n";

	LOGDEB2(("Chunk: [%s]\n", (const char *)chunk.utf8()));
	append(chunk);
	setCursorPosition(0,0);
	ensureCursorVisible();

        m_pageParaToReldocnums[paragraphs()-1] = i;
	m_curDocs.push_back(doc);
    }

    // Footer
    chunk = "<p align=\"center\">";
    if (hasPrev || hasNext) {
	if (hasPrev) {
	    chunk += "<a href=\"p-1\"><b>";
	    chunk += tr("Previous");
	    chunk += "</b></a>&nbsp;&nbsp;&nbsp;";
	}
	if (hasNext) {
	    chunk += "<a href=\"n-1\"><b>";
	    chunk += tr("Next");
	    chunk += "</b></a>";
	}
    }
    chunk += "</p>\n";
    chunk += "</body></qt>\n";
    append(chunk);

    // Possibly color paragraph of current preview if any
    previewExposed(m_curPvDoc);
    ensureCursorVisible();
}

// Color paragraph (if any) of currently visible preview
void ResList::previewExposed(int docnum)
{
    LOGDEB(("ResList::previewExposed: doc %d\n", docnum));

    // Possibly erase old one to white
    int par;
    if (m_curPvDoc != -1 && (par = parnumfromdocnum(m_curPvDoc)) != -1) {
	QColor color("white");
	setParagraphBackgroundColor(par, color);
	m_curPvDoc = -1;
    }
    m_curPvDoc = docnum;
    par = parnumfromdocnum(docnum);
    // Maybe docnum is -1 or not in this window, 
    if (par < 0)
	return;

    setCursorPosition(par, 1);
    ensureCursorVisible();

    // Color the new active paragraph
    QColor color("LightBlue");
    setParagraphBackgroundColor(par, color);
}

// SELECTION BEWARE: these emit simple text if the list format is
// Auto. If we get back to using Rich for some reason, there will be
// need to extract the simple text here.

#if (QT_VERSION >= 0x040000)
// I have absolutely no idea why this nonsense is needed to get
// q3textedit to copy to x11 primary selection when text is
// selected. This used to be automatic, and, looking at the code, it
// should happen inside q3textedit (the code here is copied from the
// private copyToClipboard method). To be checked again with a later
// qt version. Seems to be needed at least up to 4.2.3
void ResList::selecChanged()
{
    QClipboard *clipboard = QApplication::clipboard();
    if (hasSelectedText()) {
        disconnect(QApplication::clipboard(), SIGNAL(selectionChanged()), 
		   this, 0);
	clipboard->setText(selectedText(), QClipboard::Selection);
        connect(QApplication::clipboard(), SIGNAL(selectionChanged()),
		this, SLOT(clipboardChanged()));
    }
}
#else
void ResList::selecChanged(){}
#endif

// Double click in res list: add selection to simple search
void ResList::doubleClicked(int, int)
{
    if (hasSelectedText())
	emit(wordSelect(selectedText()));
}

void ResList::linkWasClicked(const QString &s, int clkmod)
{
    LOGDEB(("ResList::linkWasClicked: [%s]\n", s.ascii()));
    int i = atoi(s.ascii()+1) -1;
    int what = s.ascii()[0];
    switch (what) {
    case 'H': 
	emit headerClicked(); 
	break;
    case 'P': 
	emit docPreviewClicked(i, clkmod); 
	break;
    case 'E': 
	emit docEditClicked(i);
	break;
    case 'n':
	resultPageNext();
	break;
    case 'p':
	resultPageBack();
	break;
    default: break;// ?? 
    }
}

RCLPOPUP *ResList::createPopupMenu(const QPoint& pos)
{
    int para = paragraphAt(pos);
    clicked(para, 0);
    m_popDoc = docnumfromparnum(para);
    if (m_popDoc < 0) 
	return 0;
    RCLPOPUP *popup = new RCLPOPUP(this);
    popup->insertItem(tr("&Preview"), this, SLOT(menuPreview()));
    popup->insertItem(tr("&Edit"), this, SLOT(menuEdit()));
    popup->insertItem(tr("Copy &File Name"), this, SLOT(menuCopyFN()));
    popup->insertItem(tr("Copy &URL"), this, SLOT(menuCopyURL()));
    popup->insertItem(tr("Find &similar documents"), this, SLOT(menuExpand()));
    popup->insertItem(tr("P&arent document/folder"), 
		      this, SLOT(menuSeeParent()));
    return popup;
}

void ResList::menuPreview()
{
    emit docPreviewClicked(m_popDoc, 0);
}

void ResList::menuSeeParent()
{
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
	Rcl::Doc doc1;
	if (FileInterner::getEnclosing(doc.url, doc.ipath, 
				       doc1.url, doc1.ipath)) {
	    emit previewRequested(doc1);
	} else {
	    // No parent doc: show enclosing folder with app configured for
	    // directories
	    doc1.url = path_getfather(doc.url);
	    doc1.mimetype = "application/x-fsdirectory";
	    emit editRequested(doc1);
	}
    }
}

void ResList::menuEdit()
{
    emit docEditClicked(m_popDoc);
}
void ResList::menuCopyFN()
{
    LOGDEB(("menuCopyFN\n"));
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc)) {
	LOGDEB(("menuCopyFN: Got doc, fn: [%s]\n", doc.url.c_str()));
	// Our urls currently always begin with "file://"
	QApplication::clipboard()->setText(doc.url.c_str()+7, 
					   QClipboard::Selection);
	QApplication::clipboard()->setText(doc.url.c_str()+7, 
					   QClipboard::Clipboard);
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
    emit docExpand(m_popDoc);
}

QString ResList::getDescription()
{
    return QString::fromUtf8(m_docSource->getDescription().c_str());
}

/** Show detailed expansion of a query */
void ResList::showQueryDetails()
{
    // Break query into lines of reasonable length, avoid cutting words,
    // Also limit the total number of lines. 
    const unsigned int ll = 100;
    const unsigned int maxlines = 50;
    string query = m_docSource->getDescription();
    string oq;
    unsigned int nlines = 0;
    while (query.length() > 0) {
	string ss = query.substr(0, ll);
	if (ss.length() == ll) {
	    string::size_type pos = ss.find_last_of(" ");
	    if (pos == string::npos) {
		pos = query.find_first_of(" ");
		if (pos != string::npos)
		    ss = query.substr(0, pos+1);
		else 
		    ss = query;
	    } else {
		ss = ss.substr(0, pos+1);
	    }
	}
	// This cant happen, but anyway. Be very sure to avoid an infinite loop
	if (ss.length() == 0) {
	    LOGDEB(("showQueryDetails: Internal error!\n"));
	    oq = query;
	    break;
	}
	oq += ss + "\n";
	if (nlines++ >= maxlines) {
	    oq += " ... \n";
	    break;
	}
	query= query.substr(ss.length());
	LOGDEB1(("oq [%s]\n, query [%s]\n, ss [%s]\n",
		oq.c_str(), query.c_str(), ss.c_str()));
    }

    QString desc = tr("Query details") + ": " + 
	QString::fromUtf8(oq.c_str());
    QMessageBox::information(this, tr("Query details"), desc);
}
