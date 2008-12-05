#ifndef lint
static char rcsid[] = "@(#$Id: ssearch_w.cpp,v 1.25.2.1 2008-12-05 11:17:51 dockes Exp $ (C) 2006 J.F.Dockes";
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

void SSearch::init()
{
    // See enum above and keep in order !
    searchTypCMB->insertItem(tr("Any term"));
    searchTypCMB->insertItem(tr("All terms"));
    searchTypCMB->insertItem(tr("File name"));
    searchTypCMB->insertItem(tr("Query language"));
    
    queryText->insertStringList(prefs.ssearchHistory);
    queryText->setEditText("");
    connect(queryText->lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(startSimpleSearch()));
    connect(queryText->lineEdit(), SIGNAL(textChanged(const QString&)),
	    this, SLOT(searchTextChanged(const QString&)));
    connect(clearqPB, SIGNAL(clicked()), 
	    queryText->lineEdit(), SLOT(clear()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(startSimpleSearch()));

#if QT_VERSION >= 0x040000
    queryText->installEventFilter(this);
#else
    queryText->lineEdit()->installEventFilter(this);
#endif
    m_escape = false;
}

void SSearch::searchTextChanged( const QString & text )
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

void SSearch::startSimpleSearch()
{
    if (queryText->currentText().length() == 0)
	return;

    string u8 = (const char *)queryText->currentText().utf8();
    LOGDEB(("SSearch::startSimpleSearch: [%s]\n", u8.c_str()));

    trimstring(u8);
    if (u8.length() == 0)
	return;

    SSearchType tp = (SSearchType)searchTypCMB->currentItem();
    Rcl::SearchData *sdata = 0;

    if (tp == SST_LANG) {
	string reason;
	sdata = wasaStringToRcl(u8, reason);
	if (sdata == 0) {
	    QMessageBox::warning(0, "Recoll", tr("Bad query string") +
				 QString::fromAscii(reason.c_str()));
	    return;
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

    // Need to remove any previous occurence of the search entry from
    // the listbox list, The qt listbox doesn't do lru correctly (if
    // already in the list the new entry would remain at it's place,
    // not jump at the top as it should
    LOGDEB3(("Querytext list count %d\n", queryText->count()));
    // Have to save current text, this will change while we clean up the list
    QString txt = queryText->currentText();
    bool changed;
    do {
	changed = false;
	for (int index = 0; index < queryText->count(); index++) {
	    LOGDEB3(("Querytext[%d] = [%s]\n", index,
		    (const char *)(queryText->text(index).utf8())));
	    if (queryText->text(index).length() == 0 || 
		QString::compare(queryText->text(index), txt) == 0) {
		LOGDEB3(("Querytext removing at %d [%s] [%s]\n", index,
			(const char *)(queryText->text(index).utf8()),
			(const char *)(txt.utf8())));
		queryText->removeItem(index);
		changed = true;
		break;
	    }
	}
    } while (changed);
    // The combobox is set for no insertion, insert here:
    queryText->insertItem(txt, 0);
    queryText->setCurrentItem(0);

    // Save the current state of the listbox list to the prefs (will
    // go to disk)
    prefs.ssearchHistory.clear();
    for (int index = 0; index < queryText->count(); index++) {
	prefs.ssearchHistory.push_back(queryText->text(index));
    }


    RefCntr<Rcl::SearchData> rsdata(sdata);
    emit startSearch(rsdata);
}
void SSearch::setSearchString(const QString& txt)
{
    queryText->setEditText(txt);
}

void SSearch::setAnyTermMode()
{
    searchTypCMB->setCurrentItem(SST_ANY);
}

// Complete last word in input by querying db for all possible terms.
void SSearch::completion()
{
    if (!rcldb)
	return;
    if (searchTypCMB->currentItem() == SST_FNM) {
	// Filename: no completion
	QApplication::beep();
	return;
    }
    // Extract last word in text
    string txt = (const char *)queryText->currentText().utf8();
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
    list<Rcl::TermMatchEntry> strs;
    string stemLang = (const char *)prefs.queryStemLang.ascii();
    if (stemLang == "ALL") {
	rclconfig->getConfParam("indexstemminglanguages", stemLang);
    }
    if (!rcldb->termMatch(Rcl::Db::ET_WILD, stemLang, s, strs, max) || 
	strs.size() == 0) {
	QApplication::beep();
	return;
    }
    if (strs.size() == (unsigned int)max) {
	QMessageBox::warning(0, "Recoll", tr("Too many completions"));
	return;
    }

    // If list from db is single word, insert it, else ask user to select
    QString res;
    bool ok = false;
    if (strs.size() == 1) {
	res = QString::fromUtf8(strs.begin()->term.c_str());
	ok = true;
    } else {
	QStringList lst;
	for (list<Rcl::TermMatchEntry>::iterator it=strs.begin(); 
	     it != strs.end(); it++) {
	    lst.push_back(QString::fromUtf8(it->term.c_str()));
	}
	res = QInputDialog::getItem(tr("Completions"),
				    tr("Select an item:"), lst, 0, 
				    FALSE, &ok, this);
    }

    // Insert result
    if (ok) {
	txt.erase(cs);
	txt.append(res.utf8());
	queryText->setEditText(QString::fromUtf8(txt.c_str()));
    } else {
	return;
    }
}

#undef SHOWEVENTS
#if defined(SHOWEVENTS)
#if QT_VERSION < 0x040000
static const char *eventTypeToStr(int tp)
{
    switch (tp) {
    case 0: return "None";
    case 1: return "Timer";
    case 2: return "MouseButtonPress";
    case 3: return "MouseButtonRelease";
    case 4: return "MouseButtonDblClick";
    case 5: return "MouseMove";
    case 6: return "KeyPress";
    case 7: return "KeyRelease";
    case 8: return "FocusIn";
    case 9: return "FocusOut";
    case 10: return "Enter";
    case 11: return "Leave";
    case 12: return "Paint";
    case 13: return "Move";
    case 14: return "Resize";
    case 15: return "Create";
    case 16: return "Destroy";
    case 17: return "Show";
    case 18: return "Hide";
    case 19: return "Close";
    case 20: return "Quit";
    case 21: return "Reparent";
    case 22: return "ShowMinimized";
    case 23: return "ShowNormal";
    case 24: return "WindowActivate";
    case 25: return "WindowDeactivate";
    case 26: return "ShowToParent";
    case 27: return "HideToParent";
    case 28: return "ShowMaximized";
    case 29: return "ShowFullScreen";
    case 30: return "Accel";
    case 31: return "Wheel";
    case 32: return "AccelAvailable";
    case 33: return "CaptionChange";
    case 34: return "IconChange";
    case 35: return "ParentFontChange";
    case 36: return "ApplicationFontChange";
    case 37: return "ParentPaletteChange";
    case 38: return "ApplicationPaletteChange";
    case 39: return "PaletteChange";
    case 40: return "Clipboard";
    case 42: return "Speech";
    case 50: return "SockAct";
    case 51: return "AccelOverride";
    case 52: return "DeferredDelete";
    case 60: return "DragEnter";
    case 61: return "DragMove";
    case 62: return "DragLeave";
    case 63: return "Drop";
    case 64: return "DragResponse";
    case 70: return "ChildInserted";
    case 71: return "ChildRemoved";
    case 72: return "LayoutHint";
    case 73: return "ShowWindowRequest";
    case 74: return "WindowBlocked";
    case 75: return "WindowUnblocked";
    case 80: return "ActivateControl";
    case 81: return "DeactivateControl";
    case 82: return "ContextMenu";
    case 83: return "IMStart";
    case 84: return "IMCompose";
    case 85: return "IMEnd";
    case 86: return "Accessibility";
    case 87: return "TabletMove";
    case 88: return "LocaleChange";
    case 89: return "LanguageChange";
    case 90: return "LayoutDirectionChange";
    case 91: return "Style";
    case 92: return "TabletPress";
    case 93: return "TabletRelease";
    case 94: return "OkRequest";
    case 95: return "HelpRequest";
    case 96: return "WindowStateChange";
    case 97: return "IconDrag";
    case 1000: return "User";
    case 65535: return "MaxUser";
    default: return "Unknown";
    }
}
#else
static const char *eventTypeToStr(int tp)
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
