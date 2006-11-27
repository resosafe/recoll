#ifndef lint
static char rcsid[] = "@(#$Id: ssearch_w.cpp,v 1.11.2.1 2006-11-27 09:18:00 dockes Exp $ (C) 2006 J.F.Dockes";
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

#include "debuglog.h"
#include "guiutils.h"
#include "searchdata.h"
#include "ssearch_w.h"
#include "refcntr.h"

void SSearch::init()
{
    searchTypCMB->insertItem(tr("Any term"));
    searchTypCMB->insertItem(tr("All terms"));
    searchTypCMB->insertItem(tr("File name"));
    queryText->insertStringList(prefs.ssearchHistory);
    queryText->setEditText("");
    connect(queryText->lineEdit(), SIGNAL(returnPressed()),
	    this, SLOT(startSimpleSearch()));
    connect(queryText->lineEdit(), SIGNAL(textChanged(const QString&)),
	    this, SLOT(searchTextChanged(const QString&)));
    connect(clearqPB, SIGNAL(clicked()), 
	    queryText->lineEdit(), SLOT(clear()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(startSimpleSearch()));

    queryText->lineEdit()->installEventFilter(this);
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
	string u8 =  (const char *)queryText->currentText().utf8();
	if (prefs.autoSearchOnWS && !u8.empty() && u8[u8.length()-1] == ' ')
	    startSimpleSearch();
    }
}

void SSearch::startSimpleSearch()
{
    if (queryText->currentText().length() == 0)
	return;
    RefCntr<Rcl::SearchData> sdata(new Rcl::SearchData(Rcl::SCLT_AND));
    string u8 = (const char *)queryText->currentText().utf8();

    LOGDEB(("SSearch::startSimpleSearch: [%s]\n", u8.c_str()));

    switch (searchTypCMB->currentItem()) {
    case 0:
    default: 
	// If this is an or and we're set for autophrase and there are
	// no quotes in the query, add a phrase search
	if (prefs.ssearchAutoPhrase && 
	    u8.find_first_of("\"") == string::npos) {
	    u8 += string(" \"") + u8 + "\"";
	}
	sdata->addClause(new Rcl::SearchDataClauseSimple(Rcl::SCLT_OR, u8));
	break;
    case 1:
	sdata->addClause(new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, u8));
	break;
    case 2:
	sdata->addClause(new Rcl::SearchDataClauseFilename(u8));
	break;
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
    emit startSearch(sdata);
}

void SSearch::setAnyTermMode()
{
    searchTypCMB->setCurrentItem(0);
}

// Complete last word in input by querying db for all possible terms.
void SSearch::completion()
{
    if (!rcldb)
	return;
    if (searchTypCMB->currentItem() == 2) {
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
    list<string> strs;
    
    if (!rcldb->termMatch(Rcl::Db::ET_WILD, s, strs, 
			    prefs.queryStemLang.ascii(),max)
	|| strs.size() == 0) {
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
	res = QString::fromUtf8(strs.begin()->c_str());
	ok = true;
    } else {
	QStringList lst;
	for (list<string>::iterator it=strs.begin(); it != strs.end(); it++) 
	    lst.push_back(QString::fromUtf8(it->c_str()));
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


bool SSearch::eventFilter(QObject *target, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)  {
	QKeyEvent *ke = (QKeyEvent *)event;
	LOGDEB1(("SSearch::eventFilter: keyPress escape %d key %d\n", 
		 m_escape, ke->key()));
	if (ke->key() == Key_Escape) {
	    LOGDEB(("Escape\n"));
	    m_escape = true;
	    return true;
	} else if (m_escape && ke->key() == Key_Space) {
	    LOGDEB(("Escape space\n"));
	    ke->accept();
	    completion();
	    m_escape = false;
	    return true;
	} else {
	    m_escape = false;
	}
    }
    return QWidget::eventFilter(target, event);
}


