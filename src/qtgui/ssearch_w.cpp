/* Copyright (C) 2006 J.F.Dockes
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
#include <qapplication.h>
#include <qinputdialog.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qmessagebox.h>
#include <qevent.h>

#include "debuglog.h"
#include "guiutils.h"
#include "searchdata.h"
#include "ssearch_w.h"
#include "refcntr.h"
#include "textsplit.h"
#include "wasatorcl.h"
#include "rclhelp.h"

void SSearch::init()
{
    // See enum above and keep in order !
    searchTypCMB->addItem(tr("Any term"));
    searchTypCMB->addItem(tr("All terms"));
    searchTypCMB->addItem(tr("File name"));
    searchTypCMB->addItem(tr("Query language"));
    
    // We'd like to use QComboBox::InsertAtTop but it doesn't do lru
    // (existing item stays at its place instead of jumping at top)
    queryText->setInsertPolicy(QComboBox::NoInsert);

    queryText->addItems(prefs.ssearchHistory);
    queryText->setEditText("");
    connect(queryText->lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(startSimpleSearch()));
    connect(queryText->lineEdit(), SIGNAL(textChanged(const QString&)),
	    this, SLOT(searchTextChanged(const QString&)));
    connect(clearqPB, SIGNAL(clicked()), 
	    queryText->lineEdit(), SLOT(clear()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(startSimpleSearch()));
    connect(searchTypCMB, SIGNAL(activated(int)), this, SLOT(searchTypeChanged(int)));

    queryText->installEventFilter(this);
    m_escape = false;
}

void SSearch::searchTextChanged(const QString& text)
{
    if (text.isEmpty()) {
	searchPB->setEnabled(false);
	clearqPB->setEnabled(false);
	emit clearSearch();
    } else {
	searchPB->setEnabled(true);
	clearqPB->setEnabled(true);
    }
}

void SSearch::searchTypeChanged(int typ)
{
    LOGDEB(("Search type now %d\n", typ));
    // Adjust context help
    if (typ == SST_LANG)
	HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			       "RCL.SEARCH.LANG");
    else 
	HelpClient::installMap((const char *)this->objectName().toUtf8(), 
			       "RCL.SEARCH.SIMPLE");

    // Also fix tooltips
    switch (typ) {
    case SST_LANG:
        queryText->setToolTip(tr(
"Enter query language expression. Cheat sheet:<br>\n"
"<i>term1 term2</i> : 'term1' and 'term2' in any field.<br>\n"
"<i>field:term1</i> : 'term1' in field 'field'.<br>\n"
" Standard field names/synonyms:<br>\n"
"  title/subject/caption, author/from, recipient/to, filename, ext.<br>\n"
" Pseudo-fields: dir, mime/format, type/rclcat, date.<br>\n"
" Two date interval exemples: 2009-03-01/2009-05-20  2009-03-01/P2M.<br>\n"
"<i>term1 term2 OR term3</i> : term1 AND (term2 OR term3).<br>\n"
"  No actual parentheses allowed.<br>\n"
"<i>\"term1 term2\"</i> : phrase (must occur exactly). Possible modifiers:<br>\n"
"<i>\"term1 term2\"p</i> : unordered proximity search with default distance.<br>\n"
"Use <b>Show Query</b> link when in doubt about result and see manual (&lt;F1>) for more detail.\n"
				  ));
        break;
    case SST_FNM:
        queryText->setToolTip(tr("Enter file name wildcard expression."));
        break;
    case SST_ANY:
    case SST_ALL:
    default:
        queryText->setToolTip(tr(
      "Enter search terms here. Type ESC SPC for completions of current term."
				  ));
    }
}

void SSearch::startSimpleSearch()
{
    if (queryText->currentText().length() == 0)
	return;

    string u8 = (const char *)queryText->currentText().toUtf8();
    LOGDEB(("SSearch::startSimpleSearch: [%s]\n", u8.c_str()));

    trimstring(u8);
    if (u8.length() == 0)
	return;

    SSearchType tp = (SSearchType)searchTypCMB->currentIndex();
    Rcl::SearchData *sdata = 0;

    if (tp == SST_LANG) {
	string reason;
        if (prefs.autoSuffsEnable)
            sdata = wasaStringToRcl(theconfig, u8, reason, (const char *)prefs.autoSuffs.toUtf8());
        else
            sdata = wasaStringToRcl(theconfig, u8, reason);
	if (sdata == 0) {
	    QMessageBox::warning(0, "Recoll", tr("Bad query string") + ": " +
				 QString::fromAscii(reason.c_str()));
	    return;
	}
	if (prefs.ssearchAutoPhrase) {
	    sdata->maybeAddAutoPhrase();
	}
    } else {
	sdata = new Rcl::SearchData(Rcl::SCLT_OR);
	if (sdata == 0) {
	    QMessageBox::warning(0, "Recoll", tr("Out of memory"));
	    return;
	}

	// If there is no white space inside the query, then the user
	// certainly means it as a phrase.
	bool isreallyaphrase = false;
	if (!TextSplit::hasVisibleWhite(u8))
	    isreallyaphrase = true;

	// Maybe add automatic phrase ? For ALL and ANY, and not if
	// there is already a phrase or wildcard terms.
	if (!isreallyaphrase && 
	    prefs.ssearchAutoPhrase && (tp == SST_ANY || tp == SST_ALL) &&
	    u8.find_first_of("\"*[]?") == string::npos && 
	    TextSplit::countWords(u8) > 1) {
	    sdata->addClause(new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, 
							   u8, 0));
	}
	Rcl::SearchDataClause *clp = 0;
	switch (tp) {
	case SST_ANY:
	default:
	    clp = isreallyaphrase ? 
		new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, u8, 0) :
		new Rcl::SearchDataClauseSimple(Rcl::SCLT_OR, u8);
	    break;
	case SST_ALL:
	    clp = isreallyaphrase ? 
		new Rcl::SearchDataClauseDist(Rcl::SCLT_PHRASE, u8, 0) :
		new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, u8);
	    break;
	case SST_FNM:
	    clp = new Rcl::SearchDataClauseFilename(u8);
	    break;
	}
	sdata->addClause(clp);
    }

    // Search terms history

    // We want to have the new text at the top and any older identical
    // entry to be erased. There is no standard qt policy to do this ? 
    // So do it by hand.
    QString txt = queryText->currentText();
    int index = queryText->findText(txt);
    if (index >= 0) 
	queryText->removeItem(index);
    queryText->insertItem(0, txt);
    queryText->setCurrentIndex(0);

    // Save the current state of the listbox list to the prefs (will
    // go to disk)
    prefs.ssearchHistory.clear();
    for (int index = 0; index < queryText->count(); index++) {
	prefs.ssearchHistory.push_back(queryText->itemText(index));
    }

    RefCntr<Rcl::SearchData> rsdata(sdata);
    emit startSearch(rsdata);
}

void SSearch::setSearchString(const QString& txt)
{
    queryText->setEditText(txt);
}

bool SSearch::hasSearchString()
{
    return !queryText->lineEdit()->text().isEmpty();
}

// Add term to simple search. Term comes out of double-click in
// reslist or preview. 
// It would probably be better to cleanup in preview.ui.h and
// reslist.cpp and do the proper html stuff in the latter case
// (which is different because it format is explicit richtext
// instead of auto as for preview, needed because it's built by
// fragments?).
static const char* punct = " \t()<>\"'[]{}!^*.,:;\n\r";
void SSearch::addTerm(QString term)
{
    LOGDEB(("SSearch::AddTerm: [%s]\n", (const char *)term.toUtf8()));
    string t = (const char *)term.toUtf8();
    string::size_type pos = t.find_last_not_of(punct);
    if (pos == string::npos)
	return;
    t = t.substr(0, pos+1);
    pos = t.find_first_not_of(punct);
    if (pos != string::npos)
	t = t.substr(pos);
    if (t.empty())
	return;
    term = QString::fromUtf8(t.c_str());

    QString text = queryText->currentText();
    text += QString::fromLatin1(" ") + term;
    queryText->setEditText(text);
}

void SSearch::setAnyTermMode()
{
    searchTypCMB->setCurrentIndex(SST_ANY);
}

// Complete last word in input by querying db for all possible terms.
void SSearch::completion()
{
    if (!rcldb)
	return;
    if (searchTypCMB->currentIndex() == SST_FNM) {
	// Filename: no completion
	QApplication::beep();
	return;
    }
    // Extract last word in text
    string txt = (const char *)queryText->currentText().toUtf8();
    string::size_type cs = txt.find_last_of(" ");
    if (cs == string::npos)
	cs = 0;
    else
	cs++;
    if (txt.size() == 0 || cs == txt.size()) {
	QApplication::beep();
	return;
    }
    string s = txt.substr(cs) + "*";
    LOGDEB(("Completing: [%s]\n", s.c_str()));

    // Query database
    const int max = 100;
    Rcl::TermMatchResult tmres;
    string stemLang = (const char *)prefs.queryStemLang.toAscii();
    if (stemLang == "ALL") {
	theconfig->getConfParam("indexstemminglanguages", stemLang);
    }
    if (!rcldb->termMatch(Rcl::Db::ET_WILD, stemLang, s, tmres, max) || 
	tmres.entries.size() == 0) {
	QApplication::beep();
	return;
    }
    if (tmres.entries.size() == (unsigned int)max) {
	QMessageBox::warning(0, "Recoll", tr("Too many completions"));
	return;
    }

    // If list from db is single word, insert it, else ask user to select
    QString res;
    bool ok = false;
    if (tmres.entries.size() == 1) {
	res = QString::fromUtf8(tmres.entries.begin()->term.c_str());
	ok = true;
    } else {
	QStringList lst;
	for (list<Rcl::TermMatchEntry>::iterator it = tmres.entries.begin(); 
	     it != tmres.entries.end(); it++) {
	    lst.push_back(QString::fromUtf8(it->term.c_str()));
	}
	res = QInputDialog::getItem (this, tr("Completions"), 
				     tr("Select an item:"),
				     lst, 0, false, &ok);
    }

    // Insert result
    if (ok) {
	txt.erase(cs);
	txt.append((const char *)res.toUtf8());
	queryText->setEditText(QString::fromUtf8(txt.c_str()));
    } else {
	return;
    }
}

#undef SHOWEVENTS
#if defined(SHOWEVENTS)
const char *eventTypeToStr(int tp)
{
    switch (tp) {
    case  0: return "None";
    case  1: return "Timer";
    case  2: return "MouseButtonPress";
    case  3: return "MouseButtonRelease";
    case  4: return "MouseButtonDblClick";
    case  5: return "MouseMove";
    case  6: return "KeyPress";
    case  7: return "KeyRelease";
    case  8: return "FocusIn";
    case  9: return "FocusOut";
    case  10: return "Enter";
    case  11: return "Leave";
    case  12: return "Paint";
    case  13: return "Move";
    case  14: return "Resize";
    case  15: return "Create";
    case  16: return "Destroy";
    case  17: return "Show";
    case  18: return "Hide";
    case  19: return "Close";
    case  20: return "Quit";
    case  21: return "ParentChange";
    case  131: return "ParentAboutToChange";
    case  22: return "ThreadChange";
    case  24: return "WindowActivate";
    case  25: return "WindowDeactivate";
    case  26: return "ShowToParent";
    case  27: return "HideToParent";
    case  31: return "Wheel";
    case  33: return "WindowTitleChange";
    case  34: return "WindowIconChange";
    case  35: return "ApplicationWindowIconChange";
    case  36: return "ApplicationFontChange";
    case  37: return "ApplicationLayoutDirectionChange";
    case  38: return "ApplicationPaletteChange";
    case  39: return "PaletteChange";
    case  40: return "Clipboard";
    case  42: return "Speech";
    case   43: return "MetaCall";
    case  50: return "SockAct";
    case  132: return "WinEventAct";
    case  52: return "DeferredDelete";
    case  60: return "DragEnter";
    case  61: return "DragMove";
    case  62: return "DragLeave";
    case  63: return "Drop";
    case  64: return "DragResponse";
    case  68: return "ChildAdded";
    case  69: return "ChildPolished";
    case  70: return "ChildInserted";
    case  72: return "LayoutHint";
    case  71: return "ChildRemoved";
    case  73: return "ShowWindowRequest";
    case  74: return "PolishRequest";
    case  75: return "Polish";
    case  76: return "LayoutRequest";
    case  77: return "UpdateRequest";
    case  78: return "UpdateLater";
    case  79: return "EmbeddingControl";
    case  80: return "ActivateControl";
    case  81: return "DeactivateControl";
    case  82: return "ContextMenu";
    case  83: return "InputMethod";
    case  86: return "AccessibilityPrepare";
    case  87: return "TabletMove";
    case  88: return "LocaleChange";
    case  89: return "LanguageChange";
    case  90: return "LayoutDirectionChange";
    case  91: return "Style";
    case  92: return "TabletPress";
    case  93: return "TabletRelease";
    case  94: return "OkRequest";
    case  95: return "HelpRequest";
    case  96: return "IconDrag";
    case  97: return "FontChange";
    case  98: return "EnabledChange";
    case  99: return "ActivationChange";
    case  100: return "StyleChange";
    case  101: return "IconTextChange";
    case  102: return "ModifiedChange";
    case  109: return "MouseTrackingChange";
    case  103: return "WindowBlocked";
    case  104: return "WindowUnblocked";
    case  105: return "WindowStateChange";
    case  110: return "ToolTip";
    case  111: return "WhatsThis";
    case  112: return "StatusTip";
    case  113: return "ActionChanged";
    case  114: return "ActionAdded";
    case  115: return "ActionRemoved";
    case  116: return "FileOpen";
    case  117: return "Shortcut";
    case  51: return "ShortcutOverride";
    case  30: return "Accel";
    case  32: return "AccelAvailable";
    case  118: return "WhatsThisClicked";
    case  120: return "ToolBarChange";
    case  121: return "ApplicationActivated";
    case  122: return "ApplicationDeactivated";
    case  123: return "QueryWhatsThis";
    case  124: return "EnterWhatsThisMode";
    case  125: return "LeaveWhatsThisMode";
    case  126: return "ZOrderChange";
    case  127: return "HoverEnter";
    case  128: return "HoverLeave";
    case  129: return "HoverMove";
    case  119: return "AccessibilityHelp";
    case  130: return "AccessibilityDescription";
    case  150: return "EnterEditFocus";
    case  151: return "LeaveEditFocus";
    case  152: return "AcceptDropsChange";
    case  153: return "MenubarUpdated";
    case  154: return "ZeroTimerEvent";
    case  155: return "GraphicsSceneMouseMove";
    case  156: return "GraphicsSceneMousePress";
    case  157: return "GraphicsSceneMouseRelease";
    case  158: return "GraphicsSceneMouseDoubleClick";
    case  159: return "GraphicsSceneContextMenu";
    case  160: return "GraphicsSceneHoverEnter";
    case  161: return "GraphicsSceneHoverMove";
    case  162: return "GraphicsSceneHoverLeave";
    case  163: return "GraphicsSceneHelp";
    case  164: return "GraphicsSceneDragEnter";
    case  165: return "GraphicsSceneDragMove";
    case  166: return "GraphicsSceneDragLeave";
    case  167: return "GraphicsSceneDrop";
    case  168: return "GraphicsSceneWheel";
    case  169: return "KeyboardLayoutChange";
    case  170: return "DynamicPropertyChange";
    case  171: return "TabletEnterProximity";
    case  172: return "TabletLeaveProximity";
    default: return "UnknownEvent";
    }
}
#endif

bool SSearch::eventFilter(QObject *, QEvent *event)
{
#if defined(SHOWEVENTS)
    if (event->type() == QEvent::Timer || 
	event->type() == QEvent::UpdateRequest ||
	event->type() == QEvent::Paint)
	return false;
    LOGDEB2(("SSearch::eventFilter: target %p (%p) type %s\n", 
	    target, queryText->lineEdit(),
	    eventTypeToStr(event->type())));
#endif

    if (event->type() == QEvent::KeyPress)  {
	QKeyEvent *ke = (QKeyEvent *)event;
	LOGDEB1(("SSearch::eventFilter: keyPress (m_escape %d) key %d\n", 
		 m_escape, ke->key()));
	if (ke->key() == Qt::Key_Escape) {
	    LOGDEB(("Escape\n"));
	    m_escape = true;
	    return true;
	} else if (m_escape && ke->key() == Qt::Key_Space) {
	    LOGDEB(("Escape space\n"));
	    ke->accept();
	    completion();
	    m_escape = false;
	    return true;
	} else if (ke->key() == Qt::Key_Space) {
	    if (prefs.autoSearchOnWS)
		startSimpleSearch();
	}
	m_escape = false;
    }
    return false;
}
