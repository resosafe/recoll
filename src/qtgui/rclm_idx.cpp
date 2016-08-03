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

#include <signal.h>
#include "safeunistd.h"

#include <QMessageBox>
#include <QTimer>

#include "execmd.h"
#include "debuglog.h"
#include "transcode.h"
#include "indexer.h"
#include "rclmain_w.h"
#include "specialindex.h"

using namespace std;

void RclMain::updateIdxStatus()
{
    ConfSimple cs(theconfig->getIdxStatusFile().c_str(), 1);
    QString msg = tr("Indexing in progress: ");
    DbIxStatus status;
    string val;
    cs.get("phase", val);
    status.phase = DbIxStatus::Phase(atoi(val.c_str()));
    cs.get("fn", status.fn);
    cs.get("docsdone", &status.docsdone);
    cs.get("filesdone", &status.filesdone);
    cs.get("fileerrors", &status.fileerrors);
    cs.get("dbtotdocs", &status.dbtotdocs);
    cs.get("totfiles", &status.totfiles);

    QString phs;
    switch (status.phase) {
    case DbIxStatus::DBIXS_NONE:phs=tr("None");break;
    case DbIxStatus::DBIXS_FILES: phs=tr("Updating");break;
    case DbIxStatus::DBIXS_PURGE: phs=tr("Purge");break;
    case DbIxStatus::DBIXS_STEMDB: phs=tr("Stemdb");break;
    case DbIxStatus::DBIXS_CLOSING:phs=tr("Closing");break;
    case DbIxStatus::DBIXS_DONE:phs=tr("Done");break;
    case DbIxStatus::DBIXS_MONITOR:phs=tr("Monitor");break;
    default: phs=tr("Unknown");break;
    }
    msg += phs + " ";
    if (status.phase == DbIxStatus::DBIXS_FILES) {
	char cnts[100];
	if (status.dbtotdocs > 0) {
	    sprintf(cnts, "(%d docs/%d files/%d errors/%d tot files) ",
                    status.docsdone, status.filesdone, status.fileerrors,
		    status.totfiles);
        } else {
	    sprintf(cnts, "(%d docs/%d files/%d errors) ",
                    status.docsdone, status.filesdone, status.fileerrors);
        }
	msg += QString::fromUtf8(cnts) + " ";
    }
    string mf;int ecnt = 0;
    string fcharset = theconfig->getDefCharset(true);
    if (!transcode(status.fn, mf, fcharset, "UTF-8", &ecnt) || ecnt) {
	mf = url_encode(status.fn, 0);
    }
    msg += QString::fromUtf8(mf.c_str());
    statusBar()->showMessage(msg, 4000);
}

// This is called by a periodic timer to check the status of 
// indexing, a possible need to exit, and cleanup exited viewers
void RclMain::periodic100()
{
    LOGDEB2(("Periodic100\n"));
    if (m_idxproc) {
	// An indexing process was launched. If its' done, see status.
	int status;
	bool exited = m_idxproc->maybereap(&status);
	if (exited) {
	    deleteZ(m_idxproc);
	    if (status) {
                if (m_idxkilled) {
                    QMessageBox::warning(0, "Recoll", 
                                         tr("Indexing interrupted"));
                    m_idxkilled = false;
                } else {
                    QMessageBox::warning(0, "Recoll", 
                                         tr("Indexing failed"));
                }
	    } else {
		// On the first run, show missing helpers. We only do this once
		if (m_firstIndexing)
		    showMissingHelpers();
	    }
	    string reason;
	    maybeOpenDb(reason, 1);
	} else {
	    // update/show status even if the status file did not
	    // change (else the status line goes blank during
	    // lengthy operations).
	    updateIdxStatus();
	}
    }
    // Update the "start/stop indexing" menu entry, can't be done from
    // the "start/stop indexing" slot itself
    IndexerState prevstate = m_indexerState;
    if (m_idxproc) {
	m_indexerState = IXST_RUNNINGMINE;
	fileToggleIndexingAction->setText(tr("Stop &Indexing"));
	fileToggleIndexingAction->setEnabled(true);
	fileRebuildIndexAction->setEnabled(false);
        actionSpecial_Indexing->setEnabled(false);
	periodictimer->setInterval(200);
    } else {
	Pidfile pidfile(theconfig->getPidfile());
        pid_t pid = pidfile.open();
        if (pid == getpid()) {
            // Locked by me
	    m_indexerState = IXST_NOTRUNNING;
	    fileToggleIndexingAction->setText(tr("Index locked"));
	    fileToggleIndexingAction->setEnabled(false);
	    fileRebuildIndexAction->setEnabled(false);
            actionSpecial_Indexing->setEnabled(false);
	    periodictimer->setInterval(1000);
        } else if (pid == 0) {
	    m_indexerState = IXST_NOTRUNNING;
	    fileToggleIndexingAction->setText(tr("Update &Index"));
	    fileToggleIndexingAction->setEnabled(true);
	    fileRebuildIndexAction->setEnabled(true);
            actionSpecial_Indexing->setEnabled(true);
	    periodictimer->setInterval(1000);
	} else {
	    // Real time or externally started batch indexer running
	    m_indexerState = IXST_RUNNINGNOTMINE;
	    fileToggleIndexingAction->setText(tr("Stop &Indexing"));
	    fileToggleIndexingAction->setEnabled(true);
	    fileRebuildIndexAction->setEnabled(false);
            actionSpecial_Indexing->setEnabled(false);
	    periodictimer->setInterval(200);
	}	    
    }

    if ((prevstate == IXST_RUNNINGMINE || prevstate == IXST_RUNNINGNOTMINE)
        && m_indexerState == IXST_NOTRUNNING) {
        showTrayMessage("Indexing done");
    }

    // Possibly cleanup the dead viewers
    for (vector<ExecCmd*>::iterator it = m_viewers.begin();
	 it != m_viewers.end(); it++) {
	int status;
	if ((*it)->maybereap(&status)) {
	    deleteZ(*it);
	}
    }
    vector<ExecCmd*> v;
    for (vector<ExecCmd*>::iterator it = m_viewers.begin();
	 it != m_viewers.end(); it++) {
	if (*it)
	    v.push_back(*it);
    }
    m_viewers = v;

    if (recollNeedsExit)
	fileExit();
}

bool RclMain::checkIdxPaths()
{
    vector<string> args;

    string badpaths;
    args.push_back("recollindex");
    args.push_back("-E");
    args.push_back("-c");
    args.push_back(theconfig->getConfDir());
    ExecCmd::backtick(args, badpaths);
    if (!badpaths.empty()) {
        int rep = 
            QMessageBox::warning(0, tr("Bad paths"), 
                                 tr("Bad paths in configuration file:\n") +
                                 QString::fromLocal8Bit(badpaths.c_str()),
                                 QMessageBox::Ok,
                                 QMessageBox::Cancel,
                                 QMessageBox::NoButton);
        if (rep == QMessageBox::Cancel)
            return false;
    }
    return true;
}

// This gets called when the "update index" action is activated. It executes
// the requested action, and disables the menu entry. This will be
// re-enabled by the indexing status check
void RclMain::toggleIndexing()
{
    switch (m_indexerState) {
    case IXST_RUNNINGMINE:
	if (m_idxproc) {
	    // Indexing was in progress, request stop. Let the periodic
	    // routine check for the results.
            if (m_idxproc->requestChildExit()) {
                m_idxkilled = true;
            }
	}
	break;
    case IXST_RUNNINGNOTMINE:
    {
#ifdef _WIN32
        QMessageBox::warning(0, tr("Warning"),
                             tr("The current indexing process "
                                "was not started from this "
                                "interface, can't kill it"),
                             QMessageBox::Ok,
                             QMessageBox::NoButton);
#else
	int rep = 
	    QMessageBox::information(0, tr("Warning"), 
				     tr("The current indexing process "
					"was not started from this "
					"interface. Click Ok to kill it "
					"anyway, or Cancel to leave it alone"),
					 QMessageBox::Ok,
					 QMessageBox::Cancel,
					 QMessageBox::NoButton);
	if (rep == QMessageBox::Ok) {
	    Pidfile pidfile(theconfig->getPidfile());
	    pid_t pid = pidfile.open();
	    if (pid > 0)
		kill(pid, SIGTERM);
	}
#endif
    }
    break;
    case IXST_NOTRUNNING:
    {
	// Could also mean that no helpers are missing, but then we
	// won't try to show a message anyway (which is what
	// firstIndexing is used for)
	string mhd;
	m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);

        if (!checkIdxPaths()) {
                return;
        }
        vector<string> args;
	args.push_back("-c");
	args.push_back(theconfig->getConfDir());
	m_idxproc = new ExecCmd;
	m_idxproc->startExec("recollindex", args, false, false);
    }
    break;
    case IXST_UNKNOWN:
        return;
    }
}

void RclMain::rebuildIndex()
{
    switch (m_indexerState) {
    case IXST_UNKNOWN:
    case IXST_RUNNINGMINE:
    case IXST_RUNNINGNOTMINE:
	return; //?? Should not have been called
    case IXST_NOTRUNNING:
    {
        if (m_idxproc) {
            LOGERR(("RclMain::rebuildIndex: current indexer exec not null\n"));
            return;
        }
	int rep = 
	    QMessageBox::warning(0, tr("Erasing index"), 
				     tr("Reset the index and start "
					"from scratch ?"),
					 QMessageBox::Ok,
					 QMessageBox::Cancel,
					 QMessageBox::NoButton);
	if (rep == QMessageBox::Ok) {
#ifdef _WIN32
            // Under windows, it's necessary to close the db here,
            // else Xapian won't be able to do what it wants with the
            // (open) files. Of course if there are several GUI
            // instances, this won't work...
            if (rcldb)
                rcldb->close();
#endif // _WIN32
	    // Could also mean that no helpers are missing, but then we
	    // won't try to show a message anyway (which is what
	    // firstIndexing is used for)
	    string mhd;
	    m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);

            if (!checkIdxPaths()) {
                return;
            }
            
	    vector<string> args;
	    args.push_back("-c");
	    args.push_back(theconfig->getConfDir());
	    args.push_back("-z");
	    m_idxproc = new ExecCmd;
	    m_idxproc->startExec("recollindex", args, false, false);
	}
    }
    break;
    }
}

void SpecIdxW::onBrowsePB_clicked()
{
    QString dir = myGetFileName(true, tr("Top indexed entity"), true);
    targLE->setText(dir);
}

bool SpecIdxW::noRetryFailed()
{
    return noRetryFailedCB->isChecked();
}

bool SpecIdxW::eraseFirst()
{
    return eraseBeforeCB->isChecked();
}

std::vector<std::string> SpecIdxW::selpatterns()
{
    vector<string> pats;
    string text = qs2utf8s(selPatsLE->text());
    if (!text.empty()) {
        stringToStrings(text, pats);
    }
    return pats;
}

std::string SpecIdxW::toptarg()
{
    return qs2utf8s(targLE->text());
}

void SpecIdxW::onTargLE_textChanged(const QString& text)
{
    if (text.isEmpty())
        selPatsLE->setEnabled(false);
    else
        selPatsLE->setEnabled(true);
}

static string execToString(const string& cmd, const vector<string>& args)
{
    string command = cmd + " ";
    for (vector<string>::const_iterator it = args.begin();
         it != args.end(); it++) {
        command += "{" + *it + "} ";
    }
    return command;
}

void RclMain::specialIndex()
{
    LOGDEB(("RclMain::specialIndex\n"));
    switch (m_indexerState) {
    case IXST_UNKNOWN:
    case IXST_RUNNINGMINE:
    case IXST_RUNNINGNOTMINE:
	return; //?? Should not have been called
    case IXST_NOTRUNNING:
    default:
        break;
    }
    if (m_idxproc) {
        LOGERR(("RclMain::rebuildIndex: current indexer exec not null\n"));
        return;
    }
    if (!specidx) // ??
        return;

    vector<string> args;
    args.push_back("-c");
    args.push_back(theconfig->getConfDir());

    string top = specidx->toptarg();
    if (!top.empty()) {
        args.push_back("-r");
    }

    if (specidx->eraseFirst()) {
        if (top.empty()) {
            args.push_back("-Z");
        } else {
            args.push_back("-e");
        }
    } 

    // The default for retrying differ depending if -r is set
    if (top.empty()) {
        if (!specidx->noRetryFailed()) {
            args.push_back("-k");
        }
    } else {
        if (specidx->noRetryFailed()) {
            args.push_back("-K");
        }
    }
        
    vector<string> selpats = specidx->selpatterns();
    if (!selpats.empty() && top.empty()) {
        QMessageBox::warning(0, tr("Selection patterns need topdir"), 
                             tr("Selection patterns can only be used with a "
                                "start directory"),
                                     QMessageBox::Ok,
                                     QMessageBox::NoButton);
        return;
    }

    for (vector<string>::const_iterator it = selpats.begin();
         it != selpats.end(); it++) {
        args.push_back("-p");
        args.push_back(*it);
    }
    if (!top.empty()) {
        args.push_back(top);
    }
    m_idxproc = new ExecCmd;
    LOGINFO(("specialIndex: exec: %s\n", 
            execToString("recollindex", args).c_str()));
    m_idxproc->startExec("recollindex", args, false, false);
}

void RclMain::updateIdxForDocs(vector<Rcl::Doc>& docs)
{
    if (m_idxproc) {
	QMessageBox::warning(0, tr("Warning"), 
			     tr("Can't update index: indexer running"),
			     QMessageBox::Ok, 
			     QMessageBox::NoButton);
	return;
    }
	
    vector<string> paths;
    if (ConfIndexer::docsToPaths(docs, paths)) {
	vector<string> args;
	args.push_back("-c");
	args.push_back(theconfig->getConfDir());
	args.push_back("-e");
	args.push_back("-i");
	args.insert(args.end(), paths.begin(), paths.end());
	m_idxproc = new ExecCmd;
	m_idxproc->startExec("recollindex", args, false, false);
	fileToggleIndexingAction->setText(tr("Stop &Indexing"));
    }
    fileToggleIndexingAction->setEnabled(false);
    actionSpecial_Indexing->setEnabled(false);
}

