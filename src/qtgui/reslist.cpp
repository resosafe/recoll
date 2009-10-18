#ifndef lint
static char rcsid[] = "@(#$Id: reslist.cpp,v 1.52 2008-12-17 15:12:08 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include "rclhelp.h"

#ifndef MIN
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

class PlainToRichQtReslist : public PlainToRich {
public:
    virtual ~PlainToRichQtReslist() {}
    virtual string startMatch() {return string("<termtag>");}
    virtual string endMatch() {return string("</termtag>");}
};
static PlainToRichQtReslist g_hiliter;

ResList::ResList(QWidget* parent, const char* name)
    : QTEXTBROWSER(parent, name)
{
    if (!name)
	setName("resList");
    setReadOnly(TRUE);
    setUndoRedoEnabled(FALSE);
    languageChange();

    setTabChangesFocus(true);

    (void)new HelpClient(this);
    HelpClient::installMap(this->name(), "RCL.SEARCH.RESLIST");

    // signals and slots connections
    connect(this, SIGNAL(linkClicked(const QString &, int)), 
	    this, SLOT(linkWasClicked(const QString &, int)));
    connect(this, SIGNAL(headerClicked()), this, SLOT(showQueryDetails()));
    connect(this, SIGNAL(doubleClicked(int,int)), 
	    this, SLOT(doubleClicked(int, int)));
#if (QT_VERSION >= 0x040000)
    connect(this, SIGNAL(selectionChanged()), this, SLOT(selecChanged()));
#endif
    m_curPvDoc = -1;
    m_lstClckMod = 0;
    m_listId = 0;
    m_pager = new QtGuiResListPager(this, prefs.respagesize);
    m_pager->setHighLighter(&g_hiliter);
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
    m_pager->setDocSource(m_docSource);
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
    if (m_docSource.isNull() || m_docSource->title().empty())
	return false;
    return m_docSource->title().find(htstring) == 0;
}

void ResList::languageChange()
{
    setCaption(tr("Result list"));
}

bool ResList::getTerms(vector<string>& terms, 
		       vector<vector<string> >& groups, vector<int>& gslks)
{
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
    if (m_pager->pageNumber() < 0)
	return -1;
    std::map<int,int>::iterator it = m_pageParaToReldocnums.find(par);
    int dn;
    if (it != m_pageParaToReldocnums.end()) {
        dn = m_pager->pageNumber() * prefs.respagesize + it->second;
    } else {
        dn = -1;
    }
    return dn;
}

// Get paragraph number from document number
int ResList::parnumfromdocnum(int docnum)
{
    if (m_pager->pageNumber() < 0)
	return -1;
    int winfirst = m_pager->pageNumber() * prefs.respagesize;
    if (docnum - winfirst < 0)
	return -1;
    docnum -= winfirst;
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
    LOGDEB(("ResList::getDoc: docnum %d winfirst %d\n", docnum, 
	    m_pager->pageNumber() * prefs.respagesize));
    if (docnum < 0)
	return false;
    if (m_pager->pageNumber() < 0)
	return false;
    int winfirst = m_pager->pageNumber() * prefs.respagesize;
    // Is docnum in current page ? Then all Ok
    if (docnum >= winfirst && docnum < winfirst + int(m_curDocs.size())) {
	doc = m_curDocs[docnum - winfirst];
	return true;
    }

    // Else we accept to page down or up but not further
    if (docnum < winfirst && docnum >= winfirst - prefs.respagesize) {
	resultPageBack();
    } else if (docnum < winfirst + int(m_curDocs.size()) + prefs.respagesize) {
	resultPageNext();
    }
    winfirst = m_pager->pageNumber() * prefs.respagesize;
    if (docnum >= winfirst && docnum < winfirst + int(m_curDocs.size())) {
	doc = m_curDocs[docnum - winfirst];
	return true;
    }
    return false;
}

void ResList::keyPressEvent(QKeyEvent * e)
{
    if (e->key() == Qt::Key_Q && (e->state() & Qt::ControlButton)) {
	recollNeedsExit = 1;
	return;
    } else if (e->key() == Qt::Key_Prior || e->key() == Qt::Key_Backspace) {
	resPageUpOrBack();
	return;
    } else if (e->key() == Qt::Key_Next || e->key() == Qt::Key_Space) {
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
    m_pager->resultPageBack();
    displayPage();
}

// Go to the first page
void ResList::resultPageFirst()
{
    m_pager->resultPageFirst();
    displayPage();
}

void ResList::append(const QString &text)
{
    // These has to go somewhere for translations to work
    static const char* strings[] = {
	QT_TR_NOOP("<p><b>No results found</b><br>"),
	QT_TR_NOOP("Documents <b>%d-%d</b> out of at least <b>%d</b> for "),
	QT_TR_NOOP("Documents <b>%d-%d</b> for "),
	QT_TR_NOOP("Previous"),
	QT_TR_NOOP("Next"),
	QT_TR_NOOP("Unavailable document"),
	QT_TR_NOOP("Preview"),
	QT_TR_NOOP("Open"),
	QT_TR_NOOP("(show query)"),
    };
    QTEXTBROWSER::append(text);
#if 0
    {
	FILE *fp = fopen("/tmp/debugreslist", "a");
	fprintf(fp, "%s\n", (const char *)text.utf8());
	fclose(fp);
    }
#endif
}

bool QtGuiResListPager::append(const string& data)
{
    LOGDEB1(("QtGuiReslistPager::append: %s\n", data.c_str()));
    m_parent->append(QString::fromUtf8(data.c_str()));
    return true;
}
bool QtGuiResListPager::append(const string& data, int i, const Rcl::Doc& doc)
{
    LOGDEB1(("QtGuiReslistPager::append: %d %s %s\n",
	     i, doc.url.c_str(), doc.ipath.c_str()));
    m_parent->setCursorPosition(0,0);
    m_parent->ensureCursorVisible();
    m_parent->m_pageParaToReldocnums[m_parent->paragraphs()] = i;
    m_parent->m_curDocs.push_back(doc);
    return append(data);
}

string QtGuiResListPager::trans(const string& in)
{
    return string((const char*)ResList::tr(in.c_str()).utf8());
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
string QtGuiResListPager::nextUrl()
{
    return "n-1";
}
string QtGuiResListPager::prevUrl()
{
    return "p-1";
}
string QtGuiResListPager::pageTop() 
{
    m_parent->clear();
    return string();
}

string QtGuiResListPager::iconPath(const string& mtype)
{
    string iconpath;
    RclConfig::getMainConfig()->getMimeIconName(mtype, &iconpath);
    return iconpath;
}

// Fill up result list window with next screen of hits
void ResList::resultPageNext()
{
    m_pager->resultPageNext();
    displayPage();
}

void ResList::displayPage()
{
    // Query term colorization
    static QStyleSheetItem *item;
    if (!item) {
	item = new QStyleSheetItem(styleSheet(), "termtag" );
    }
    if (item)
	item->setColor(prefs.qtermcolor);

    m_curDocs.clear();
    m_pageParaToReldocnums.clear();

    m_pager->displayPage();

    LOGDEB0(("ResList::resultPageNext: hasNext %d hasPrev %d\n",
	    m_pager->hasPrev(), m_pager->hasNext()));
    emit prevPageAvailable(m_pager->hasPrev());
    emit nextPageAvailable(m_pager->hasNext());
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
    LOGDEB2(("ResList::createPopupMenu(%d, %d)\n", pos.x(), pos.y()));
    int para = paragraphAt(pos);
    LOGDEB2(("ResList::createPopupMenu(): para %d\n", para));
    if (para == 0) {
        // There is a bug in qt3 paragraphAt() when the click is inside
        // a table.  paragraphAt() calls textcursor::place() which
        // fails because of a positioning problem inside the paragraph
        // (the paragraph is found but place() returns an error
        // anyway). Try to find the paragraph myself:
        for (;para < paragraphs(); para++) {
            QRect rect = paragraphRect(para);
            if (pos.y() >= rect.y() && pos.y() < rect.y() + rect.height())
                break;
        }
        if (para == paragraphs())
            para = 0;
    }
    m_popDoc = docnumfromparnum(para);
    if (m_popDoc < 0) 
	return 0;
    RCLPOPUP *popup = new RCLPOPUP(this);
    popup->insertItem(tr("&Preview"), this, SLOT(menuPreview()));
    popup->insertItem(tr("&Edit"), this, SLOT(menuEdit()));
    popup->insertItem(tr("Copy &File Name"), this, SLOT(menuCopyFN()));
    popup->insertItem(tr("Copy &URL"), this, SLOT(menuCopyURL()));
    Rcl::Doc doc;
    if (getDoc(m_popDoc, doc) && !doc.ipath.empty()) {
	popup->insertItem(tr("Save to File"), this, SLOT(menuSaveToFile()));
    }

    popup->insertItem(tr("Find &similar documents"), this, SLOT(menuExpand()));
    popup->insertItem(tr("P&arent document/folder"), 
		      this, SLOT(menuSeeParent()));
    return popup;
}

void ResList::menuPreview()
{
    emit docPreviewClicked(m_popDoc, 0);
}
void ResList::menuSaveToFile()
{
    emit docSaveToFileClicked(m_popDoc);
}

void ResList::menuSeeParent()
{
    Rcl::Doc doc;
    if (!getDoc(m_popDoc, doc)) 
	return;
    Rcl::Doc doc1;
    if (FileInterner::getEnclosing(doc.url, doc.ipath, doc1.url, doc1.ipath)) {
	emit previewRequested(doc1);
    } else {
	// No parent doc: show enclosing folder with app configured for
	// directories
	doc1.url = path_getfather(doc.url);
	doc1.mimetype = "application/x-fsdirectory";
	emit editRequested(doc1);
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
    string oq = breakIntoLines(m_docSource->getDescription(), 100, 50);
    QString desc = tr("Query details") + ": " + QString::fromUtf8(oq.c_str());
    QMessageBox::information(this, tr("Query details"), desc);
}
