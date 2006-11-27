#ifndef lint
static char rcsid[] = "@(#$Id: advsearch_w.cpp,v 1.12.2.1 2006-11-27 16:15:41 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include "advsearch_w.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qframe.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlistbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qfiledialog.h>
#include <qmessagebox.h>

#include <list>
#include <string>

#ifndef NO_NAMESPACES
using std::list;
using std::string;
#endif /* NO_NAMESPACES */

#include "recoll.h"
#include "rclconfig.h"
#include "debuglog.h"
#include "searchdata.h"
#include "guiutils.h"

extern RclConfig *rclconfig;

static const int initclausetypes[] = {1, 3, 0, 2, 5};
static const unsigned int iclausescnt = sizeof(initclausetypes) / sizeof(int);

void AdvSearch::init()
{
    // signals and slots connections
    connect(delFiltypPB, SIGNAL(clicked()), this, SLOT(delFiltypPB_clicked()));
    connect(searchPB, SIGNAL(clicked()), this, SLOT(searchPB_clicked()));
    connect(restrictFtCB, SIGNAL(toggled(bool)), 
	    this, SLOT(restrictFtCB_toggled(bool)));
    connect(dismissPB, SIGNAL(clicked()), this, SLOT(close()));
    connect(browsePB, SIGNAL(clicked()), this, SLOT(browsePB_clicked()));
    connect(addFiltypPB, SIGNAL(clicked()), this, SLOT(addFiltypPB_clicked()));

    connect(subtreeCMB->lineEdit(), SIGNAL(returnPressed()), 
	    this, SLOT(searchPB_clicked()));
    connect(delAFiltypPB, SIGNAL(clicked()), 
	    this, SLOT(delAFiltypPB_clicked()));
    connect(addAFiltypPB, SIGNAL(clicked()), 
	    this, SLOT(addAFiltypPB_clicked()));
    connect(saveFileTypesPB, SIGNAL(clicked()), 
	    this, SLOT(saveFileTypes()));
    connect(addClausePB, SIGNAL(clicked()), this, SLOT(addClause()));
    connect(delClausePB, SIGNAL(clicked()), this, SLOT(delClause()));

    conjunctCMB->insertItem(tr("All clauses"));
    conjunctCMB->insertItem(tr("Any clause"));

    // Create preconfigured clauses
    for (unsigned int i = 0; i < iclausescnt; i++) {
	addClause(initclausetypes[i]);
    }
    // Tune initial state according to last saved
    {
	std::list<SearchClauseW *>::iterator cit = m_clauseWins.begin();
	for (vector<int>::iterator it = prefs.advSearchClauses.begin(); 
	     it != prefs.advSearchClauses.end(); it++) {
	    if (cit != m_clauseWins.end()) {
		(*cit)->tpChange(*it);
		cit++;
	    } else {
		addClause(*it);
	    }
	}
    }
    
    // Initialize lists of accepted and ignored mime types from config
    // and settings
    list<string> types = rclconfig->getAllMimeTypes();
    noFiltypsLB->insertStringList(prefs.asearchIgnFilTyps); 

    QStringList ql;
    for (list<string>::iterator it = types.begin(); it != types.end(); it++) {
	if (prefs.asearchIgnFilTyps.findIndex(it->c_str())<0)
	    ql.append(it->c_str());
    }
    yesFiltypsLB->insertStringList(ql);

    subtreeCMB->insertStringList(prefs.asearchSubdirHist);
    subtreeCMB->setEditText("");

    // The clauseline frame is needed to force designer to accept a
    // vbox to englobe the base clauses grid and 'something else' (the
    // vbox is so that we can then insert SearchClauseWs), but we
    // don't want to see it.
    clauseline->close();
}

void AdvSearch::saveCnf()
{
    // Save my state
    prefs.advSearchClauses.clear(); 
    for (std::list<SearchClauseW *>::iterator cit = m_clauseWins.begin();
	 cit != m_clauseWins.end(); cit++) {
	prefs.advSearchClauses.push_back((*cit)->sTpCMB->currentItem());
    }
}

bool AdvSearch::close()
{
    saveCnf();
    return AdvSearchBase::close();
}

// Move selected file types from the searched to the ignored box
void AdvSearch::delFiltypPB_clicked()
{
    list<int> trl;
    QStringList moved;
    for (unsigned int i = 0; i < yesFiltypsLB->count();i++) {
	QListBoxItem *item = yesFiltypsLB->item(i);
	if (item && item->isSelected()) {
	    moved.push_back(item->text());
	    trl.push_front(i);
	}
    }
    if (!moved.empty()) {
	noFiltypsLB->insertStringList(moved);
	for (list<int>::iterator it = trl.begin();it != trl.end(); it++)
	    yesFiltypsLB->removeItem(*it);
    }
    yesFiltypsLB->sort();
    noFiltypsLB->sort();
}

void AdvSearch::delAFiltypPB_clicked()
{
    for (unsigned int i = 0; i < yesFiltypsLB->count();i++) {
	yesFiltypsLB->setSelected(i, true);
    }
    delFiltypPB_clicked();
}

// Save current list of ignored file types to prefs
void AdvSearch::saveFileTypes()
{
    prefs.asearchIgnFilTyps.clear();
    for (unsigned int i = 0; i < noFiltypsLB->count();i++) {
	QListBoxItem *item = noFiltypsLB->item(i);
	prefs.asearchIgnFilTyps.append(item->text());
    }
    rwSettings(true);
}
void AdvSearch::addClause()
{
    addClause(0);
}

#define HORADJ 50
#define VERTADJ 30

void AdvSearch::addClause(int tp)
{
    SearchClauseW *w = new SearchClauseW(this);
    m_clauseWins.push_back(w);
    connect(w->wordsLE, SIGNAL(returnPressed()),
	    this, SLOT(searchPB_clicked()));
    clauseVBox->addWidget(w);
    w->show();
    w->tpChange(tp);
    if (m_clauseWins.size() > iclausescnt) {
	delClausePB->setEnabled(true);
    } else {
	delClausePB->setEnabled(false);
    }
    // Have to adjust the size else we lose the bottom buttons! Why?
    QSize sz = AdvSearchBaseLayout->sizeHint();
    resize(QSize(sz.width()+HORADJ, sz.height()+VERTADJ));
}

void AdvSearch::delClause()
{
    if (m_clauseWins.size() <= iclausescnt)
	return;
    delete m_clauseWins.back();
    m_clauseWins.pop_back();
    if (m_clauseWins.size() > iclausescnt) {
	delClausePB->setEnabled(true);
    } else {
	delClausePB->setEnabled(false);
    }
    QSize sz = AdvSearchBaseLayout->sizeHint();
    resize(QSize(sz.width()+HORADJ, sz.height()+VERTADJ));
}

void AdvSearch::polish()
{
    AdvSearchBase::polish();
    QSize sz = sizeHint();
    resize(QSize(sz.width()+HORADJ+10, sz.height()+VERTADJ-20));
}

// Move selected file types from the ignored to the searched box
void AdvSearch::addFiltypPB_clicked()
{
    list<int> trl;
    QStringList moved;
    for (unsigned int i = 0; i < noFiltypsLB->count(); i++) {
	QListBoxItem *item = noFiltypsLB->item(i);
	if (item && item->isSelected()) {
	    moved.push_back(item->text());
	    trl.push_front(i);
	}
    }
    if (!moved.empty()) {
	yesFiltypsLB->insertStringList(moved);
	for (list<int>::iterator it = trl.begin();it != trl.end(); it++)
	    noFiltypsLB->removeItem(*it);
    }
    yesFiltypsLB->sort();
    noFiltypsLB->sort();
}

void AdvSearch::addAFiltypPB_clicked()
{
    for (unsigned int i = 0; i < noFiltypsLB->count();i++) {
	noFiltypsLB->setSelected(i, true);
    }
    addFiltypPB_clicked();
}

// Activate file type selection
void AdvSearch::restrictFtCB_toggled(bool on)
{
    yesFiltypsLB->setEnabled(on);
    delFiltypPB->setEnabled(on);
    addFiltypPB->setEnabled(on);
    delAFiltypPB->setEnabled(on);
    addAFiltypPB->setEnabled(on);
    noFiltypsLB->setEnabled(on);
    saveFileTypesPB->setEnabled(on);
}

using namespace Rcl;
void AdvSearch::searchPB_clicked()
{
    RefCntr<SearchData> sdata(new SearchData(conjunctCMB->currentItem() == 0 ?
					     SCLT_AND : SCLT_OR));
    bool hasnotnot = false;
    bool hasnot = false;

    for (list<SearchClauseW*>::iterator it = m_clauseWins.begin();
	 it != m_clauseWins.end(); it++) {
	SearchDataClause *cl;
	if ((cl = (*it)->getClause())) {
	    switch (cl->getTp()) {
	    case SCLT_EXCL: hasnot = true; break;
	    default: hasnotnot = true; break;
	    }
	    sdata->addClause(cl);
	}
    }
    if (!hasnotnot) {
	if (!hasnot)
	    return;
	QMessageBox::warning(0, "Recoll", tr("Cannot execute pure negative"
					     "query. Please enter common terms"
					     " in the 'any words' field")); 
	return;
    }
    if (restrictFtCB->isOn() && noFiltypsLB->count() > 0) {
	for (unsigned int i = 0; i < yesFiltypsLB->count(); i++) {
	    QCString ctext = yesFiltypsLB->item(i)->text().utf8();
	    sdata->addFiletype((const char *)ctext);
	}
    }

    if (!subtreeCMB->currentText().isEmpty()) {
	sdata->setTopdir((const char*)subtreeCMB->currentText().utf8());
	// The listbox is set for no insertion, do it. Have to check
	// for dups as the internal feature seems to only work for
	// user-inserted strings
	if (!subtreeCMB->listBox()->findItem(subtreeCMB->currentText(),
					     Qt::CaseSensitive|Qt::ExactMatch))
	    subtreeCMB->insertItem(subtreeCMB->currentText(), 0);
	// And keep it sorted
	subtreeCMB->listBox()->sort();
	prefs.asearchSubdirHist.clear();
	for (int index = 0; index < subtreeCMB->count(); index++)
	    prefs.asearchSubdirHist.push_back(subtreeCMB->text(index));
    }
    saveCnf();
    
    emit startSearch(sdata);
}

void AdvSearch::browsePB_clicked()
{
    QString dir = QFileDialog::getExistingDirectory();
    subtreeCMB->setEditText(dir);
}
