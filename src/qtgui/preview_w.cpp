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
#include <qprinter.h>
#include <qprintdialog.h>
#include <qscrollbar.h>
#include <qmenu.h>
#include <qtextedit.h>
#include <qprogressdialog.h>
#include <qevent.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qimage.h>
#include <qurl.h>

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
#include "docseqhist.h"
#include "rclhelp.h"

#ifndef MIN
#define MIN(A,B) ((A)<(B)?(A):(B))
#endif

// Subclass plainToRich to add <termtag>s and anchors to the preview text
class PlainToRichQtPreview : public PlainToRich {
public:
    int lastanchor;
    PlainToRichQtPreview() 
    {
	lastanchor = 0;
    }    
    virtual ~PlainToRichQtPreview() {}
    virtual string header() {
	if (m_inputhtml) {
	    return snull;
	} else {
	    if (prefs.previewPlainPre) {
		m_eolbr = false;
		return string("<qt><head><title></title></head><body><pre>");
	    } else {
		m_eolbr = true;
		return string("<qt><head><title></title></head><body>");
	    }
	}
    }
    virtual string startMatch() 
    {
	return string("<span style='color: ")
	    + string((const char *)(prefs.qtermcolor.toUtf8()))
	    + string(";font-weight: bold;")
	    + string("'>");
    }
    virtual string endMatch() {return string("</span>");}
    virtual string termAnchorName(int i) {
	static const char *termAnchorNameBase = "TRM";
	char acname[sizeof(termAnchorNameBase) + 20];
	sprintf(acname, "%s%d", termAnchorNameBase, i);
	if (i > lastanchor)
	    lastanchor = i;
	return string(acname);
    }

    virtual string startAnchor(int i) {
	return string("<a name=\"") + termAnchorName(i) + "\">";
    }
    virtual string endAnchor() {
	return string("</a>");
    }
    virtual string startChunk() { return "<pre>";}
};

void Preview::init()
{
    setObjectName("Preview");
    QVBoxLayout* previewLayout = new QVBoxLayout(this);

    pvTab = new QTabWidget(this);

    // Create the first tab. Should be possible to use addEditorTab
    // but this causes a pb with the sizeing
    QWidget *unnamed = new QWidget(pvTab);
    QVBoxLayout *unnamedLayout = new QVBoxLayout(unnamed);
    PreviewTextEdit *pvEdit = new PreviewTextEdit(unnamed, "pvEdit", this);
    pvEdit->setReadOnly(TRUE);
    pvEdit->setUndoRedoEnabled(FALSE);
    unnamedLayout->addWidget(pvEdit);
    pvTab->addTab(unnamed, "");

    previewLayout->addWidget(pvTab);

    // Create the buttons and entry field
    QHBoxLayout *layout3 = new QHBoxLayout(0); 
    searchLabel = new QLabel(this);
    layout3->addWidget(searchLabel);
    searchTextLine = new QLineEdit(this);
    layout3->addWidget(searchTextLine);
    nextButton = new QPushButton(this);
    nextButton->setEnabled(TRUE);
    layout3->addWidget(nextButton);
    prevButton = new QPushButton(this);
    prevButton->setEnabled(TRUE);
    layout3->addWidget(prevButton);
    clearPB = new QPushButton(this);
    clearPB->setEnabled(FALSE);
    layout3->addWidget(clearPB);
    matchCheck = new QCheckBox(this);
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

    QPushButton * bt = new QPushButton(tr("Close Tab"), this);
    pvTab->setCornerWidget(bt);

    (void)new HelpClient(this);
    HelpClient::installMap((const char *)objectName().toUtf8(), 
			   "RCL.SEARCH.PREVIEW");

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
    currentChanged(pvTab->currentWidget());
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

    /* Release all temporary files (but maybe none is actually set) */
    for (int i = 0; i < pvTab->count(); i++) {
        QWidget *tw = pvTab->widget(i);
        if (tw) {
	    PreviewTextEdit *edit = 
		tw->findChild<PreviewTextEdit*>("pvEdit");
            if (edit) {
		forgetTempFile(edit->m_tmpfilename);
            }
        }
    }
    emit previewExposed(this, m_searchId, -1);
    emit previewClosed(this);
    QWidget::closeEvent(e);
}

extern const char *eventTypeToStr(int tp);

bool Preview::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() != QEvent::KeyPress) {
#if 0
    LOGDEB(("Preview::eventFilter(): %s\n", eventTypeToStr(event->type())));
	if (event->type() == QEvent::MouseButtonRelease) {
	    QMouseEvent *mev = (QMouseEvent *)event;
	    LOGDEB(("Mouse: GlobalY %d y %d\n", mev->globalY(),
		    mev->y()));
	}
#endif
	return false;
    }
    
    LOGDEB2(("Preview::eventFilter: keyEvent\n"));

    PreviewTextEdit *edit = currentEditor();
    QKeyEvent *keyEvent = (QKeyEvent *)event;
    if (keyEvent->key() == Qt::Key_Escape) {
	close();
	return true;
    } else if (keyEvent->key() == Qt::Key_Down &&
	       (keyEvent->modifiers() & Qt::ShiftModifier)) {
	LOGDEB2(("Preview::eventFilter: got Shift-Up\n"));
	if (edit) 
	    emit(showNext(this, m_searchId, edit->m_docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_Up &&
	       (keyEvent->modifiers() & Qt::ShiftModifier)) {
	LOGDEB2(("Preview::eventFilter: got Shift-Down\n"));
	if (edit) 
	    emit(showPrev(this, m_searchId, edit->m_docnum));
	return true;
    } else if (keyEvent->key() == Qt::Key_W &&
	       (keyEvent->modifiers() & Qt::ControlModifier)) {
	LOGDEB2(("Preview::eventFilter: got ^W\n"));
	closeCurrentTab();
	return true;
    } else if (keyEvent->key() == Qt::Key_P &&
	       (keyEvent->modifiers() & Qt::ControlModifier)) {
	LOGDEB2(("Preview::eventFilter: got ^P\n"));
	emit(printCurrentPreviewRequest());
	return true;
    } else if (m_dynSearchActive) {
	if (keyEvent->key() == Qt::Key_F3) {
	    LOGDEB2(("Preview::eventFilter: got F3\n"));
	    doSearch(searchTextLine->text(), true, false);
	    return true;
	}
	if (target != searchTextLine)
	    return QApplication::sendEvent(searchTextLine, event);
    } else {
	if (edit && 
	    (target == edit || target == edit->viewport())) {
	    if (keyEvent->key() == Qt::Key_Slash ||
		(keyEvent->key() == Qt::Key_F &&
		 (keyEvent->modifiers() & Qt::ControlModifier))) {
		LOGDEB2(("Preview::eventFilter: got / or C-F\n"));
		searchTextLine->setFocus();
		m_dynSearchActive = true;
		return true;
	    } else if (keyEvent->key() == Qt::Key_Space) {
		LOGDEB2(("Preview::eventFilter: got Space\n"));
		int value = edit->verticalScrollBar()->value();
		value += edit->verticalScrollBar()->pageStep();
		edit->verticalScrollBar()->setValue(value);
		return true;
	    } else if (keyEvent->key() == Qt::Key_Backspace) {
		LOGDEB2(("Preview::eventFilter: got Backspace\n"));
		int value = edit->verticalScrollBar()->value();
		value -= edit->verticalScrollBar()->pageStep();
		edit->verticalScrollBar()->setValue(value);
		return true;
	    }
	}
    }

    return false;
}

void Preview::searchTextLine_textChanged(const QString & text)
{
    LOGDEB2(("search line text changed. text: '%s'\n", text.ascii()));
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

PreviewTextEdit *Preview::currentEditor()
{
    LOGDEB2(("Preview::currentEditor()\n"));
    QWidget *tw = pvTab->currentWidget();
    PreviewTextEdit *edit = 0;
    if (tw) {
	edit = tw->findChild<PreviewTextEdit*>("pvEdit");
    }
    return edit;
}

// Save current document to file
void Preview::emitSaveDocToFile()
{
    PreviewTextEdit *ce = currentEditor();
    if (ce && !ce->m_dbdoc.url.empty()) {
	emit saveDocToFile(ce->m_dbdoc);
    }
}

// Perform text search. If next is true, we look for the next match of the
// current search, trying to advance and possibly wrapping around. If next is
// false, the search string has been modified, we search for the new string, 
// starting from the current position
void Preview::doSearch(const QString &_text, bool next, bool reverse, 
		       bool wordOnly)
{
    LOGDEB(("Preview::doSearch: text [%s] txtlen %d next %d rev %d word %d\n", 
             (const char *)_text.toUtf8(), _text.length(), int(next), 
             int(reverse), int(wordOnly)));
    QString text = _text;

    bool matchCase = matchCheck->isChecked();
    PreviewTextEdit *edit = currentEditor();
    if (edit == 0) {
	// ??
	return;
    }

    if (text.isEmpty()) {
	if (m_haveAnchors == false) {
	    LOGDEB(("NO ANCHORS\n"));
	    return;
	}
	if (reverse) {
	    if (m_curAnchor == 1)
		m_curAnchor = edit->m_plaintorich->lastanchor;
	    else
		m_curAnchor--;
	} else {
	    if (m_curAnchor == edit->m_plaintorich->lastanchor)
		m_curAnchor = 1;
	    else
		m_curAnchor++;
	}
	LOGDEB(("m_curAnchor: %d\n", m_curAnchor));
	QString aname = 
	   QString::fromUtf8(edit->m_plaintorich->termAnchorName(m_curAnchor).c_str());
	LOGDEB(("Calling scrollToAnchor(%s)\n", (const char *)aname.toUtf8()));
	edit->scrollToAnchor(aname);
	// Position the cursor approximately at the anchor (top of
	// viewport) so that searches start from here
	QTextCursor cursor = edit->cursorForPosition(QPoint(0, 0));
	edit->setTextCursor(cursor);
	return;
    }

    // If next is false, the user added characters to the current
    // search string.  We need to reset the cursor position to the
    // start of the previous match, else incremental search is going
    // to look for the next occurrence instead of trying to lenghten
    // the current match
    if (!next) {
	QTextCursor cursor = edit->textCursor();
	cursor.setPosition(cursor.anchor(), QTextCursor::KeepAnchor);
	edit->setTextCursor(cursor);
    }
    Chrono chron;
    LOGDEB(("Preview::doSearch: first find call\n"));
    QTextDocument::FindFlags flags = 0;
    if (reverse)
	flags |= QTextDocument::FindBackward;
    if (wordOnly)
	flags |= QTextDocument::FindWholeWords;
    if (matchCase)
	flags |= QTextDocument::FindCaseSensitively;
    bool found = edit->find(text, flags);
    LOGDEB(("Preview::doSearch: first find call return: found %d %.2f S\n", 
            found, chron.secs()));
    // If not found, try to wrap around. 
    if (!found) { 
	LOGDEB(("Preview::doSearch: wrapping around\n"));
	if (reverse) {
	    edit->moveCursor (QTextCursor::End);
	} else {
	    edit->moveCursor (QTextCursor::Start);
	}
	LOGDEB(("Preview::doSearch: 2nd find call\n"));
        chron.restart();
	found = edit->find(text, flags);
	LOGDEB(("Preview::doSearch: 2nd find call return found %d %.2f S\n",
                found, chron.secs()));
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
    LOGDEB2(("PreviewTextEdit::nextPressed\n"));
    doSearch(searchTextLine->text(), true, false);
}

void Preview::prevPressed()
{
    LOGDEB2(("PreviewTextEdit::prevPressed\n"));
    doSearch(searchTextLine->text(), true, true);
}

// Called when user clicks on tab
void Preview::currentChanged(QWidget * tw)
{
    LOGDEB2(("PreviewTextEdit::currentChanged\n"));
    PreviewTextEdit *edit = 
	tw->findChild<PreviewTextEdit*>("pvEdit");
    m_currentW = tw;
    LOGDEB1(("Preview::currentChanged(). Editor: %p\n", edit));
    
    if (edit == 0) {
	LOGERR(("Editor child not found\n"));
	return;
    }
    edit->setFocus();
    // Disconnect the print signal and reconnect it to the current editor
    LOGDEB(("Disconnecting reconnecting print signal\n"));
    disconnect(this, SIGNAL(printCurrentPreviewRequest()), 0, 0);
    connect(this, SIGNAL(printCurrentPreviewRequest()), edit, SLOT(print()));
    edit->installEventFilter(this);
    edit->viewport()->installEventFilter(this);
    searchTextLine->installEventFilter(this);
    emit(previewExposed(this, m_searchId, edit->m_docnum));
}

void Preview::closeCurrentTab()
{
    LOGDEB1(("Preview::closeCurrentTab: m_loading %d\n", m_loading));
    if (m_loading) {
	CancelCheck::instance().setCancel();
	return;
    }
    PreviewTextEdit *e = currentEditor();
    if (e)
	forgetTempFile(e->m_tmpfilename);
    if (pvTab->count() > 1) {
	pvTab->removeTab(pvTab->currentIndex());
    } else {
	close();
    }
}

PreviewTextEdit *Preview::addEditorTab()
{
    LOGDEB1(("PreviewTextEdit::addEditorTab()\n"));
    QWidget *anon = new QWidget((QWidget *)pvTab);
    QVBoxLayout *anonLayout = new QVBoxLayout(anon); 
    PreviewTextEdit *editor = new PreviewTextEdit(anon, "pvEdit", this);
    editor->setReadOnly(TRUE);
    editor->setUndoRedoEnabled(FALSE );
    anonLayout->addWidget(editor);
    pvTab->addTab(anon, "Tab");
    pvTab->setCurrentIndex(pvTab->count() -1);
    return editor;
}

void Preview::setCurTabProps(const Rcl::Doc &doc, int docnum)
{
    LOGDEB1(("PreviewTextEdit::setCurTabProps\n"));
    QString title;
    map<string,string>::const_iterator meta_it;
    if ((meta_it = doc.meta.find(Rcl::Doc::keytt)) != doc.meta.end()
        && !meta_it->second.empty()) {
	    title = QString::fromUtf8(meta_it->second.c_str(), 
				      meta_it->second.length());
    } else {
        title = QString::fromLocal8Bit(path_getsimple(doc.url).c_str());
    }
    if (title.length() > 20) {
	title = title.left(10) + "..." + title.right(10);
    }
    int curidx = pvTab->currentIndex();
    pvTab->setTabText(curidx, title);

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
    printableUrl(theconfig->getDefCharset(), doc.url, url);
    string tiptxt = url + string("\n");
    tiptxt += doc.mimetype + " " + string(datebuf) + "\n";
    if (meta_it != doc.meta.end() && !meta_it->second.empty())
	tiptxt += meta_it->second + "\n";
    pvTab->setTabToolTip(curidx,
			 QString::fromUtf8(tiptxt.c_str(), tiptxt.length()));

    PreviewTextEdit *e = currentEditor();
    if (e) {
	e->m_url = doc.url;
	e->m_ipath = doc.ipath;
	e->m_docnum = docnum;
    }
}

bool Preview::makeDocCurrent(const Rcl::Doc& doc, int docnum, bool sametab)
{
    LOGDEB(("Preview::makeDocCurrent: %s\n", doc.url.c_str()));

    if (m_loading) {
	LOGERR(("Already loading\n"));
	return false;
    }

    /* Check if we already have this page */
    for (int i = 0; i < pvTab->count(); i++) {
        QWidget *tw = pvTab->widget(i);
        if (tw) {
	    PreviewTextEdit *edit = 
		tw->findChild<PreviewTextEdit*>("pvEdit");
            if (edit && !edit->m_url.compare(doc.url) && 
                !edit->m_ipath.compare(doc.ipath)) {
                pvTab->setCurrentIndex(i);
                return true;
            }
        }
    }

    // if just created the first tab was created during init
    if (!sametab && !m_justCreated && !addEditorTab()) {
	return false;
    }
    m_justCreated = false;
    if (!loadDocInCurrentTab(doc, docnum)) {
	closeCurrentTab();
	return false;
    }
    raise();
    return true;
}

void Preview::emitWordSelect(QString word)
{
    emit(wordSelect(word));
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
  
  It might be possible, but complicated (need modifications in
  handler) to implement a kind of bucket brigade, to have the
  beginning of the text displayed faster
*/

/* A thread to to the file reading / format conversion */
class LoadThread : public QThread {
    int *statusp;
    Rcl::Doc& out;
    const Rcl::Doc& idoc;
    TempDir tmpdir;
    int loglevel;
 public: 
    string missing;
    TempFile imgtmp;
    string tdirreason;

    LoadThread(int *stp, Rcl::Doc& odoc, const Rcl::Doc& idc) 
	: statusp(stp), out(odoc), idoc(idc)
	{
	    loglevel = DebugLog::getdbl()->getlevel();
	}
    ~LoadThread() {
    }
    virtual void run() {
	DebugLog::getdbl()->setloglevel(loglevel);
	if (!tmpdir.ok()) {
	    tdirreason = tmpdir.getreason();
	    LOGERR(("Preview: %s\n", tdirreason.c_str()));
	    *statusp = -1;
	    return;
	}

	FileInterner interner(idoc, theconfig, tmpdir, 
                              FileInterner::FIF_forPreview);
	FIMissingStore mst;
	interner.setMissingStore(&mst);
	// We don't set the interner's target mtype to html because we
	// do want the html filter to do its work: we won't use the
	// text, but we need the conversion to utf-8
	// interner.setTargetMType("text/html");
	try {
            string ipath = idoc.ipath;
	    FileInterner::Status ret = interner.internfile(out, ipath);
	    if (ret == FileInterner::FIDone || ret == FileInterner::FIAgain) {
		// FIAgain is actually not nice here. It means that the record
		// for the *file* of a multidoc was selected. Actually this
		// shouldn't have had a preview link at all, but we don't know
		// how to handle it now. Better to show the first doc than
		// a mysterious error. Happens when the file name matches a
		// a search term.
		*statusp = 0;
		// If we prefer html and it is available, replace the
		// text/plain document text
		if (prefs.previewHtml && !interner.get_html().empty()) {
		    out.text = interner.get_html();
		    out.mimetype = "text/html";
		}
		imgtmp = interner.get_imgtmp();
	    } else {
		out.mimetype = interner.getMimetype();
		interner.getMissingExternal(&mst, missing);
		*statusp = -1;
	    }
	} catch (CancelExcept) {
	    *statusp = -1;
	}
    }
};


// Insert into editor by chunks so that the top becomes visible
// earlier for big texts. This provokes some artifacts (adds empty line),
// so we can't set it too low.
#define CHUNKL 500*1000

/* A thread to convert to rich text (mark search terms) */
class ToRichThread : public QThread {
    string &in;
    const HiliteData &hdata;
    list<string> &out;
    int loglevel;
    PlainToRichQtPreview *ptr;
 public:
    ToRichThread(string &i, const HiliteData& hd, list<string> &o, 
		 PlainToRichQtPreview *_ptr)
	: in(i), hdata(hd), out(o), ptr(_ptr)
    {
	    loglevel = DebugLog::getdbl()->getlevel();
    }
    virtual void run()
    {
	DebugLog::getdbl()->setloglevel(loglevel);
	try {
	    ptr->plaintorich(in, out, hdata, CHUNKL);
	} catch (CancelExcept) {
	}
    }
};

class LoadGuard {
    bool *m_bp;
public:
    LoadGuard(bool *bp) {m_bp = bp ; *m_bp = true;}
    ~LoadGuard() {*m_bp = false; CancelCheck::instance().setCancel(false);}
};

bool Preview::loadDocInCurrentTab(const Rcl::Doc &idoc, int docnum)
{
    LOGDEB1(("PreviewTextEdit::loadDocInCurrentTab()\n"));

    LoadGuard guard(&m_loading);
    CancelCheck::instance().setCancel(false);

    m_haveAnchors = false;

    setCurTabProps(idoc, docnum);

    QString msg = QString("Loading: %1 (size %2 bytes)")
	.arg(QString::fromLocal8Bit(idoc.url.c_str()))
	.arg(QString::fromAscii(idoc.fbytes.c_str()));

    // Create progress dialog and aux objects
    const int nsteps = 20;
    QProgressDialog progress(msg, tr("Cancel"), 0, nsteps, this);
    progress.setMinimumDuration(2000);

    ////////////////////////////////////////////////////////////////////////
    // Load and convert document
    // idoc came out of the index data (main text and other fields missing). 
    // foc is the complete one what we are going to extract from storage.
    Rcl::Doc fdoc;
    int status = 1;
    LoadThread lthr(&status, fdoc, idoc);
    lthr.start();
    int prog;
    for (prog = 1;;prog++) {
	if (lthr.wait(100))
	    break;
	progress.setValue(prog);
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
	if (!lthr.tdirreason.empty()) {
            explain = tr("Cannot create temporary directory: ") +
                QString::fromLocal8Bit(lthr.tdirreason.c_str());
	    QMessageBox::critical(0, "Recoll", explain);
	} else if (!lthr.missing.empty()) {
            explain = QString::fromAscii("<br>") +
                tr("Missing helper program: ") +
                QString::fromLocal8Bit(lthr.missing.c_str());
	    QMessageBox::warning(0, "Recoll",
				 tr("Can't turn doc into internal "
				    "representation for ") +
				 fdoc.mimetype.c_str() + explain);
        } else {
	    QMessageBox::warning(0, "Recoll", 
				  tr("Error while loading file"));
	}

	return false;
    }
    // Reset config just in case.
    theconfig->setKeyDir("");

    ////////////////////////////////////////////////////////////////////////
    // Create preview text: highlight search terms
    // We don't do the highlighting for very big texts: too long. We
    // should at least do special char escaping, in case a '&' or '<'
    // somehow slipped through previous processing.
    bool highlightTerms = fdoc.text.length() < 
	(unsigned long)prefs.maxhltextmbs * 1024 * 1024;

    // Final text is produced in chunks so that we can display the top
    // while still inserting at bottom
    list<QString> qrichlst;
    PreviewTextEdit *editor = currentEditor();
    editor->setHtml("");
    editor->m_format = Qt::RichText;
    bool inputishtml = !fdoc.mimetype.compare("text/html");

#if 0
    // For testing qtextedit bugs...
    highlightTerms = true;
    const char *textlist[] =
    {
        "Du plain text avec un\n <termtag>termtag</termtag> fin de ligne:",
        "texte apres le tag\n",
    };
    const int listl = sizeof(textlist) / sizeof(char*);
    for (int i = 0 ; i < listl ; i++)
        qrichlst.push_back(QString::fromUtf8(textlist[i]));
#else
    if (highlightTerms) {
	progress.setLabelText(tr("Creating preview text"));
	qApp->processEvents();

	if (inputishtml) {
	    LOGDEB1(("Preview: got html %s\n", fdoc.text.c_str()));
	    editor->m_plaintorich->set_inputhtml(true);
	} else {
	    LOGDEB1(("Preview: got plain %s\n", fdoc.text.c_str()));
	    editor->m_plaintorich->set_inputhtml(false);
	}
	list<string> richlst;
	ToRichThread rthr(fdoc.text, m_hData, richlst, editor->m_plaintorich);
	rthr.start();

	for (;;prog++) {
	    if (rthr.wait(100))
		break;
	    progress.setValue(nsteps);
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
		// We can't call closeCurrentTab here as it might delete
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
            editor->setPlainText("");
            editor->m_format = Qt::PlainText;
	    for (int pos = 0; pos < (int)qr.length(); pos += l) {
		l = MIN(CHUNKL, qr.length() - pos);
		qrichlst.push_back(qr.mid(pos, l));
	    }
	}
    }
#endif



    ///////////////////////////////////////////////////////////
    // Load text into editor window.
    prog = 2 * nsteps / 3;
    progress.setLabelText(tr("Loading preview text into editor"));
    qApp->processEvents();
    int instep = 0;
    for (list<QString>::iterator it = qrichlst.begin(); 
	 it != qrichlst.end(); it++, prog++, instep++) {
	progress.setValue(prog);
	qApp->processEvents();

	editor->append(*it);
        // We need to save the rich text for printing, the editor does
        // not do it consistently for us.
        editor->m_richtxt.append(*it);

	if (progress.wasCanceled()) {
            editor->append("<b>Cancelled !</b>");
	    LOGDEB(("LoadFileInCurrentTab: cancelled in editor load\n"));
	    break;
	}
    }

    progress.close();
    editor->m_curdsp = PreviewTextEdit::PTE_DSPTXT;

    ////////////////////////////////////////////////////////////////////////
    // Finishing steps

    // Maybe the text was actually empty ? Switch to fields then. Else free-up 
    // the text memory in the loaded document. We still have a copy of the text
    // in editor->m_richtxt
    bool textempty = fdoc.text.empty();
    if (!textempty)
        fdoc.text.clear(); 
    editor->m_fdoc = fdoc;
    editor->m_dbdoc = idoc;
    if (textempty)
        editor->displayFields();

    // If this is an image, display it instead of the text.
    if (!idoc.mimetype.compare(0, 6, "image/")) {
	string fn = fileurltolocalpath(idoc.url);

	// If the command wants a file but this is not a file url, or
	// there is an ipath that it won't understand, we need a temp file:
	theconfig->setKeyDir(path_getfather(fn));
	if (fn.empty() || !idoc.ipath.empty()) {
	    TempFile temp = lthr.imgtmp;
	    if (temp.isNotNull()) {
		LOGDEB1(("Preview: load: got temp file from internfile\n"));
	    } else if (!FileInterner::idocToFile(temp, string(), 
						 theconfig, idoc)) {
		temp.release(); // just in case.
	    }
	    if (temp.isNotNull()) {
		rememberTempFile(temp);
		fn = temp->filename();
		editor->m_tmpfilename = fn;
	    } else {
		editor->m_tmpfilename.erase();
		fn.erase();
	    }
	}

	if (!fn.empty()) {
	    editor->m_image = QImage(fn.c_str());
	    editor->displayImage();
	}
     }


    // Position the editor so that the first search term is visible
    m_haveAnchors = editor->m_plaintorich->lastanchor != 0;
    if (searchTextLine->text().length() != 0) {
	// If there is a current search string, perform the search
	m_canBeep = true;
	doSearch(searchTextLine->text(), true, false);
    } else {
	// Position to the first query term
	if (m_haveAnchors) {
	    QString aname = 
		QString::fromUtf8(editor->m_plaintorich->termAnchorName(1).c_str());
	    LOGDEB2(("Call movetoanchor(%s)\n", (const char *)aname.toUtf8()));
	    editor->scrollToAnchor(aname);
	    // Position the cursor approximately at the anchor (top of
	    // viewport) so that searches start from here
	    QTextCursor cursor = editor->cursorForPosition(QPoint(0, 0));
	    editor->setTextCursor(cursor);
	    m_curAnchor = 1;
	}
    }


    // Enter document in document history
    map<string,string>::const_iterator udit = idoc.meta.find(Rcl::Doc::keyudi);
    if (udit != idoc.meta.end())
        historyEnterDoc(g_dynconf, udit->second);

    editor->setFocus();
    emit(previewExposed(this, m_searchId, docnum));
    LOGDEB(("LoadFileInCurrentTab: returning true\n"));
    return true;
}

PreviewTextEdit::PreviewTextEdit(QWidget* parent,const char* name, Preview *pv) 
    : QTextEdit(parent), m_preview(pv), m_dspflds(false), m_docnum(-1) 
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setObjectName(name);
    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
	    this, SLOT(createPopupMenu(const QPoint&)));
    m_plaintorich = new PlainToRichQtPreview();
}

PreviewTextEdit::~PreviewTextEdit()
{
    delete m_plaintorich;
}

void PreviewTextEdit::createPopupMenu(const QPoint& pos)
{
    LOGDEB1(("PreviewTextEdit::createPopupMenu()\n"));
    QMenu *popup = new QMenu(this);
    switch (m_curdsp) {
    case PTE_DSPTXT:
	popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
	if (!m_image.isNull())
	    popup->addAction(tr("Show image"), this, SLOT(displayImage()));
	break;
    case PTE_DSPFLDS:
	popup->addAction(tr("Show main text"), this, SLOT(displayText()));
	if (!m_image.isNull())
	    popup->addAction(tr("Show image"), this, SLOT(displayImage()));
	break;
    case PTE_DSPIMG:
    default:
	popup->addAction(tr("Show fields"), this, SLOT(displayFields()));
	popup->addAction(tr("Show main text"), this, SLOT(displayText()));
	break;
    }
    popup->addAction(tr("Select All"), this, SLOT(selectAll()));
    popup->addAction(tr("Copy"), this, SLOT(copy()));
    popup->addAction(tr("Print"), this, SLOT(print()));
    // Need to check ipath until we fix the internfile bug that always
    // has it convert to html for top level docs
    if (!m_dbdoc.url.empty() && !m_dbdoc.ipath.empty())
	popup->addAction(tr("Save document to file"), 
			 m_preview, SLOT(emitSaveDocToFile()));
    popup->popup(mapToGlobal(pos));
}

// Display main text
void PreviewTextEdit::displayText()
{
    LOGDEB1(("PreviewTextEdit::displayText()\n"));
    if (m_format == Qt::PlainText)
	setPlainText(m_richtxt);
    else
	setHtml(m_richtxt);
    m_curdsp = PTE_DSPTXT;
}

// Display field values
void PreviewTextEdit::displayFields()
{
    LOGDEB1(("PreviewTextEdit::displayFields()\n"));

    QString txt = "<html><head></head><body>\n";
    txt += "<b>" + QString::fromLocal8Bit(m_url.c_str());
    if (!m_ipath.empty())
	txt += "|" + QString::fromUtf8(m_ipath.c_str());
    txt += "</b><br><br>";
    txt += "<dl>\n";
    for (map<string,string>::const_iterator it = m_fdoc.meta.begin();
	 it != m_fdoc.meta.end(); it++) {
	if (!it->second.empty())
	    txt += "<dt>" + QString::fromUtf8(it->first.c_str()) + "</dt> " 
		+ "<dd>" + QString::fromUtf8(escapeHtml(it->second).c_str()) 
		+ "</dd>\n";
    }
    txt += "</dl></body></html>";
    setHtml(txt);
    m_curdsp = PTE_DSPFLDS;
}

void PreviewTextEdit::displayImage()
{
    LOGDEB1(("PreviewTextEdit::displayImage()\n"));
    if (m_image.isNull())
	displayText();

    setPlainText("");
    if (m_image.width() > width() || 
	m_image.height() > height()) {
	m_image = m_image.scaled(width(), height(), Qt::KeepAspectRatio);
    }
    document()->addResource(QTextDocument::ImageResource, QUrl("image"), 
			    m_image);
    QTextCursor cursor = textCursor();
    cursor.insertImage("image");
    m_curdsp = PTE_DSPIMG;
}

void PreviewTextEdit::mouseDoubleClickEvent(QMouseEvent *event)
{
    LOGDEB2(("PreviewTextEdit::mouseDoubleClickEvent\n"));
    QTextEdit::mouseDoubleClickEvent(event);
    if (textCursor().hasSelection() && m_preview)
	m_preview->emitWordSelect(textCursor().selectedText());
}

void PreviewTextEdit::print()
{
    LOGDEB(("PreviewTextEdit::print\n"));
    if (!m_preview)
        return;
	
#ifndef QT_NO_PRINTER
    QPrinter printer;
    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Current Preview"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    QTextEdit::print(&printer);
#endif
}
