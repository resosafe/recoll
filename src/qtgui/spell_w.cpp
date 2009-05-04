#ifndef lint
static char rcsid[] = "@(#$Id: spell_w.cpp,v 1.11 2007-02-19 16:28:05 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include "autoconfig.h"

#include <unistd.h>

#include <list>
#include <stdio.h>

#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qcombobox.h>
#if (QT_VERSION < 0x040000)
#include <qlistview.h>
#else
#define QListView Q3ListView
#define QListViewItem Q3ListViewItem
#define QListViewItemIterator Q3ListViewItemIterator
#include <q3listview.h>
#endif

#include "debuglog.h"
#include "recoll.h"
#include "spell_w.h"
#include "guiutils.h"
#include "rcldb.h"
#include "rclhelp.h"

#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

void SpellW::init()
{
    // Don't change the order, or fix the rest of the code...
    /*0*/expTypeCMB->insertItem(tr("Wildcards"));
    /*1*/expTypeCMB->insertItem(tr("Regexp"));
    /*2*/expTypeCMB->insertItem(tr("Stem expansion"));
#ifdef RCL_USE_ASPELL
    bool noaspell = false;
    rclconfig->getConfParam("noaspell", &noaspell);
    if (!noaspell)
	/*3*/expTypeCMB->insertItem(tr("Spelling/Phonetic"));
#endif

    int typ = prefs.termMatchType;
    if (typ < 0 || typ > expTypeCMB->count())
	typ = 0;
    expTypeCMB->setCurrentItem(typ);

    // Stemming language combobox
    stemLangCMB->clear();
    list<string> langs;
    if (!getStemLangs(langs)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("error retrieving stemming languages"));
    }
    for (list<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	stemLangCMB->
	    insertItem(QString::fromAscii(it->c_str(), it->length()));
    }
    stemLangCMB->setEnabled(expTypeCMB->currentItem()==2);

    (void)new HelpClient(this);
    HelpClient::installMap(this->name(), "RCL.SEARCH.TERMEXPLORER");

    // signals and slots connections
    connect(baseWordLE, SIGNAL(textChanged(const QString&)), 
	    this, SLOT(wordChanged(const QString&)));
    connect(baseWordLE, SIGNAL(returnPressed()), this, SLOT(doExpand()));
    connect(expandPB, SIGNAL(clicked()), this, SLOT(doExpand()));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));

    connect(suggsLV,
#if (QT_VERSION < 0x040000)
	   SIGNAL(doubleClicked(QListViewItem *, const QPoint &, int)),
#else
	   SIGNAL(doubleClicked(Q3ListViewItem *, const QPoint &, int)),
#endif
	   this, SLOT(textDoubleClicked()));

    connect(expTypeCMB, SIGNAL(activated(int)), 
	    this, SLOT(modeSet(int)));

    suggsLV->setColumnWidth(0, 200);
    suggsLV->setColumnWidth(1, 100);
    // No initial sorting: user can choose to establish one
    suggsLV->setSorting(100, false);
}

// Subclass qlistviewitem for numeric sorting on column 1
class MyListViewItem : public QListViewItem
{
public:
    MyListViewItem(QListView *listView, const QString& s1, const QString& s2)
        : QListViewItem(listView, s1, s2)
    { }

    int compare(QListViewItem * i, int col, bool) const {
	if (col == 0)
	    return i->text(0).compare(text(0));
	if (col == 1)
	    return i->text(1).toInt() - text(1).toInt();
	// ??
	return 0;
    }
};


/* Expand term according to current mode */
void SpellW::doExpand()
{
    suggsLV->clear();
    if (baseWordLE->text().isEmpty()) 
	return;

    string reason;
    if (!maybeOpenDb(reason)) {
	LOGDEB(("SpellW::doExpand: db error: %s\n", reason.c_str()));
	return;
    }

    string expr = string((const char *)baseWordLE->text().utf8());
    list<string> suggs;

    prefs.termMatchType = expTypeCMB->currentItem();

    Rcl::Db::MatchType mt = Rcl::Db::ET_WILD;
    switch(expTypeCMB->currentItem()) {
    case 0: mt = Rcl::Db::ET_WILD; break;
    case 1:mt = Rcl::Db::ET_REGEXP; break;
    case 2:mt = Rcl::Db::ET_STEM; break;
    }

    list<Rcl::TermMatchEntry> entries;
    switch (expTypeCMB->currentItem()) {
    case 0: 
    case 1:
    case 2: {
	string l_stemlang = stemLangCMB->currentText().ascii();

	if (!rcldb->termMatch(mt, l_stemlang, expr, entries, 200)) {
	    LOGERR(("SpellW::doExpand:rcldb::termMatch failed\n"));
	    return;
	}
    }
	break;

#ifdef RCL_USE_ASPELL
    case 3: {
	LOGDEB(("SpellW::doExpand: aspelling\n"));
	if (!aspell) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Aspell init failed. "
				    "Aspell not installed?"));
	    LOGDEB(("SpellW::doExpand: aspell init error\n"));
	    return;
	}
	list<string> suggs;
	if (!aspell->suggest(*rcldb, expr, suggs, reason)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Aspell expansion error. "));
	    LOGERR(("SpellW::doExpand:suggest failed: %s\n", reason.c_str()));
	}
	for (list<string>::const_iterator it = suggs.begin(); 
	     it != suggs.end(); it++) 
	    entries.push_back(Rcl::TermMatchEntry(*it));
    }
#endif
    }


    if (entries.empty()) {
	new MyListViewItem(suggsLV, tr("No expansion found"), "");
    } else {
	// Seems that need to use a reverse iterator to get same order in 
	// listview and input list ??
	for (list<Rcl::TermMatchEntry>::reverse_iterator it = entries.rbegin(); 
	     it != entries.rend(); it++) {
	    LOGDEB(("SpellW::expand: %6d [%s]\n", it->wcf, it->term.c_str()));
	    char num[20];
	    if (it->wcf)
		sprintf(num, "%d", it->wcf);
	    else
		num[0] = 0;
	    new MyListViewItem(suggsLV, 
			      QString::fromUtf8(it->term.c_str()),
			      QString::fromAscii(num));
	}
    }
}

void SpellW::wordChanged(const QString &text)
{
    if (text.isEmpty()) {
	expandPB->setEnabled(false);
	suggsLV->clear();
    } else {
	expandPB->setEnabled(true);
    }
}

void SpellW::textDoubleClicked()
{
    QListViewItemIterator it(suggsLV);
    while (it.current()) {
	QListViewItem *item = it.current();
	if (!item->isSelected()) {
	    ++it;
	    continue;
	}
	emit(wordSelect((const char *)item->text(0)));
	++it;
    }
}

void SpellW::modeSet(int mode)
{
    if (mode == 2)
	stemLangCMB->setEnabled(true);
    else
	stemLangCMB->setEnabled(false);
}
