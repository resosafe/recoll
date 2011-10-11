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
#include "autoconfig.h"

#include <sys/stat.h>

#include <string>
#include <algorithm>
#include <list>

#include <qfontdialog.h>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qvariant.h>
#include <qpushbutton.h>
#include <qtabwidget.h>
#include <qlistwidget.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qspinbox.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qtextedit.h>
#include <qlist.h>

#include "recoll.h"
#include "guiutils.h"
#include "rcldb.h"
#include "rclconfig.h"
#include "pathut.h"
#include "uiprefs_w.h"
#include "viewaction_w.h"
#include "debuglog.h"

void UIPrefsDialog::init()
{
    m_viewAction = 0;

    connect(viewActionPB, SIGNAL(clicked()), this, SLOT(showViewAction()));
    connect(reslistFontPB, SIGNAL(clicked()), this, SLOT(showFontDialog()));
    connect(resetFontPB, SIGNAL(clicked()), this, SLOT(resetReslistFont()));

    connect(addExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(addExtraDbPB_clicked()));
    connect(delExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(delExtraDbPB_clicked()));
    connect(togExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(togExtraDbPB_clicked()));
    connect(actAllExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(actAllExtraDbPB_clicked()));
    connect(unacAllExtraDbPB, SIGNAL(clicked()), 
	    this, SLOT(unacAllExtraDbPB_clicked()));

    connect(buttonOk, SIGNAL(clicked()), this, SLOT(accept()));
    connect(buttonCancel, SIGNAL(clicked()), this, SLOT(reject()));
    connect(buildAbsCB, SIGNAL(toggled(bool)), 
	    replAbsCB, SLOT(setEnabled(bool)));
    connect(useDesktopOpenCB, SIGNAL(toggled(bool)), 
	    viewActionPB, SLOT(setDisabled(bool)));

    setFromPrefs();
}

// Update dialog state from stored prefs
void UIPrefsDialog::setFromPrefs()
{
    // Entries per result page spinbox
    pageLenSB->setValue(prefs.respagesize);
    collapseDupsCB->setChecked(prefs.collapseDuplicates);
    maxHLTSB->setValue(prefs.maxhltextmbs);
    catgToolBarCB->setChecked(prefs.catgToolBar);
    autoSearchCB->setChecked(prefs.autoSearchOnWS);
    syntlenSB->setValue(prefs.syntAbsLen);
    syntctxSB->setValue(prefs.syntAbsCtx);

    initStartAdvCB->setChecked(prefs.startWithAdvSearchOpen);

    // External editor. Can use desktop prefs or internal
    useDesktopOpenCB->setChecked(prefs.useDesktopOpen);
    viewActionPB->setEnabled(!prefs.useDesktopOpen);

    keepSortCB->setChecked(prefs.keepSort);
    previewHtmlCB->setChecked(prefs.previewHtml);
    previewPlainPreCB->setChecked(prefs.previewPlainPre);
    // Query terms color
    qtermColorLE->setText(prefs.qtermcolor);
    // Abstract snippet separator string
    abssepLE->setText(prefs.abssep);

    // Result list font family and size
    reslistFontFamily = prefs.reslistfontfamily;
    reslistFontSize = prefs.reslistfontsize;
    QString s;
    if (prefs.reslistfontfamily.length() == 0) {
	reslistFontPB->setText(this->font().family() + "-" +
			       s.setNum(this->font().pointSize()));
    } else {
	reslistFontPB->setText(reslistFontFamily + "-" +
			       s.setNum(reslistFontSize));
    }
    rlfTE->setPlainText(prefs.reslistformat);

    // Stemming language combobox
    stemLangCMB->clear();
    stemLangCMB->addItem(g_stringNoStem);
    stemLangCMB->addItem(g_stringAllStem);
    list<string> langs;
    if (!getStemLangs(langs)) {
	QMessageBox::warning(0, "Recoll", 
			     tr("error retrieving stemming languages"));
    }
    int cur = prefs.queryStemLang == ""  ? 0 : 1;
    for (list<string>::const_iterator it = langs.begin(); 
	 it != langs.end(); it++) {
	stemLangCMB->
	    addItem(QString::fromAscii(it->c_str(), it->length()));
	if (cur == 0 && !strcmp((const char*)prefs.queryStemLang.toAscii(), 
				it->c_str())) {
	    cur = stemLangCMB->count();
	}
    }
    stemLangCMB->setCurrentIndex(cur);

    autoPhraseCB->setChecked(prefs.ssearchAutoPhrase);

    buildAbsCB->setChecked(prefs.queryBuildAbstract);
    replAbsCB->setEnabled(prefs.queryBuildAbstract);
    replAbsCB->setChecked(prefs.queryReplaceAbstract);

    autoSuffsCB->setChecked(prefs.autoSuffsEnable);
    autoSuffsLE->setText(prefs.autoSuffs);

    // Initialize the extra indexes listboxes
    idxLV->clear();
    for (list<string>::iterator it = prefs.allExtraDbs.begin(); 
	 it != prefs.allExtraDbs.end(); it++) {
	QListWidgetItem *item = 
	    new QListWidgetItem(QString::fromLocal8Bit(it->c_str()), 
				idxLV);
	if (item) 
	    item->setCheckState(Qt::Unchecked);
    }
    for (list<string>::iterator it = prefs.activeExtraDbs.begin(); 
	 it != prefs.activeExtraDbs.end(); it++) {
	QList<QListWidgetItem *>items =
	     idxLV->findItems (QString::fromLocal8Bit(it->c_str()), 
			       Qt::MatchFixedString|Qt::MatchCaseSensitive);
	for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	     it != items.end(); it++) {
	    (*it)->setCheckState(Qt::Checked);
	}
    }
    idxLV->sortItems();
}

void UIPrefsDialog::accept()
{
    prefs.autoSearchOnWS = autoSearchCB->isChecked();
    prefs.catgToolBar = catgToolBarCB->isChecked();
    prefs.respagesize = pageLenSB->value();
    prefs.collapseDuplicates = collapseDupsCB->isChecked();
    prefs.maxhltextmbs = maxHLTSB->value();

    prefs.qtermcolor = qtermColorLE->text();
    prefs.abssep = abssepLE->text();

    prefs.reslistfontfamily = reslistFontFamily;
    prefs.reslistfontsize = reslistFontSize;
    prefs.reslistformat =  rlfTE->toPlainText();
    if (prefs.reslistformat.trimmed().isEmpty()) {
	prefs.reslistformat = prefs.dfltResListFormat;
	rlfTE->setPlainText(prefs.reslistformat);
    }
    prefs.creslistformat = (const char*)prefs.reslistformat.toUtf8();

    if (stemLangCMB->currentIndex() == 0) {
	prefs.queryStemLang = "";
    } else if (stemLangCMB->currentIndex() == 1) {
	prefs.queryStemLang = "ALL";
    } else {
	prefs.queryStemLang = stemLangCMB->currentText();
    }
    prefs.ssearchAutoPhrase = autoPhraseCB->isChecked();
    prefs.queryBuildAbstract = buildAbsCB->isChecked();
    prefs.queryReplaceAbstract = buildAbsCB->isChecked() && 
	replAbsCB->isChecked();

    prefs.startWithAdvSearchOpen = initStartAdvCB->isChecked();
    prefs.useDesktopOpen = useDesktopOpenCB->isChecked();
    prefs.keepSort = keepSortCB->isChecked();
    prefs.previewHtml = previewHtmlCB->isChecked();
    prefs.previewPlainPre = previewPlainPreCB->isChecked();

    prefs.syntAbsLen = syntlenSB->value();
    prefs.syntAbsCtx = syntctxSB->value();

    
    prefs.autoSuffsEnable = autoSuffsCB->isChecked();
    prefs.autoSuffs = autoSuffsLE->text();

    prefs.allExtraDbs.clear();
    prefs.activeExtraDbs.clear();
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	if (item) {
	    prefs.allExtraDbs.push_back((const char *)item->text().toLocal8Bit());
	    if (item->checkState() == Qt::Checked) {
		prefs.activeExtraDbs.push_back((const char *)
					       item->text().toLocal8Bit());
	    }
	}
    }

    rwSettings(true);
    string reason;
    maybeOpenDb(reason, true);
    emit uiprefsDone();
    QDialog::accept();
}

void UIPrefsDialog::reject()
{
    setFromPrefs();
    QDialog::reject();
}

void UIPrefsDialog::setStemLang(const QString& lang)
{
    int cur = 0;
    if (lang == "") {
	cur = 0;
    } else if (lang == "ALL") {
	cur = 1;
    } else {
	for (int i = 1; i < stemLangCMB->count(); i++) {
	    if (lang == stemLangCMB->itemText(i)) {
		cur = i;
		break;
	    }
	}
    }
    stemLangCMB->setCurrentIndex(cur);
}

void UIPrefsDialog::showFontDialog()
{
    bool ok;
    QFont font;
    if (prefs.reslistfontfamily.length()) {
	font.setFamily(prefs.reslistfontfamily);
	font.setPointSize(prefs.reslistfontsize);
    }

    font = QFontDialog::getFont(&ok, font, this);
    if (ok) {
	// Check if the default font was set, in which case we
	// erase the preference
	if (font.family().compare(this->font().family()) || 
	    font.pointSize() != this->font().pointSize()) {
	    reslistFontFamily = font.family();
	    reslistFontSize = font.pointSize();
	    QString s;
	    reslistFontPB->setText(reslistFontFamily + "-" +
			       s.setNum(reslistFontSize));
	} else {
	    reslistFontFamily = "";
	    reslistFontSize = 0;
	}
    }
}

void UIPrefsDialog::resetReslistFont()
{
    reslistFontFamily = "";
    reslistFontSize = 0;
    reslistFontPB->setText(this->font().family() + "-" +
			   QString().setNum(this->font().pointSize()));
}

void UIPrefsDialog::showViewAction()
{
    if (m_viewAction== 0) {
	m_viewAction = new ViewAction(0);
    } else {
	// Close and reopen, in hope that makes us visible...
	m_viewAction->close();
    }
    m_viewAction->show();
}
void UIPrefsDialog::showViewAction(const QString& mt)
{
    showViewAction();
    m_viewAction->selectMT(mt);
}

////////////////////////////////////////////
// External / extra search indexes setup

void UIPrefsDialog::togExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	if (item->isSelected()) {
	    if (item->checkState() == Qt::Checked) {
		item->setCheckState(Qt::Unchecked);
	    } else {
		item->setCheckState(Qt::Checked);
	    }
	}
    }
}
void UIPrefsDialog::actAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	    item->setCheckState(Qt::Checked);
    }
}
void UIPrefsDialog::unacAllExtraDbPB_clicked()
{
    for (int i = 0; i < idxLV->count(); i++) {
	QListWidgetItem *item = idxLV->item(i);
	    item->setCheckState(Qt::Unchecked);
    }
}

void UIPrefsDialog::delExtraDbPB_clicked()
{
    QList<QListWidgetItem *> items = idxLV->selectedItems();
    for (QList<QListWidgetItem *>::iterator it = items.begin(); 
	 it != items.end(); it++) {
	delete *it;
    }
}

/** 
 * Browse to add another index.
 * We do a textual comparison to check for duplicates, except for
 * the main db for which we check inode numbers. 
 */
void UIPrefsDialog::addExtraDbPB_clicked()
{
    QString input = myGetFileName(true, 
				  tr("Select xapian index directory "
				     "(ie: /home/buddy/.recoll/xapiandb)"));

    if (input.isEmpty())
	return;

    string dbdir = (const char *)input.toLocal8Bit();
    LOGDEB(("ExtraDbDial: got: [%s]\n", dbdir.c_str()));
    path_catslash(dbdir);
    if (!Rcl::Db::testDbDir(dbdir)) {
	QMessageBox::warning(0, "Recoll", 
        tr("The selected directory does not appear to be a Xapian index"));
	return;
    }
    struct stat st1, st2;
    stat(dbdir.c_str(), &st1);
    string rcldbdir = theconfig->getDbDir();
    stat(rcldbdir.c_str(), &st2);
    path_catslash(rcldbdir);

    if (st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino) {
	QMessageBox::warning(0, "Recoll", 
			     tr("This is the main/local index!"));
	return;
    }

    // For some reason, finditem (which we used to use to detect duplicates 
    // here) does not work anymore here: qt 4.6.3
    QList<QListWidgetItem *>items =
	idxLV->findItems (input, Qt::MatchFixedString|Qt::MatchCaseSensitive);
    if (!items.empty()) {
	    QMessageBox::warning(0, "Recoll", 
		 tr("The selected directory is already in the index list"));
	    return;
    }
#if 0
    string nv = (const char *)input.toLocal8Bit();
    QListViewItemIterator it(idxLV);
    while (it.current()) {
	QCheckListItem *item = (QCheckListItem *)it.current();
	string ov = (const char *)item->text().toLocal8Bit();
	if (!ov.compare(nv)) {
	    QMessageBox::warning(0, "Recoll", 
		 tr("The selected directory is already in the index list"));
	    return;
	}
	++it;
    }
#endif
    QListWidgetItem *item = new QListWidgetItem(input, idxLV);
    item->setCheckState(Qt::Unchecked);
    idxLV->sortItems();
}
