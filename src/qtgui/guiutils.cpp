/* Copyright (C) 2005 Jean-Francois Dockes
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

#include <algorithm>
#include <cstdio>

#include "recoll.h"
#include "debuglog.h"
#include "smallut.h"
#include "guiutils.h"
#include "pathut.h"
#include "base64.h"
#include "transcode.h"

#include <QSettings>
#include <QStringList>
#include <QFileDialog>

RclDynConf *g_dynconf;

// The table should not be necessary, but I found no css way to get
// qt 4.6 qtextedit to clear the margins after the float img without 
// introducing blank space.
const char *PrefsPack::dfltResListFormat = 
#if 1
	"<table><tr><td><img src='%I'></td>"
	"<td>%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
	"%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
	"%A %K</td></tr></table>"
#elif 0
	"<img src='%I' align='left'>"
	"%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
	"%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
	"%A %K"
#endif
    ;

const char *v114reslistformat = "<img src=\"%I\" align=\"left\">"
	"%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
	"%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i>&nbsp;%i<br>"
	"%A %K";


// The global preferences structure
PrefsPack prefs;

// Using the same macro to read/write a setting. insurance against typing 
// mistakes
#define PARS(X) (X)
#define SETTING_RW(var, nm, tp, def)			\
    if (writing) {					\
	settings.setValue(nm , var);			\
    } else {						\
	var = settings.value(nm, def).to##tp		\
	    ();						\
    }						

/** 
 * Saving and restoring user preferences. These are stored in a global
 * structure during program execution and saved to disk using the QT
 * settings mechanism
 */
void rwSettings(bool writing)
{
    LOGDEB1(("rwSettings: write %d\n", int(writing)));
    QSettings settings("Recoll.org", "recoll");
    SETTING_RW(prefs.mainwidth, "/Recoll/geometry/width", Int, 0);
    SETTING_RW(prefs.mainheight, "/Recoll/geometry/height", Int, 0);
    SETTING_RW(prefs.pvwidth, "/Recoll/geometry/pvwidth", Int, 0);
    SETTING_RW(prefs.pvheight, "/Recoll/geometry/pvheight", Int, 0);
    SETTING_RW(prefs.ssearchTyp, "/Recoll/prefs/simpleSearchTyp", Int, 3);
    SETTING_RW(prefs.startWithAdvSearchOpen, 
	       "/Recoll/prefs/startWithAdvSearchOpen", Bool, false);
    SETTING_RW(prefs.previewHtml, 
	       "/Recoll/prefs/previewHtml", Bool, true);

    QString advSearchClauses;
    QString ascdflt;
    if (writing) {
	for (vector<int>::iterator it = prefs.advSearchClauses.begin();
	     it != prefs.advSearchClauses.end(); it++) {
	    char buf[20];
	    sprintf(buf, "%d ", *it);
	    advSearchClauses += QString::fromAscii(buf);
	}
    }
    SETTING_RW(advSearchClauses, "/Recoll/prefs/adv/clauseList", String, ascdflt);
    if (!writing) {
	list<string> clauses;
	stringToStrings((const char *)advSearchClauses.toUtf8(), clauses);
	for (list<string>::iterator it = clauses.begin(); 
	     it != clauses.end(); it++) {
	    prefs.advSearchClauses.push_back(atoi(it->c_str()));
	}
    }

    SETTING_RW(prefs.autoSearchOnWS, "/Recoll/prefs/reslist/autoSearchOnWS", 
	       Bool, false);
    SETTING_RW(prefs.catgToolBar, "/Recoll/prefs/catgToolBar", Bool, false);
    SETTING_RW(prefs.ssearchAutoPhrase, 
	       "/Recoll/prefs/ssearchAutoPhrase", Bool, false);
    SETTING_RW(prefs.respagesize, "/Recoll/prefs/reslist/pagelen", Int, 8);
    SETTING_RW(prefs.collapseDuplicates, 
	       "/Recoll/prefs/reslist/collapseDuplicates", Bool, false);
    SETTING_RW(prefs.showResultsAsTable, 
	       "/Recoll/prefs/showResultsAsTable", Bool, false);
    SETTING_RW(prefs.maxhltextmbs, "/Recoll/prefs/preview/maxhltextmbs", Int, 3);

    // The default is true because I find it more often useful to keep
    // indentation than to fold lines. Mileage may vary. 
    SETTING_RW(prefs.previewPlainPre, 
	       "/Recoll/prefs/preview/plainPre", Bool, true);
    SETTING_RW(prefs.qtermcolor, "/Recoll/prefs/qtermcolor", String, "blue");
    if (!writing && prefs.qtermcolor == "")
	prefs.qtermcolor = "blue";

    // Abstract snippet separator
    SETTING_RW(prefs.abssep, "/Recoll/prefs/reslist/abssep", String,"&hellip;");
    if (!writing && prefs.abssep == "")
	prefs.abssep = "&hellip;";

    SETTING_RW(prefs.reslistfontfamily, "/Recoll/prefs/reslist/fontFamily", String,
	       "");
    SETTING_RW(prefs.reslistfontsize, "/Recoll/prefs/reslist/fontSize", Int, 
	       10);
    QString rlfDflt = QString::fromAscii(prefs.dfltResListFormat);
    SETTING_RW(prefs.reslistformat, "/Recoll/prefs/reslist/format", String, rlfDflt);
    prefs.creslistformat = (const char*)prefs.reslistformat.toUtf8();
    if (!writing) {
	// If the current value of the format is the default for the
	// previous version, replace it with the new default. We
	// should have a flag to say if it was changed instead
	if (!prefs.creslistformat.compare(v114reslistformat)) {
	    LOGDEB(("Replacing old default format\n"));
	    prefs.reslistformat = rlfDflt;
	    prefs.creslistformat = (const char*)prefs.reslistformat.toUtf8();
	}
    }
    SETTING_RW(prefs.queryStemLang, "/Recoll/prefs/query/stemLang", String,
	       "english");
    SETTING_RW(prefs.useDesktopOpen, 
	       "/Recoll/prefs/useDesktopOpen", Bool, true);

    SETTING_RW(prefs.keepSort, 
	       "/Recoll/prefs/keepSort", Bool, false);
    SETTING_RW(prefs.sortField, "/Recoll/prefs/sortField", String, "");
    SETTING_RW(prefs.sortActive, 
	       "/Recoll/prefs/sortActive", Bool, false);
    SETTING_RW(prefs.sortDesc, 
	       "/Recoll/prefs/query/sortDesc", Bool, 0);
    if (!writing) {
	// Handle transition from older prefs which did not store sortColumn
	// (Active always meant sort by date).
	if (prefs.sortActive && prefs.sortField.isNull())
	    prefs.sortField = "mtime";
    }

    SETTING_RW(prefs.queryBuildAbstract, 
	       "/Recoll/prefs/query/buildAbstract", Bool, true);
    SETTING_RW(prefs.queryReplaceAbstract, 
	       "/Recoll/prefs/query/replaceAbstract", Bool, false);
    SETTING_RW(prefs.syntAbsLen, "/Recoll/prefs/query/syntAbsLen", 
	       Int, 250);
    SETTING_RW(prefs.syntAbsCtx, "/Recoll/prefs/query/syntAbsCtx", 
	       Int, 4);
    SETTING_RW(prefs.autoSuffs, "/Recoll/prefs/query/autoSuffs", String, "");
    SETTING_RW(prefs.autoSuffsEnable, 
	       "/Recoll/prefs/query/autoSuffsEnable", Bool, false);

    SETTING_RW(prefs.termMatchType, "/Recoll/prefs/query/termMatchType", 
	       Int, 0);
    // This is not really the current program version, just a value to
    // be used in case we have incompatible changes one day
    SETTING_RW(prefs.rclVersion, "/Recoll/prefs/rclVersion", 
	       Int, 1009);

    // Ssearch combobox history list
    if (writing) {
	settings.setValue("/Recoll/prefs/query/ssearchHistory",
			    prefs.ssearchHistory);
    } else {
	prefs.ssearchHistory = 
	    settings.value("/Recoll/prefs/query/ssearchHistory").toStringList();
    }
    SETTING_RW(prefs.ssearchAutoPhrase, 
	       "/Recoll/prefs/query/ssearchAutoPhrase", Bool, false);

    // Ignored file types (advanced search)
    if (writing) {
	settings.setValue("/Recoll/prefs/query/asearchIgnFilTyps",
			    prefs.asearchIgnFilTyps);
    } else {
	prefs.asearchIgnFilTyps = 
	    settings.value("/Recoll/prefs/query/asearchIgnFilTyps").toStringList();
    }


    // Field list for the restable
    if (writing) {
	settings.setValue("/Recoll/prefs/query/restableFields",
			    prefs.restableFields);
    } else {
	prefs.restableFields = 
	    settings.value("/Recoll/prefs/query/restableFields").toStringList();
	if (prefs.restableFields.empty()) {
	    prefs.restableFields.push_back("date");
	    prefs.restableFields.push_back("title");
	    prefs.restableFields.push_back("filename");
	    prefs.restableFields.push_back("author");
	    prefs.restableFields.push_back("url");
	}
    }

    // restable col widths
    QString rtcw;
    if (writing) {
	for (vector<int>::iterator it = prefs.restableColWidths.begin();
	     it != prefs.restableColWidths.end(); it++) {
	    char buf[20];
	    sprintf(buf, "%d ", *it);
	    rtcw += QString::fromAscii(buf);
	}
    }
    SETTING_RW(rtcw, "/Recoll/prefs/query/restableWidths", String, 
	       "83 253 132 172 130 ");
    if (!writing) {
	vector<string> widths;
	stringToStrings((const char *)rtcw.toUtf8(), widths);
	for (vector<string>::iterator it = widths.begin(); 
	     it != widths.end(); it++) {
	    prefs.restableColWidths.push_back(atoi(it->c_str()));
	}
    }

    SETTING_RW(prefs.fileTypesByCats, "/Recoll/prefs/query/asearchFilTypByCat",
	       Bool, false);

    if (g_dynconf == 0) {
        // Happens
        return;
    }
    // The extra databases settings. These are stored as a list of
    // xapian directory names, encoded in base64 to avoid any
    // binary/charset conversion issues. There are 2 lists for all
    // known dbs and active (searched) ones.
    // When starting up, we also add from the RECOLL_EXTRA_DBS environment
    // variable.
    // This are stored inside the dynamic configuration file (aka: history), 
    // as they are likely to depend on RECOLL_CONFDIR.
    if (writing) {
	g_dynconf->eraseAll(allEdbsSk);
	for (list<string>::const_iterator it = prefs.allExtraDbs.begin();
	     it != prefs.allExtraDbs.end(); it++) {
	    g_dynconf->enterString(allEdbsSk, *it);
	}

	g_dynconf->eraseAll(actEdbsSk);
	for (list<string>::const_iterator it = prefs.activeExtraDbs.begin();
	     it != prefs.activeExtraDbs.end(); it++) {
	    g_dynconf->enterString(actEdbsSk, *it);

	}
    } else {
	prefs.allExtraDbs = g_dynconf->getStringList(allEdbsSk);
	const char *cp;
	if ((cp = getenv("RECOLL_EXTRA_DBS")) != 0) {
	    vector<string> dbl;
	    stringToTokens(cp, dbl, ":");
	    for (vector<string>::iterator dit = dbl.begin(); dit != dbl.end();
		 dit++) {
		string dbdir = path_canon(*dit);
		path_catslash(dbdir);
		if (std::find(prefs.allExtraDbs.begin(), 
			      prefs.allExtraDbs.end(), dbdir) != 
		    prefs.allExtraDbs.end())
		    continue;
		if (!Rcl::Db::testDbDir(dbdir)) {
		    LOGERR(("Not a xapian index: [%s]\n", dbdir.c_str()));
		    continue;
		}
		prefs.allExtraDbs.push_back(dbdir);
	    }
	}
	prefs.activeExtraDbs = g_dynconf->getStringList(actEdbsSk);
    }
#if 0
    {
	list<string>::const_iterator it;
	fprintf(stderr, "All extra Dbs:\n");
	for (it = prefs.allExtraDbs.begin(); 
	     it != prefs.allExtraDbs.end(); it++) {
	    fprintf(stderr, "    [%s]\n", it->c_str());
	}
	fprintf(stderr, "Active extra Dbs:\n");
	for (it = prefs.activeExtraDbs.begin(); 
	     it != prefs.activeExtraDbs.end(); it++) {
	    fprintf(stderr, "    [%s]\n", it->c_str());
	}
    }
#endif


    const string asbdSk = "asearchSbd";
    if (writing) {
	while (prefs.asearchSubdirHist.size() > 20)
	    prefs.asearchSubdirHist.pop_back();
	g_dynconf->eraseAll(asbdSk);
	for (QStringList::iterator it = prefs.asearchSubdirHist.begin();
	     it != prefs.asearchSubdirHist.end(); it++) {
	    g_dynconf->enterString(asbdSk, (const char *)((*it).toUtf8()));
	}
    } else {
	list<string> tl = g_dynconf->getStringList(asbdSk);
	for (list<string>::iterator it = tl.begin(); it != tl.end(); it++)
	    prefs.asearchSubdirHist.push_front(QString::fromUtf8(it->c_str()));
    }
}

QString myGetFileName(bool isdir, QString caption)
{
    LOGDEB1(("myFileDialog: isdir %d\n", isdir));
    QFileDialog dialog(0, caption);

    if (isdir) {
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setOptions(QFileDialog::ShowDirsOnly);
    } else {
	dialog.setFileMode(QFileDialog::AnyFile);
	dialog.setAcceptMode(QFileDialog::AcceptSave);
    }
    dialog.setViewMode(QFileDialog::List);
    QFlags<QDir::Filter> flags = QDir::NoDotAndDotDot | QDir::Hidden; 
    if (isdir)
	flags |= QDir::Dirs;
    else 
	flags |= QDir::Dirs | QDir::Files;
    dialog.setFilter(flags);


    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedFiles().value(0);
    }
    return QString();
}
