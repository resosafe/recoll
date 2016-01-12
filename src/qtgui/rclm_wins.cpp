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

#include <QShortcut>
#include <QMessageBox>

#include "debuglog.h"
#include "internfile.h"
#include "listdialog.h"
#include "confgui/confguiindex.h"
#include "idxsched.h"
#include "crontool.h"
#include "rtitool.h"
#include "snippets_w.h"
#include "fragbuts.h"
#include "specialindex.h"
#include "rclmain_w.h"

using namespace std;

static const QKeySequence quitKeySeq("Ctrl+q");
static const QKeySequence closeKeySeq("Ctrl+w");

// Open advanced search dialog.
void RclMain::showAdvSearchDialog()
{
    if (asearchform == 0) {
	asearchform = new AdvSearch(0);
        if (asearchform == 0) {
            return;
        }
	connect(new QShortcut(quitKeySeq, asearchform), SIGNAL (activated()), 
		this, SLOT (fileExit()));

	connect(asearchform, 
		SIGNAL(startSearch(RefCntr<Rcl::SearchData>, bool)), 
		this, SLOT(startSearch(RefCntr<Rcl::SearchData>, bool)));
	asearchform->show();
    } else {
	// Close and reopen, in hope that makes us visible...
	asearchform->close();
	asearchform->show();
    }
}

void RclMain::showSpellDialog()
{
    if (spellform == 0) {
	spellform = new SpellW(0);
	connect(new QShortcut(quitKeySeq, spellform), SIGNAL (activated()), 
		this, SLOT (fileExit()));
	connect(spellform, SIGNAL(wordSelect(QString)),
		sSearch, SLOT(addTerm(QString)));
	spellform->show();
    } else {
	// Close and reopen, in hope that makes us visible...
	spellform->close();
        spellform->show();
    }
}

void RclMain::showFragButs()
{
    if (fragbuts && fragbuts->isStale(0)) {
        deleteZ(fragbuts);
    }
    if (fragbuts == 0) {
	fragbuts = new FragButs(0);
        if (fragbuts->ok()) {
            fragbuts->show();
            connect(fragbuts, SIGNAL(fragmentsChanged()),
                    this, SLOT(onFragmentsChanged()));
        } else {
            delete fragbuts;
            fragbuts = 0;
        }
    } else {
	// Close and reopen, in hope that makes us visible...
	fragbuts->close();
        fragbuts->show();
    }
}

void RclMain::showSpecIdx()
{
    if (specidx == 0) {
	specidx = new SpecIdxW(0);
        connect(specidx, SIGNAL(accepted()), this, SLOT(specialIndex()));
        specidx->show();
    } else {
	// Close and reopen, in hope that makes us visible...
	specidx->close();
        specidx->show();
    }
}

void RclMain::showIndexConfig()
{
    showIndexConfig(false);
}
void RclMain::execIndexConfig()
{
    showIndexConfig(true);
}
void RclMain::showIndexConfig(bool modal)
{
    LOGDEB(("showIndexConfig()\n"));
    if (indexConfig == 0) {
	indexConfig = new ConfIndexW(0, theconfig);
	connect(new QShortcut(quitKeySeq, indexConfig), SIGNAL (activated()), 
		this, SLOT (fileExit()));
    } else {
	// Close and reopen, in hope that makes us visible...
	indexConfig->close();
	indexConfig->reloadPanels();
    }
    if (modal) {
	indexConfig->exec();
	indexConfig->setModal(false);
    } else {
	indexConfig->show();
    }
}

void RclMain::showIndexSched()
{
    showIndexSched(false);
}
void RclMain::execIndexSched()
{
    showIndexSched(true);
}
void RclMain::showIndexSched(bool modal)
{
    LOGDEB(("showIndexSched()\n"));
    if (indexSched == 0) {
	indexSched = new IdxSchedW(this);
	connect(new QShortcut(quitKeySeq, indexSched), SIGNAL (activated()), 
		this, SLOT (fileExit()));
	connect(indexSched->cronCLB, SIGNAL(clicked()), 
		this, SLOT(execCronTool()));
	if (theconfig && theconfig->isDefaultConfig()) {
#ifdef RCL_MONITOR
	    connect(indexSched->rtidxCLB, SIGNAL(clicked()), 
		    this, SLOT(execRTITool()));
#else
	    indexSched->rtidxCLB->setEnabled(false);
	    indexSched->rtidxCLB->setToolTip(tr("Disabled because the real time indexer was not compiled in."));
#endif
	} else {
	    indexSched->rtidxCLB->setEnabled(false);
	    indexSched->rtidxCLB->setToolTip(tr("This configuration tool only works for the main index."));
	}
    } else {
	// Close and reopen, in hope that makes us visible...
	indexSched->close();
    }
    if (modal) {
	indexSched->exec();
	indexSched->setModal(false);
    } else {
	indexSched->show();
    }
}

void RclMain::showCronTool()
{
    showCronTool(false);
}
void RclMain::execCronTool()
{
    showCronTool(true);
}
void RclMain::showCronTool(bool modal)
{
    LOGDEB(("showCronTool()\n"));
    if (cronTool == 0) {
	cronTool = new CronToolW(0);
	connect(new QShortcut(quitKeySeq, cronTool), SIGNAL (activated()), 
		this, SLOT (fileExit()));
    } else {
	// Close and reopen, in hope that makes us visible...
	cronTool->close();
    }
    if (modal) {
	cronTool->exec();
	cronTool->setModal(false);
    } else {
	cronTool->show();
    }
}

void RclMain::showRTITool()
{
    showRTITool(false);
}
void RclMain::execRTITool()
{
    showRTITool(true);
}
void RclMain::showRTITool(bool modal)
{
    LOGDEB(("showRTITool()\n"));
    if (rtiTool == 0) {
	rtiTool = new RTIToolW(0);
	connect(new QShortcut(quitKeySeq, rtiTool), SIGNAL (activated()), 
		this, SLOT (fileExit()));
    } else {
	// Close and reopen, in hope that makes us visible...
	rtiTool->close();
    }
    if (modal) {
	rtiTool->exec();
	rtiTool->setModal(false);
    } else {
	rtiTool->show();
    }
}

void RclMain::showUIPrefs()
{
    if (uiprefs == 0) {
	uiprefs = new UIPrefsDialog(this);
	connect(new QShortcut(quitKeySeq, uiprefs), SIGNAL (activated()), 
		this, SLOT (fileExit()));
	connect(uiprefs, SIGNAL(uiprefsDone()), this, SLOT(setUIPrefs()));
	connect(this, SIGNAL(stemLangChanged(const QString&)), 
		uiprefs, SLOT(setStemLang(const QString&)));
    } else {
	// Close and reopen, in hope that makes us visible...
	uiprefs->close();
        rwSettings(false);
        uiprefs->setFromPrefs();
    }
    uiprefs->show();
}

void RclMain::showExtIdxDialog()
{
    if (uiprefs == 0) {
	uiprefs = new UIPrefsDialog(this);
	connect(new QShortcut(quitKeySeq, uiprefs), SIGNAL (activated()), 
		this, SLOT (fileExit()));
	connect(uiprefs, SIGNAL(uiprefsDone()), this, SLOT(setUIPrefs()));
    } else {
	// Close and reopen, in hope that makes us visible...
	uiprefs->close();
    }
    uiprefs->tabWidget->setCurrentIndex(3);
    uiprefs->show();
}

void RclMain::showAboutDialog()
{
    string vstring = Rcl::version_string() +
        string("<br> http://www.recoll.org") +
	string("<br> http://www.xapian.org");
    QMessageBox::information(this, tr("About Recoll"), vstring.c_str());
}

void RclMain::showMissingHelpers()
{
    string miss;
    if (!theconfig->getMissingHelperDesc(miss)) {
	QMessageBox::information(this, "", tr("Indexing did not run yet"));
	return;
    }
    QString msg = QString::fromUtf8("<p>") +
	tr("External applications/commands needed for your file types "
	   "and not found, as stored by the last indexing pass in ");
    msg += "<i>";
    msg += QString::fromLocal8Bit(theconfig->getConfDir().c_str());
    msg += "/missing</i>:<pre>\n";
    if (!miss.empty()) {
	msg += QString::fromUtf8(miss.c_str());
    } else {
	msg += tr("No helpers found missing");
    }
    msg += "</pre>";
    QMessageBox::information(this, tr("Missing helper programs"), msg);
}

void RclMain::showActiveTypes()
{
    if (rcldb == 0) {
	QMessageBox::warning(0, tr("Error"), 
			     tr("Index not open"),
			     QMessageBox::Ok, 
			     QMessageBox::NoButton);
	return;
    }

    // All mime types in index. 
    vector<string> vdbtypes;
    if (!rcldb->getAllDbMimeTypes(vdbtypes)) {
	QMessageBox::warning(0, tr("Error"), 
			     tr("Index query error"),
			     QMessageBox::Ok, 
			     QMessageBox::NoButton);
	return;
    }
    set<string> mtypesfromdb;
    mtypesfromdb.insert(vdbtypes.begin(), vdbtypes.end());

    // All types listed in mimeconf:
    vector<string> mtypesfromconfig = theconfig->getAllMimeTypes();

    // Intersect file system types with config types (those not in the
    // config can be indexed by name, not by content)
    set<string> mtypesfromdbconf;
    for (vector<string>::const_iterator it = mtypesfromconfig.begin();
	 it != mtypesfromconfig.end(); it++) {
	if (mtypesfromdb.find(*it) != mtypesfromdb.end())
	    mtypesfromdbconf.insert(*it);
    }

    // Substract the types for missing helpers (the docs are indexed
    // by name only):
    string miss;
    if (theconfig->getMissingHelperDesc(miss) && !miss.empty()) {
	FIMissingStore st(miss);
	map<string, set<string> >::const_iterator it;
	for (it = st.m_typesForMissing.begin(); 
	     it != st.m_typesForMissing.end(); it++) {
	    set<string>::const_iterator it1;
	    for (it1 = it->second.begin(); 
		 it1 != it->second.end(); it1++) {
		set<string>::iterator it2 = mtypesfromdbconf.find(*it1);
		if (it2 != mtypesfromdbconf.end())
		    mtypesfromdbconf.erase(it2);
	    }
	}	
    }
    ListDialog dialog;
    dialog.setWindowTitle(tr("Indexed MIME Types"));

    // Turn the result into a string and display
    dialog.groupBox->setTitle(tr("Content has been indexed for these mime types:"));

    // We replace the list with an editor so that the user can copy/paste
    delete dialog.listWidget;
    QTextEdit *editor = new QTextEdit(dialog.groupBox);
    editor->setReadOnly(true);
    dialog.horizontalLayout->addWidget(editor);

    for (set<string>::const_iterator it = mtypesfromdbconf.begin(); 
	 it != mtypesfromdbconf.end(); it++) {
	editor->append(QString::fromUtf8(it->c_str()));
    }
    editor->moveCursor(QTextCursor::Start);
    editor->ensureCursorVisible();
    dialog.exec();
}

void RclMain::newDupsW(const Rcl::Doc, const vector<Rcl::Doc> dups)
{
    ListDialog dialog;
    dialog.setWindowTitle(tr("Duplicate documents"));

    dialog.groupBox->setTitle(tr("These Urls ( | ipath) share the same"
				 " content:"));
    // We replace the list with an editor so that the user can copy/paste
    delete dialog.listWidget;
    QTextEdit *editor = new QTextEdit(dialog.groupBox);
    editor->setReadOnly(true);
    dialog.horizontalLayout->addWidget(editor);

    for (vector<Rcl::Doc>::const_iterator it = dups.begin(); 
	 it != dups.end(); it++) {
	if (it->ipath.empty()) 
	    editor->append(QString::fromLocal8Bit(it->url.c_str()));
	else 
	    editor->append(QString::fromLocal8Bit(it->url.c_str()) + " | " +
			   QString::fromUtf8(it->ipath.c_str()));
    }
    editor->moveCursor(QTextCursor::Start);
    editor->ensureCursorVisible();
    dialog.exec();
}

void RclMain::showSnippets(Rcl::Doc doc)
{
    SnippetsW *sp = new SnippetsW(doc, m_source);
    connect(sp, SIGNAL(startNativeViewer(Rcl::Doc, int, QString)),
	    this, SLOT(startNativeViewer(Rcl::Doc, int, QString)));
    connect(new QShortcut(quitKeySeq, sp), SIGNAL (activated()), 
	    this, SLOT (fileExit()));
    connect(new QShortcut(closeKeySeq, sp), SIGNAL (activated()), 
	    sp, SLOT (close()));
    sp->show();
}
