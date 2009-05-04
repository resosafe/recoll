#ifndef lint
static char rcsid[] = "@(#$Id: guiutils.cpp,v 1.40 2008-11-24 15:23:12 dockes Exp $ (C) 2005 Jean-Francois Dockes";
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

#include <qsettings.h>
#include <qstringlist.h>

bool getStemLangs(list<string>& langs)
{
    string reason;
    if (!maybeOpenDb(reason)) {
	LOGERR(("getStemLangs: %s\n", reason.c_str()));
	return false;
    }
    langs = rcldb->getStemLangs();
    return true;
}

// The global preferences structure
PrefsPack prefs;

// Using the same macro to read/write a setting. insurance against typing 
// mistakes
#define SETTING_RW(var, nm, tp, def)			\
    if (writing) {					\
	settings.writeEntry(nm , var);			\
    } else {						\
	var = settings.read##tp##Entry(nm, def);	\
    }						

/** 
 * Saving and restoring user preferences. These are stored in a global
 * structure during program execution and saved to disk using the QT
 * settings mechanism
 */
void rwSettings(bool writing)
{
    LOGDEB1(("rwSettings: write %d\n", int(writing)));
#if QT_VERSION >= 0x040000
    QSettings settings("Recoll.org", "recoll");
#else
    QSettings settings;
    settings.setPath("Recoll.org", "Recoll", QSettings::User);
#endif
    SETTING_RW(prefs.mainwidth, "/Recoll/geometry/width", Num, 0);
    SETTING_RW(prefs.mainheight, "/Recoll/geometry/height", Num, 0);
    SETTING_RW(prefs.pvwidth, "/Recoll/geometry/pvwidth", Num, 0);
    SETTING_RW(prefs.pvheight, "/Recoll/geometry/pvheight", Num, 0);
    SETTING_RW(prefs.ssearchTyp, "/Recoll/prefs/simpleSearchTyp", Num, 1);
    SETTING_RW(prefs.startWithAdvSearchOpen, 
	       "/Recoll/prefs/startWithAdvSearchOpen", Bool, false);
    SETTING_RW(prefs.startWithSortToolOpen, 
	       "/Recoll/prefs/startWithSortToolOpen", Bool, false);
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
    SETTING_RW(advSearchClauses, "/Recoll/prefs/adv/clauseList", , ascdflt);
    if (!writing) {
	list<string> clauses;
	stringToStrings((const char *)advSearchClauses.utf8(), clauses);
	for (list<string>::iterator it = clauses.begin(); 
	     it != clauses.end(); it++) {
	    prefs.advSearchClauses.push_back(atoi(it->c_str()));
	}
    }

    SETTING_RW(prefs.autoSearchOnWS, "/Recoll/prefs/reslist/autoSearchOnWS", 
	       Bool, false);
    SETTING_RW(prefs.ssearchAutoPhrase, 
	       "/Recoll/prefs/ssearchAutoPhrase", Bool, false);
    SETTING_RW(prefs.respagesize, "/Recoll/prefs/reslist/pagelen", Num, 8);
    SETTING_RW(prefs.collapseDuplicates, 
	       "/Recoll/prefs/reslist/collapseDuplicates", Bool, false);
    SETTING_RW(prefs.maxhltextmbs, "/Recoll/prefs/preview/maxhltextmbs", Num, 3);
    SETTING_RW(prefs.qtermcolor, "/Recoll/prefs/qtermcolor", , "blue");
    if (!writing && prefs.qtermcolor == "")
	prefs.qtermcolor = "blue";

    SETTING_RW(prefs.reslistfontfamily, "/Recoll/prefs/reslist/fontFamily", ,
	       "");
    SETTING_RW(prefs.reslistfontsize, "/Recoll/prefs/reslist/fontSize", Num, 
	       10);
    QString rlfDflt = 
	QString::fromAscii(prefs.getDfltResListFormat());
    SETTING_RW(prefs.reslistformat, "/Recoll/prefs/reslist/format", , rlfDflt);
    if (prefs.reslistformat == 
	QString::fromAscii(prefs.getV18DfltResListFormat()) || 
	prefs.reslistformat.stripWhiteSpace().isEmpty())
	prefs.reslistformat = rlfDflt;
    prefs.creslistformat = (const char*)prefs.reslistformat.utf8();

    SETTING_RW(prefs.queryStemLang, "/Recoll/prefs/query/stemLang", ,
	       "english");
    SETTING_RW(prefs.useDesktopOpen, 
	       "/Recoll/prefs/useDesktopOpen", Bool, false);
    SETTING_RW(prefs.keepSort, 
	       "/Recoll/prefs/keepSort", Bool, false);
    SETTING_RW(prefs.sortActive, 
	       "/Recoll/prefs/sortActive", Bool, false);
    SETTING_RW(prefs.queryBuildAbstract, 
	       "/Recoll/prefs/query/buildAbstract", Bool, true);
    SETTING_RW(prefs.queryReplaceAbstract, 
	       "/Recoll/prefs/query/replaceAbstract", Bool, false);
    SETTING_RW(prefs.syntAbsLen, "/Recoll/prefs/query/syntAbsLen", 
	       Num, 250);
    SETTING_RW(prefs.syntAbsCtx, "/Recoll/prefs/query/syntAbsCtx", 
	       Num, 4);

    SETTING_RW(prefs.sortDepth, "/Recoll/prefs/query/sortDepth",
	       Num, 100);
    SETTING_RW(prefs.sortSpec, "/Recoll/prefs/query/sortSpec", 
	       Num, 0);
    SETTING_RW(prefs.termMatchType, "/Recoll/prefs/query/termMatchType", 
	       Num, 0);
    SETTING_RW(prefs.rclVersion, "/Recoll/prefs/rclVersion", 
	       Num, 1009);

    // Ssearch combobox history list
    if (writing) {
	settings.writeEntry("/Recoll/prefs/query/ssearchHistory",
			    prefs.ssearchHistory);
    } else {
	prefs.ssearchHistory = 
	    settings.readListEntry("/Recoll/prefs/query/ssearchHistory");
    }
    SETTING_RW(prefs.ssearchAutoPhrase, 
	       "/Recoll/prefs/query/ssearchAutoPhrase", Bool, false);

    // Ignored file types (advanced search)
    if (writing) {
	settings.writeEntry("/Recoll/prefs/query/asearchIgnFilTyps",
			    prefs.asearchIgnFilTyps);
    } else {
	prefs.asearchIgnFilTyps = 
	    settings.readListEntry("/Recoll/prefs/query/asearchIgnFilTyps");
    }
    SETTING_RW(prefs.fileTypesByCats, "/Recoll/prefs/query/asearchFilTypByCat",
	       Bool, false);

    // The extra databases settings. These are stored as a list of
    // xapian directory names, encoded in base64 to avoid any
    // binary/charset conversion issues. There are 2 lists for all
    // known dbs and active (searched) ones.
    // When starting up, we also add from the RECOLL_EXTRA_DBS environment
    // variable.
    // This are stored inside the dynamic configuration file (aka: history), 
    // as they are likely to depend on RECOLL_CONFDIR.
    const string allEdbsSk = "allExtDbs";
    const string actEdbsSk = "actExtDbs";
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
	    list<string> dbl;
	    stringToTokens(cp, dbl, ":");
	    for (list<string>::iterator dit = dbl.begin(); dit != dbl.end();
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
	    g_dynconf->enterString(asbdSk, (const char *)((*it).utf8()));
	}
    } else {
	list<string> tl = g_dynconf->getStringList(asbdSk);
	for (list<string>::iterator it = tl.begin(); it != tl.end(); it++)
	    prefs.asearchSubdirHist.push_front(QString::fromUtf8(it->c_str()));
    }

}
