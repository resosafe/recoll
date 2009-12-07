#ifndef lint
static char rcsid[] = "@(#$Id: preview_w.cpp,v 1.39 2008-10-07 16:19:15 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include <list>
#include <utility>
#ifndef NO_NAMESPACES
using std::pair;
#endif /* NO_NAMESPACES */

#include <qmessagebox.h>
#include <qthread.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#if (QT_VERSION < 0x040000)
#include <qtextedit.h>
#include <qpopupmenu.h>
#include <qprogressdialog.h>
#define THRFINISHED finished
#else
#include <q3popupmenu.h>
#include <q3textedit.h>
#include <q3progressdialog.h>
#include <q3stylesheet.h>
#define THRFINISHED isFinished
#endif
#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qapplication.h>
#include <qclipboard.h>

#include "debuglog.h"
#include "pathut.h"
#include "internfile.h"
#include "recoll.h"
#include "plaintorich.h"
#include "smallut.h"
#include "wipedir.h"
#include "cancelcheck.h"
#include "preview_w.h"
#include "guiutils.h"

#if (QT_VERSION < 0x030300)
#define wasCanceled wasCancelled
#endif

#if (QT_VERSION < 0x040000)
#include <qtextedit.h>
#include <private/qrichtext_p.h>
#define QTEXTEDIT QTextEdit
#define QTEXTCURSOR QTextCursor
#define QTEXTPARAGRAPH QTextParagraph
#define QTEXTSTRINGCHAR QTextStringChar
#else
#include <q3textedit.h>
#include "q3richtext_p.h"
#define QTEXTEDIT Q3TextEdit
#define QTEXTCURSOR Q3TextCursor
#define QTEXTPARAGRAPH Q3TextParagraph
#define QTEXTSTRINGCHAR Q3TextStringChar
#endif

#include "rclhelp.h"

// QTextEdit's scrollToAnchor() is supposed to make the anchor visible, but
// actually, it only moves to the top of the paragraph containing the anchor.
// As we only have one paragraph, this doesnt' help a lot (qt3 and qt4)
//
// So, had to write a different function, inspired from what 
// qtextedit::find() does, instead. This ones actually moves the
// cursor, which is probably not necessary, but does what we need.
//
// Problem is, it uses the sem-private qrichtext_p.h, which is not
// even installed under qt4. We use a local copy, which is not nice.
void PreviewTextEdit::moveToAnchor(const QString& name)
{
    if (name.isEmpty())
	return;
    sync();
    QTEXTCURSOR l_cursor(document());
    QTEXTPARAGRAPH* last = document()->lastParagraph();
    for (;;) {
	QTEXTSTRINGCHAR* c = l_cursor.paragraph()->at(l_cursor.index());
	if(c->isAnchor()) {
	    QString a = c->anchorName();
	    if ( a == name ||
		 (a.contains( '#' ) && 
		  QStringList::split('#', a).contains(name))) {
		
		*(textCursor())  = l_cursor;
		ensureCursorVisible();
		break;
	    }
	}
	if (l_cursor.paragraph() == last && l_cursor.atParagEnd())
	    break;
	l_cursor.gotoNextLetter();
    }
}


#if (QT_VERSION >= 0x040000)

// Had to reimplement Qtextdocument::find() by duplicating the qt3
// version.  The version in qt4 qt3support was modified for some
// unknown reason and exhibits quadratic behaviour, and is totally
// unusable on big documents
static bool QTextDocument_find(Q3TextDocument *doc, Q3TextCursor& cursor, 
                               const QString &expr, bool cs, bool wo, 
                               bool forward)
{
    Qt::CaseSensitivity caseSensitive = cs ? Qt::CaseSensitive : 
        Qt::CaseInsensitive;

    doc->removeSelection(Q3TextDocument::Standard);
    Q3TextParagraph *p = 0;
    if ( expr.isEmpty() )
	return FALSE;
    for (;;) {
	if ( p != cursor.paragraph() ) {
	    p = cursor.paragraph();
	    QString s = cursor.paragraph()->string()->toString();
	    int start = cursor.index();
	    for ( ;; ) {
		int res = forward ? s.indexOf(expr, start, caseSensitive ) : 
                    s.lastIndexOf(expr, start, caseSensitive);
		int end = res + expr.length();
		if ( res == -1 || ( !forward && start <= res ) )
		    break;
		if (!wo || ((res == 0||s[res - 1].isSpace() || 
                             s[res - 1].isPunct()) &&
                            (end == (int)s.length() || s[end].isSpace() || 
                             s[end].isPunct()))) {
		    doc->removeSelection(Q3TextDocument::Standard);
		    cursor.setIndex( forward ? end : res );
		    doc->setSelectionStart(Q3TextDocument::Standard, cursor);
		    cursor.setIndex( forward ? res : end );
		    doc->setSelectionEnd(Q3TextDocument::Standard, cursor);
		    if ( !forward )
			cursor.setIndex( res );
		    return TRUE;
		}
		start = res + (forward ? 1 : -1);
	    }
	}
	if ( forward ) {
	    if ( cursor.paragraph() == doc->lastParagraph() && 
                 cursor.atParagEnd() )
		 break;
	    cursor.gotoNextLetter();
	} else {
	    if ( cursor.paragraph() == doc->firstParagraph() && 
                 cursor.atParagStart() )
		 break;
	    cursor.gotoPreviousLetter();
	}
    }
    return FALSE;

}

bool PreviewTextEdit::find(const QString &expr, bool cs, bool wo,
                           bool forward, int *para, int *index)
{
    drawCursor(false);
#ifndef QT_NO_CURSOR
    viewport()->setCursor(isReadOnly() ? Qt::ArrowCursor : Qt::IBeamCursor);
#endif
    Q3TextCursor findcur = *textCursor();
    if (para && index) {
        if (document()->paragAt(*para))
            findcur.gotoPosition(document()->paragAt(*para), *index);
        else
            findcur.gotoEnd();
    } else if (document()->hasSelection(Q3TextDocument::Standard)){
        // maks sure we do not find the same selection again
        if (forward)
            findcur.gotoNextLetter();
        else
            findcur.gotoPreviousLetter();
    } else if (!forward && findcur.index() == 0 && findcur.paragraph() == findcur.topParagraph()) {
        findcur.gotoEnd();
    }
    removeSelection(Q3TextDocument::Standard);
    bool found = QTextDocument_find(document(), findcur, expr, cs, wo, forward);
    if (found) {
        if (para)
            *para = findcur.paragraph()->paragId();
        if (index)
            *index = findcur.index();
        *textCursor() = findcur;
        repaintChanged();
        ensureCursorVisible();
    }
    drawCursor(true);
    if (found) {
        emit cursorPositionChanged(textCursor());
        emit cursorPositionChanged(textCursor()->paragraph()->paragId(), 
                                   textCursor()->index());
    }
    return found;
}

#endif

void Preview::init()
{
    setName("Preview");
    setSizePolicy( QSizePolicy((QSizePolicy::SizeType)5, 
			       (QSizePolicy::SizeType)5, 0, 0, 
			       sizePolicy().hasHeightForWidth()));
    QVBoxLayout* previewLayout =
	new QVBoxLayout( this, 4, 6, "previewLayout"); 

    pvTab = new QTabWidget(this, "pvTab");

    // Create the first tab. Should be possible to use addEditorTab
    // but this causes a pb with the sizeing
    QWidget *unnamed = new QWidget(pvTab, "unnamed");
    QVBoxLayout *unnamedLayout = 
	new QVBoxLayout(unnamed, 0, 6, "unnamedLayout"); 
    PreviewTextEdit *pvEdit = new PreviewTextEdit(unnamed, "pvEdit", this);
    pvEdit->setReadOnly(TRUE);
    pvEdit->setUndoRedoEnabled(FALSE);
    unnamedLayout->addWidget(pvEdit);
    pvTab->insertTab(unnamed, QString::fromLatin1(""));
    m_tabData.push_back(TabData(pvTab->currentPage()));

    previewLayout->addWidget(pvTab);

    // Create the buttons and entry field
    QHBoxLayout *layout3 = new QHBoxLayout(0, 0, 6, "layout3"); 
    searchLabel = new QLabel(this, "searchLabel");
    layout3->addWidget(searchLabel);
    searchTextLine = new QLineEdit(this, "searchTextLine");
    layout3->addWidget(searchTextLine);
    nextButton = new QPushButton(this, "nextButton");
    nextButton->setEnabled(TRUE);
    layout3->addWidget(nextButton);
    prevButton = new QPushButton(this, "prevButton");
    prevButton->setEnabled(TRUE);
    layout3->addWidget(prevButton);
    clearPB = new QPushButton(this, "clearPB");
    clearPB->setEnabled(FALSE);
    layout3->addWidget(clearPB);
    matchCheck = new QCheckBox(this, "matchCheck");
    layout3->addWidget(matchCheck);

    previewLayout->addLayout(layout3);

    resize(QSize(640, 480).expandedTo(minimumSizeHint()));

    // buddies
    searchLabel->setBuddy(searchTextLine);

    searchLabel->setText(tr("&Search for:"));
    nextButton->setText(tr("&Next"));
    prevButton->setText(tr("&Previous"));
    clearPB->setText(tr("Clear"));
    matchCheck->setText(tr("Match &Case"));

#if 0
    // Couldn't get a small button really in the corner. stays on the left of
    // the button area and looks ugly
    QPixmap px = QPixmap::fromMimeSource("cancel.png");
    QPushButton * bt = new QPushButton(px, "", this);
    bt->setFixedSize(px.size());
#else
    QPushButton * bt = new QPushButton(tr("Close Tab"), this);
#endif

    pvTab->setCornerWidget(bt);

    (void)new HelpClient(this);
    HelpClient::installMap(this->name(), "RCL.SEARCH.PREVIEW");

    // signals and slots connections
    connect(searchTextLine, SIGNAL(textChanged(const QString&)), 
	    this, SLOT(searchTextLine_textChanged(const QString&)));
    connect(nextButton, SIGNAL(clicked()), this, SLOT(nextPressed()));
    connect(prevButton, SIGNAL(clicked()), this, SLOT(prevPressed()));
    connect(clearPB, SIGNAL(clicked()), searchTextLine, SLOT(clear()));
    connect(pvTab, SIGNAL(currentChanged(QWidget *)), 
	    this, SLOT(currentChanged(QWidget *)));
    connect(bt, SIGNAL(clicked()), this, SLOT(closeCurrentTab()));

    m_dynSearchActive = false;
    m_canBeep = true;
    m_currentW = 0;
    if (prefs.pvwidth > 100) {
	resize(prefs.pvwidth, prefs.pvheight);
    }
    m_loading = false;
    currentChanged(pvTab->currentPage());
    m_justCreated = true;
    m_haveAnchors = false;
    m_curAnchor = 1;
}

void Preview::closeEvent(QCloseEvent *e)
{
    LOGDEB(("Preview::closeEvent. m_loading %d\n", m_loading));
    if (m_loading) {
	CancelCheck::instance().setCancel();
	e->ignore();
	return;
    }
    prefs.pvwidth = width();
    prefs.pvheight = height();
    emit previewExposed(this, m_searchId, -1);
    emit previewClosed(this);
    QWidget::closeEvent(e);
}

bool Preview::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) 
	return false;
    
    LOGDEB1(("Preview::eventFilter: keyEvent\n"));
    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Q && 
	(keyEvent->state() & Qt::ControlButton)) {
	recollNeedsExit = 1;
	return true;
    } else if (keyEvent->key() == Qt::Key_Escape) {
	close();
	return true;
    } else if (keyEvent->key() == Qt::Key_Down &&
	       (keyEvent->state() & Qt::ShiftButton)) {
	// LOGDEB(("Preview::eventFilter: got Shift-Up\n"));
	TabData *d = tabDataForCurrent();
	if (d) 
	    emit(showNext(this, m_searchId, d->docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_Up &&
	       (keyEvent->state() & Qt::ShiftButton)) {
	// LOGDEB(("Preview::eventFilter: got Shift-Down\n"));
	TabData *d = tabDataForCurrent();
	if (d) 
	    emit(showPrev(this, m_searchId, d->docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_W &&
	       (keyEvent->state() & Qt::ControlButton)) {
	// LOGDEB(("Preview::eventFilter: got ^W\n"));
	closeCurrentTab();
	return true;
    } else if (m_dynSearchActive) {
	if (keyEvent->key() == Qt::Key_F3) {
	    doSearch(searchTextLine->text(), true, false);
	    return true;
	}
	if (target != searchTextLine)
	    return QApplication::sendEvent(searchTextLine, event);
    } else {
	QWidget *tw = pvTab->currentPage();
	PreviewTextEdit *e = 0;
	if (tw)
	    e = (PreviewTextEdit *)tw->child("pvEdit");
	LOGDEB1(("Widget: %p, edit %p, target %p\n", tw, e, target));
	if (e && target == e) {
	    if (keyEvent->key() == Qt::Key_Slash) {
		searchTextLine->setFocus();
		m_dynSearchActive = true;
		return true;
	    } else if (keyEvent->key() == Qt::Key_Space) {
		e->scrollBy(0, e->visibleHeight());
		return true;
	    } else if (keyEvent->key() == Qt::Key_BackSpace) {
		e->scrollBy(0, -e->visibleHeight());
		return true;
	    }
	}
    }

    return false;
}

void Preview::searchTextLine_textChanged(const QString & text)
{
    LOGDEB1(("search line text changed. text: '%s'\n", text.ascii()));
    if (text.isEmpty()) {
	m_dynSearchActive = false;
	//	nextButton->setEnabled(false);
	//	prevButton->setEnabled(false);
	clearPB->setEnabled(false);
    } else {
	m_dynSearchActive = true;
	//	nextButton->setEnabled(true);
	//	prevButton->setEnabled(true);
	clearPB->setEnabled(true);
	doSearch(text, false, false);
    }
}

#if (QT_VERSION >= 0x040000)
#define QProgressDialog Q3ProgressDialog
#define QStyleSheetItem Q3StyleSheetItem
#endif

PreviewTextEdit *Preview::getCurrentEditor()
{
    QWidget *tw = pvTab->currentPage();
    PreviewTextEdit *edit = 0;
    if (tw) {
	edit = (PreviewTextEdit*)tw->child("pvEdit");
    }
    return edit;
}

// Perform text search. If next is true, we look for the next match of the
// current search, trying to advance and possibly wrapping around. If next is
// false, the search string has been modified, we search for the new string, 
// starting from the current position
void Preview::doSearch(const QString &_text, bool next, bool reverse, 
		       bool wordOnly)
{
    LOGDEB(("Preview::doSearch: text [%s] txtlen %d next %d rev %d word %d\n", 
	    (const char *)_text.utf8(), _text.length(), int(next), 
	    int(reverse), int(wordOnly)));
    QString text = _text;

    bool matchCase = matchCheck->isChecked();
    PreviewTextEdit *edit = getCurrentEditor();
    if (edit == 0) {
	// ??
	return;
    }

    if (text.isEmpty()) {
	if (m_haveAnchors == false)
	    return;
	if (reverse) {
	    if (m_curAnchor == 1)
		m_curAnchor = m_plaintorich.lastanchor;
	    else
		m_curAnchor--;
	} else {
	    if (m_curAnchor == m_plaintorich.lastanchor)
		m_curAnchor = 1;
	    else
		m_curAnchor++;
	}
	QString aname = 
	   QString::fromUtf8(m_plaintorich.termAnchorName(m_curAnchor).c_str());
	edit->moveToAnchor(aname);
	return;
    }

    // If next is false, the user added characters to the current
    // search string.  We need to reset the cursor position to the
    // start of the previous match, else incremental search is going
    // to look for the next occurrence instead of trying to lenghten
    // the current match
    if (!next) {
	int ps, is, pe, ie;
	edit->getSelection(&ps, &is, &pe, &ie);
	if (is > 0)
	    is--;
	else if (ps > 0)
	    ps--;
	LOGDEB(("Preview::doSearch: setting cursor to %d %d\n", ps, is));
	edit->setCursorPosition(ps, is);
    }

    LOGDEB(("Preview::doSearch: first find call\n"));
    bool found = edit->find(text, matchCase, wordOnly, !reverse, 0, 0);
    LOGDEB(("Preview::doSearch: first find call return\n"));
    // If not found, try to wrap around. 
    if (!found && next) { 
	LOGDEB(("Preview::doSearch: wrapping around\n"));
	int mspara, msindex;
	if (reverse) {
	    mspara = edit->paragraphs();
	    msindex = edit->paragraphLength(mspara);
	} else {
	    mspara = msindex = 0;
	}
	LOGDEB(("Preview::doSearch: 2nd find call\n"));
	found = edit->find(text,matchCase, false, !reverse, &mspara, &msindex);
	LOGDEB(("Preview::doSearch: 2nd find call return\n"));
    }

    if (found) {
	m_canBeep = true;
    } else {
	if (m_canBeep)
	    QApplication::beep();
	m_canBeep = false;
    }
    LOGDEB(("Preview::doSearch: return\n"));
}

void Preview::nextPressed()
{
    doSearch(searchTextLine->text(), true, false);
}

void Preview::prevPressed()
{
    doSearch(searchTextLine->text(), true, true);
}

// Called when user clicks on tab
void Preview::currentChanged(QWidget * tw)
{
    QWidget *edit = (QWidget *)tw->child("pvEdit");
    m_currentW = tw;
    LOGDEB1(("Preview::currentChanged(). Editor: %p\n", edit));
    
    if (edit == 0) {
	LOGERR(("Editor child not found\n"));
	return;
    }
    edit->setFocus();
    // Connect doubleclick but cleanup first just in case this was
    // already connected.
    disconnect(edit, SIGNAL(doubleClicked(int, int)), this, 0);
    connect(edit, SIGNAL(doubleClicked(int, int)), 
	    this, SLOT(textDoubleClicked(int, int)));
#if (QT_VERSION >= 0x040000)
    connect(edit, SIGNAL(selectionChanged()), this, SLOT(selecChanged()));
#endif
    tw->installEventFilter(this);
    edit->installEventFilter(this);
    TabData *d = tabDataForCurrent();
    if (d) 
	emit(previewExposed(this, m_searchId, d->docnum));
}

#if (QT_VERSION >= 0x040000)
// I have absolutely no idea why this nonsense is needed to get
// q3textedit to copy to x11 primary selection when text is
// selected. This used to be automatic, and, looking at the code, it
// should happen inside q3textedit (the code here is copied from the
// private copyToClipboard method). To be checked again with a later
// qt version.
void Preview::selecChanged()
{
    LOGDEB1(("Selection changed\n"));
    if (!m_currentW)
	return;
    PreviewTextEdit *edit = (PreviewTextEdit*)m_currentW->child("pvEdit");
    if (edit == 0) {
	LOGERR(("Editor child not found\n"));
	return;
    }
    QClipboard *clipboard = QApplication::clipboard();
    if (edit->hasSelectedText()) {
	LOGDEB1(("Copying [%s] to primary selection.Clipboard sel supp: %d\n", 
		(const char *)edit->selectedText().ascii(),
		clipboard->supportsSelection()));
        disconnect(QApplication::clipboard(), SIGNAL(selectionChanged()), 
		   edit, 0);
	clipboard->setText(edit->selectedText(), QClipboard::Selection);
        connect(QApplication::clipboard(), SIGNAL(selectionChanged()),
		edit, SLOT(clipboardChanged()));
    }
}
#else 
void Preview::selecChanged(){}
#endif

void Preview::textDoubleClicked(int, int)
{
    LOGDEB2(("Preview::textDoubleClicked\n"));
    if (!m_currentW)
	return;
    PreviewTextEdit *edit = (PreviewTextEdit *)m_currentW->child("pvEdit");
    if (edit == 0) {
	LOGERR(("Editor child not found\n"));
	return;
    }
    if (edit->hasSelectedText())
	emit(wordSelect(edit->selectedText()));
}

void Preview::closeCurrentTab()
{
    LOGDEB(("Preview::closeCurrentTab: m_loading %d\n", m_loading));
    if (m_loading) {
	CancelCheck::instance().setCancel();
	return;
    }
    if (pvTab->count() > 1) {
	QWidget *tw = pvTab->currentPage();
	if (!tw) 
	    return;
	pvTab->removePage(tw);
	// Have to remove from tab data list
	for (list<TabData>::iterator it = m_tabData.begin(); 
	     it != m_tabData.end(); it++) {
	    if (it->w == tw) {
		m_tabData.erase(it);
		return;
	    }
	}
    } else {
	close();
    }
}

PreviewTextEdit *Preview::addEditorTab()
{
    QWidget *anon = new QWidget((QWidget *)pvTab);
    QVBoxLayout *anonLayout = new QVBoxLayout(anon, 1, 1, "anonLayout"); 
    PreviewTextEdit *editor = new PreviewTextEdit(anon, "pvEdit", this);
    editor->setReadOnly(TRUE);
    editor->setUndoRedoEnabled(FALSE );
    anonLayout->addWidget(editor);
    pvTab->addTab(anon, "Tab");
    pvTab->showPage(anon);
    m_tabData.push_back(TabData(anon));
    return editor;
}

void Preview::setCurTabProps(const string &fn, const Rcl::Doc &doc,
			     int docnum)
{
    QString title;
    map<string,string>::const_iterator meta_it;
    if ((meta_it = doc.meta.find("title")) != doc.meta.end()) {
	    title = QString::fromUtf8(meta_it->second.c_str(), 
				      meta_it->second.length());
    }
    if (title.length() > 20) {
	title = title.left(10) + "..." + title.right(10);
    }
    QWidget *w = pvTab->currentPage();
    pvTab->changeTab(w, title);

    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.fmtime.empty() || !doc.dmtime.empty()) {
	time_t mtime = doc.dmtime.empty() ? 
	    atol(doc.fmtime.c_str()) : atol(doc.dmtime.c_str());
	struct tm *tm = localtime(&mtime);
	strftime(datebuf, 99, "%Y-%m-%d %H:%M:%S", tm);
    }
    LOGDEB(("Doc.url: [%s]\n", doc.url.c_str()));
    string url;
    printableUrl(rclconfig->getDefCharset(), doc.url, url);
    string tiptxt = url + string("\n");
    tiptxt += doc.mimetype + " " + string(datebuf) + "\n";
    if (meta_it != doc.meta.end() && !meta_it->second.empty())
	tiptxt += meta_it->second + "\n";
    pvTab->setTabToolTip(w,QString::fromUtf8(tiptxt.c_str(), tiptxt.length()));

    TabData *d = tabDataForCurrent();
    if (d) {
	d->fn = fn;
	d->ipath = doc.ipath;
	d->docnum = docnum;
    }
}

TabData *Preview::tabDataForCurrent()
{
    QWidget *w = pvTab->currentPage();
    if (w == 0)
	return 0;
    for (list<TabData>::iterator it = m_tabData.begin(); 
	 it != m_tabData.end(); it++) {
	if (it->w == w) {
	    return &(*it);
	}
    }
    return 0;
}

bool Preview::makeDocCurrent(const string &fn, size_t sz, 
			     const Rcl::Doc& doc, int docnum, bool sametab)
{
    LOGDEB(("Preview::makeDocCurrent: %s\n", fn.c_str()));
    for (list<TabData>::iterator it = m_tabData.begin(); 
	 it != m_tabData.end(); it++) {
	LOGDEB2(("Preview::makeFileCurrent: compare to w %p, file %s\n", 
		 it->w, it->fn.c_str()));
	if (!it->fn.compare(fn) && !it->ipath.compare(doc.ipath)) {
	    pvTab->showPage(it->w);
	    return true;
	}
    }
    // if just created the first tab was created during init
    if (!sametab && !m_justCreated && !addEditorTab()) {
	return false;
    }
    m_justCreated = false;
    if (!loadFileInCurrentTab(fn, sz, doc, docnum)) {
	closeCurrentTab();
	return false;
    }
    return true;
}

/*
  Code for loading a file into an editor window. The operations that
  we call have no provision to indicate progression, and it would be
  complicated or impossible to modify them to do so (Ie: for external 
  format converters).

  We implement a complicated and ugly mechanism based on threads to indicate 
  to the user that the app is doing things: lengthy operations are done in 
  threads and we update a progress indicator while they proceed (but we have 
  no estimate of their total duration).
  
  An auxiliary thread object is used for short waits. Another option would be 
  to use signals/slots and return to the event-loop instead, but this would
  be even more complicated, and we probably don't want the user to click on
  things during this time anyway.

  It might be possible, but complicated (need modifications in
  handler) to implement a kind of bucket brigade, to have the
  beginning of the text displayed faster
*/

/* A thread to to the file reading / format conversion */
class LoadThread : public QThread {
    int *statusp;
    Rcl::Doc *out;
    string filename;
    string ipath;
    string *mtype;
    string tmpdir;
    int loglevel;
 public: 
    string missing;
    LoadThread(int *stp, Rcl::Doc *odoc, string fn, string ip, string *mt) 
	: statusp(stp), out(odoc), filename(fn), ipath(ip), mtype(mt) 
	{
	    loglevel = DebugLog::getdbl()->getlevel();
	}
    ~LoadThread() {
	if (tmpdir.length()) {
	    wipedir(tmpdir);
	    rmdir(tmpdir.c_str());
	}
    }
    virtual void run() {
	DebugLog::getdbl()->setloglevel(loglevel);
	string reason;
	if (!maketmpdir(tmpdir, reason)) {
	    QMessageBox::critical(0, "Recoll",
				  Preview::tr("Cannot create temporary directory"));
	    LOGERR(("Preview: %s\n", reason.c_str()));
	    *statusp = -1;
	    return;
	}
	struct stat st;
	if (stat(filename.c_str(), &st) < 0) {
	    LOGERR(("Preview: can't stat [%s]\n", filename.c_str()));
	    QMessageBox::critical(0, "Recoll",
				  Preview::tr("File does not exist"));
	    *statusp = -1;
	    return;
	}
	
	FileInterner interner(filename, &st, rclconfig, tmpdir, mtype);
	// We don't set the interner's target mtype to html because we
	// do want the html filter to do its work: we won't use the
	// text, but we need the conversion to utf-8
	// interner.setTargetMType("text/html");
	try {
	    FileInterner::Status ret = interner.internfile(*out, ipath);
	    if (ret == FileInterner::FIDone || ret == FileInterner::FIAgain) {
		// FIAgain is actually not nice here. It means that the record
		// for the *file* of a multidoc was selected. Actually this
		// shouldn't have had a preview link at all, but we don't know
		// how to handle it now. Better to show the first doc than
		// a mysterious error. Happens when the file name matches a
		// a search term of course.
		*statusp = 0;
		if (prefs.previewHtml && !interner.get_html().empty()) {
		    out->text = interner.get_html();
		    out->mimetype = "text/html";
		}
	    } else {
		out->mimetype = interner.getMimetype();
		interner.getMissingExternal(missing);
		*statusp = -1;
	    }
	} catch (CancelExcept) {
	    *statusp = -1;
	}
    }
};

/* A thread to convert to rich text (mark search terms) */
class ToRichThread : public QThread {
    string &in;
    const HiliteData &hdata;
    list<string> &out;
    int loglevel;
    PlainToRichQtPreview& ptr;
 public:
    ToRichThread(string &i, const HiliteData& hd, list<string> &o, 
		 PlainToRichQtPreview& _ptr)
	: in(i), hdata(hd), out(o), ptr(_ptr)
    {
	    loglevel = DebugLog::getdbl()->getlevel();
    }
    virtual void run()
    {
	DebugLog::getdbl()->setloglevel(loglevel);
	try {
	    ptr.plaintorich(in, out, hdata);
	} catch (CancelExcept) {
	}
    }
};

/* A thread to implement short waiting. There must be a better way ! */
class WaiterThread : public QThread {
    int ms;
 public:
    WaiterThread(int millis) : ms(millis) {}
    virtual void run() {
	msleep(ms);
    }
};

#define CHUNKL 50*1000
#ifndef MIN
#define MIN(A,B) ((A)<(B)?(A):(B))
#endif
class LoadGuard {
    bool *m_bp;
public:
    LoadGuard(bool *bp) {m_bp = bp ; *m_bp = true;}
    ~LoadGuard() {*m_bp = false; CancelCheck::instance().setCancel(false);}
};

bool Preview::loadFileInCurrentTab(string fn, size_t sz, const Rcl::Doc &idoc,
				   int docnum)
{
    if (m_loading) {
	LOGERR(("ALready loading\n"));
	return false;
    }

    LoadGuard guard(&m_loading);
    CancelCheck::instance().setCancel(false);

    m_haveAnchors = false;

    Rcl::Doc doc = idoc;

    if (doc.meta[Rcl::Doc::keytt].empty()) 
	doc.meta[Rcl::Doc::keytt] = path_getsimple(doc.url);

    setCurTabProps(fn, doc, docnum);

    char csz[20];
    sprintf(csz, "%lu", (unsigned long)sz);
    QString msg = QString("Loading: %1 (size %2 bytes)")
	.arg(QString::fromLocal8Bit(fn.c_str()))
	.arg(csz);

    // Create progress dialog and aux objects
    const int nsteps = 20;
    QProgressDialog progress(msg, tr("Cancel"), nsteps, this, "Loading", FALSE);
    progress.setMinimumDuration(2000);
    WaiterThread waiter(100);

    // Load and convert file
    Rcl::Doc fdoc;
    // Need to setup config to retrieve possibly local parameters
    rclconfig->setKeyDir(path_getfather(fn));
    int status = 1;
    LoadThread lthr(&status, &fdoc, fn, doc.ipath, &doc.mimetype);
    lthr.start();
    int prog;
    for (prog = 1;;prog++) {
	waiter.start();
	waiter.wait();
	if (lthr.THRFINISHED ())
	    break;
	progress.setProgress(prog , prog <= nsteps-1 ? nsteps : prog+1);
	qApp->processEvents();
	if (progress.wasCanceled()) {
	    CancelCheck::instance().setCancel();
	}
	if (prog >= 5)
	    sleep(1);
    }

    LOGDEB(("LoadFileInCurrentTab: after file load: cancel %d status %d"
	    " text length %d\n", 
	    CancelCheck::instance().cancelState(), status, fdoc.text.length()));

    if (CancelCheck::instance().cancelState())
	return false;
    if (status != 0) {
	QString explain;
	if (!lthr.missing.empty()) {
	    explain = QString::fromAscii("<br>") +
		tr("Missing helper program: ") + 
		QString::fromLocal8Bit(lthr.missing.c_str());
	}
	QMessageBox::warning(0, "Recoll",
			     tr("Can't turn doc into internal "
				"representation for ") +
			     fdoc.mimetype.c_str() + explain);
	return false;
    }
    // Reset config just in case.
    rclconfig->setKeyDir("");

    // Create preview text: highlight search terms
    // We don't do the highlighting for very big texts: too long. We
    // should at least do special char escaping, in case a '&' or '<'
    // somehow slipped through previous processing.
    bool highlightTerms = fdoc.text.length() < 
	(unsigned long)prefs.maxhltextmbs * 1024 * 1024;

    // Final text is produced in chunks so that we can display the top
    // while still inserting at bottom
    list<QString> qrichlst;
    bool inputishtml = !fdoc.mimetype.compare("text/html");
    if (highlightTerms) {
	progress.setLabelText(tr("Creating preview text"));
	qApp->processEvents();
	if (inputishtml) {
	    LOGDEB1(("Preview: got html %s\n", fdoc.text.c_str()));
	    m_plaintorich.set_inputhtml(true);
	} else {
	    m_plaintorich.set_inputhtml(false);
	}
	list<string> richlst;
	ToRichThread rthr(fdoc.text, m_hData, richlst, m_plaintorich);
	rthr.start();

	for (;;prog++) {
	    waiter.start();	waiter.wait();
	    if (rthr.THRFINISHED ())
		break;
	    progress.setProgress(prog , prog <= nsteps-1 ? nsteps : prog+1);
	    qApp->processEvents();
	    if (progress.wasCanceled()) {
		CancelCheck::instance().setCancel();
	    }
	    if (prog >= 5)
		sleep(1);
	}

	// Conversion to rich text done
	if (CancelCheck::instance().cancelState()) {
	    if (richlst.size() == 0 || richlst.front().length() == 0) {
		// We cant call closeCurrentTab here as it might delete
		// the object which would be a nasty surprise to our
		// caller.
		return false;
	    } else {
		richlst.back() += "<b>Cancelled !</b>";
	    }
	}
	// Convert C++ string list to QString list
	for (list<string>::iterator it = richlst.begin(); 
	     it != richlst.end(); it++) {
	    qrichlst.push_back(QString::fromUtf8(it->c_str(), it->length()));
	}
    } else {
	LOGDEB(("Preview: no hilighting\n"));
	// No plaintorich() call.  In this case, either the text is
	// html and the html quoting is hopefully correct, or it's
	// plain-text and there is no need to escape special
	// characters. We'd still want to split in chunks (so that the
	// top is displayed faster), but we must not cut tags, and
	// it's too difficult on html. For text we do the splitting on
	// a QString to avoid utf8 issues.
	QString qr = QString::fromUtf8(fdoc.text.c_str(), fdoc.text.length());
	int l = 0;
	if (inputishtml) {
	    qrichlst.push_back(qr);
	} else {
	    for (int pos = 0; pos < (int)qr.length(); pos += l) {
		l = MIN(CHUNKL, qr.length() - pos);
		qrichlst.push_back(qr.mid(pos, l));
	    }
	}
    }
	    
    // Load into editor
    PreviewTextEdit *editor = getCurrentEditor();
    editor->setText("");
    if (highlightTerms) {
	QStyleSheetItem *item = 
	    new QStyleSheetItem(editor->styleSheet(), "termtag" );
	item->setColor(prefs.qtermcolor);
	item->setFontWeight(QFont::Bold);
    }

    prog = 2 * nsteps / 3;
    progress.setLabelText(tr("Loading preview text into editor"));
    qApp->processEvents();
    int instep = 0;
    for (list<QString>::iterator it = qrichlst.begin(); 
	 it != qrichlst.end(); it++, prog++, instep++) {
	progress.setProgress(prog , prog <= nsteps-1 ? nsteps : prog+1);
	qApp->processEvents();

	editor->append(*it);

	// Stay at top
	if (instep < 5) {
	    editor->setCursorPosition(0,0);
	    editor->ensureCursorVisible();
	}

	if (progress.wasCanceled()) {
            editor->append("<b>Cancelled !</b>");
	    LOGDEB(("LoadFileInCurrentTab: cancelled in editor load\n"));
	    break;
	}
    }

    progress.close();
    TabData *d = tabDataForCurrent();
    if (d) {
	fdoc.text.clear();
	d->fdoc = fdoc;
    }
    m_haveAnchors = m_plaintorich.lastanchor != 0;
    if (searchTextLine->text().length() != 0) {
	// If there is a current search string, perform the search
	m_canBeep = true;
	doSearch(searchTextLine->text(), true, false);
    } else {
	// Position to the first query term
	if (m_haveAnchors) {
	    QString aname = 
		QString::fromUtf8(m_plaintorich.termAnchorName(1).c_str());
	    LOGDEB2(("Call movetoanchor(%s)\n", (const char *)aname.utf8()));
	    editor->moveToAnchor(aname);
	    m_curAnchor = 1;
	}
    }

    // Enter document in document history
    g_dynconf->enterDoc(fn, doc.ipath);

    editor->setFocus();
    emit(previewExposed(this, m_searchId, docnum));
    LOGDEB(("LoadFileInCurrentTab: returning true\n"));
    return true;
}


RCLPOPUP *PreviewTextEdit::createPopupMenu(const QPoint&)
{
    RCLPOPUP *popup = new RCLPOPUP(this);
    if (m_savedText.isEmpty()) {
	popup->insertItem(tr("Show fields"), this, SLOT(menuToggleFields()));
    } else {
	popup->insertItem(tr("Show main text"), this, SLOT(menuToggleFields()));
    }
    return popup;
}

void PreviewTextEdit::menuToggleFields()
{
    if (!m_savedText.isEmpty()) {
	setText(m_savedText);
	m_savedText = "";
	return;
    }
    TabData *d = m_preview->tabDataForCurrent();
    if (!d) 
	return;
    QString txt = "<html><head></head><body>\n";
    txt += "<b>" + QString::fromLocal8Bit(d->fn.c_str());
    if (!d->ipath.empty())
	txt += "|" + QString::fromUtf8(d->ipath.c_str());
    txt += "</b><br><br>";
    txt += "<dl>\n";
    for (map<string,string>::const_iterator it = d->fdoc.meta.begin();
	 it != d->fdoc.meta.end(); it++) {
	txt += "<dt>" + QString::fromUtf8(it->first.c_str()) + "</dt> " 
	    + "<dd>" + QString::fromUtf8(it->second.c_str()) + "</dd>\n";
    }
    txt += "</dl></body></html>";
    m_savedText = text();
    setText(txt);
}
