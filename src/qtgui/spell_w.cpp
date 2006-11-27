#ifndef lint
static char rcsid[] = "@(#$Id: spell_w.cpp,v 1.6.2.1 2006-11-27 10:00:53 dockes Exp $ (C) 2005 J.F.Dockes";
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

#include <qmessagebox.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qcombobox.h>

#include "debuglog.h"
#include "recoll.h"
#include "spell_w.h"
#include "guiutils.h"

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
    stemLangCMB->setEnabled(false);

    // signals and slots connections
    connect(baseWordLE, SIGNAL(textChanged(const QString&)), 
	    this, SLOT(wordChanged(const QString&)));
    connect(baseWordLE, SIGNAL(returnPressed()), this, SLOT(doExpand()));
    connect(expandPB, SIGNAL(clicked()), this, SLOT(doExpand()));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(suggsTE, SIGNAL(doubleClicked(int, int)), 
	    this, SLOT(textDoubleClicked(int, int)));
    connect(expTypeCMB, SIGNAL(activated(int)), 
	    this, SLOT(modeSet(int)));
}

/* Expand term according to current mode */
void SpellW::doExpand()
{
    suggsTE->clear();
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
    switch (expTypeCMB->currentItem()) {
    case 1: mt = Rcl::Db::ET_REGEXP;
	/* FALLTHROUGH */
    case 0: 
	if (!rcldb->termMatch(mt, expr, suggs, prefs.queryStemLang.ascii(),
			      200)) {
	    LOGERR(("SpellW::doExpand:rcldb::termMatch failed\n"));
	    return;
	}
	break;


    case 2: 
	{
	    string stemlang = (const char *)stemLangCMB->currentText().utf8();
	    suggs = rcldb->stemExpand(stemlang,expr);
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
	if (!aspell->suggest(*rcldb, expr, suggs, reason)) {
	    QMessageBox::warning(0, "Recoll",
				 tr("Aspell expansion error. "));
	    LOGERR(("SpellW::doExpand:suggest failed: %s\n", reason.c_str()));
	}
    }
#endif
    }

    if (suggs.empty()) {
	suggsTE->append(tr("No expansion found"));
    } else {
	for (list<string>::iterator it = suggs.begin(); 
	     it != suggs.end(); it++) {
	    suggsTE->append(QString::fromUtf8(it->c_str()));
	}
	suggsTE->setCursorPosition(0,0);
	suggsTE->ensureCursorVisible();
    }
}

void SpellW::wordChanged(const QString &text)
{
    if (text.isEmpty()) {
	expandPB->setEnabled(false);
	suggsTE->clear();
    } else {
	expandPB->setEnabled(true);
    }
}

void SpellW::textDoubleClicked(int para, int)
{
    suggsTE->setSelection(para, 0, para, 1000);
    if (suggsTE->hasSelectedText())
	emit(wordSelect(suggsTE->selectedText()));
}

void SpellW::modeSet(int mode)
{
    if (mode == 2)
	stemLangCMB->setEnabled(true);
    else
	stemLangCMB->setEnabled(false);
}
