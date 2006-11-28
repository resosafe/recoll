#ifndef lint
static char rcsid[] = "@(#$Id: guiutils.cpp,v 1.25.2.2 2006-11-28 06:50:51 dockes Exp $ (C) 2005 Jean-Francois Dockes";
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

static const char *htmlbrowserlist = 
    "opera konqueror firefox mozilla netscape";

/** 
 * Search for and launch an html browser for the documentation. If the
 * user has set a preference, we use it directly instead of guessing.
 */
bool startHelpBrowser(const string &iurl)
{
    string url;
    if (iurl.empty()) {
	url = path_cat(recoll_datadir, "doc");
	url = path_cat(url, "usermanual.html");
	url = string("file://") + url;
    } else
	url = iurl;

    // If the user set a preference with an absolute path then things are
    // simple
    if (!prefs.htmlBrowser.isEmpty() && prefs.htmlBrowser.find('/') != -1) {
	if (access(prefs.htmlBrowser.ascii(), X_OK) != 0) {
	    LOGERR(("startHelpBrowser: %s not found or not executable\n",
		    prefs.htmlBrowser.ascii()));
	}
	string cmd = string(prefs.htmlBrowser.ascii()) + " " + url + "&";
	if (system(cmd.c_str()) == 0)
	    return true;
	else 
	    return false;
    }

    string searched;
    if (prefs.htmlBrowser.isEmpty()) {
	searched = htmlbrowserlist;
    } else {
	searched = prefs.htmlBrowser.ascii();
    }
    list<string> blist;
    stringToTokens(searched, blist, " ");

    const char *path = getenv("PATH");
    if (path == 0)
	path = "/bin:/usr/bin:/usr/bin/X11:/usr/X11R6/bin:/usr/local/bin";

    list<string> pathl;
    stringToTokens(path, pathl, ":");
    
    // For each browser name, search path and exec/stop if found
    for (list<string>::const_iterator bit = blist.begin(); 
	 bit != blist.end(); bit++) {
	for (list<string>::const_iterator pit = pathl.begin(); 
	     pit != pathl.end(); pit++) {
	    string exefile = path_cat(*pit, *bit);
	    LOGDEB1(("startHelpBrowser: trying %s\n", exefile.c_str()));
	    if (access(exefile.c_str(), X_OK) == 0) {
		string cmd = exefile + " " + url + "&";
		if (system(cmd.c_str()) == 0) {
		    return true;
		}
	    }
	}
    }

    LOGERR(("startHelpBrowser: no html browser found. Looked for: %s\n", 
	    searched.c_str()));
    return false;
}

// Printable url: either utf-8 if transcoding succeeds, or url-encoded
bool printableUrl(const string &in, string &out)
{
    string fcharset = rclconfig->getDefCharset(true);
    int ecnt = 0;
    if (!transcode(in, out, fcharset, "UTF-8", &ecnt) || ecnt) {
	out = url_encode(in, 7);
    }
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
    SETTING_RW(prefs.ssearchTyp, "/Recoll/prefs/simpleSearchTyp", Num, 0);
    SETTING_RW(prefs.htmlBrowser, "/Recoll/prefs/htmlBrowser", , "");
    SETTING_RW(prefs.startWithAdvSearchOpen, 
	       "/Recoll/prefs/startWithAdvSearchOpen", Bool, false);
    SETTING_RW(prefs.startWithSortToolOpen, 
	       "/Recoll/prefs/startWithSortToolOpen", Bool, false);

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
	stringToStrings(advSearchClauses, clauses);
	for (list<string>::iterator it = clauses.begin(); 
	     it != clauses.end(); it++) {
	    prefs.advSearchClauses.push_back(atoi(it->c_str()));
	}
    }

    SETTING_RW(prefs.showicons, "/Recoll/prefs/reslist/showicons", Bool, true);
    SETTING_RW(prefs.autoSearchOnWS, "/Recoll/prefs/reslist/autoSearchOnWS", 
	       Bool, false);
    SETTING_RW(prefs.ssearchAutoPhrase, 
	       "/Recoll/prefs/ssearchAutoPhrase", Bool, false);
    SETTING_RW(prefs.respagesize, "/Recoll/prefs/reslist/pagelen", Num, 8);
    SETTING_RW(prefs.reslistfontfamily, "/Recoll/prefs/reslist/fontFamily", ,
	       "Helvetica");
    SETTING_RW(prefs.reslistfontsize, "/Recoll/prefs/reslist/fontSize", Num, 
	       10);
    QString rlfDflt = 
	QString::fromAscii("%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
			   "%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
			   "%A %K");
    SETTING_RW(prefs.reslistformat, "/Recoll/prefs/reslist/format", , rlfDflt);
    if (prefs.reslistformat.stripWhiteSpace().isEmpty())
	prefs.reslistformat = rlfDflt;
    SETTING_RW(prefs.queryStemLang, "/Recoll/prefs/query/stemLang", ,
	       "english");
    SETTING_RW(prefs.queryBuildAbstract, 
	       "/Recoll/prefs/query/buildAbstract", Bool, true);
    SETTING_RW(prefs.queryReplaceAbstract, 
	       "/Recoll/prefs/query/replaceAbstract", Bool, false);
    SETTING_RW(prefs.syntAbsLen, "/Recoll/prefs/query/syntAbsLen", 
	       Num, 250);
    SETTING_RW(prefs.syntAbsCtx, "/Recoll/prefs/query/syntAbsCtx", 
	       Num, 4);

    SETTING_RW(prefs.sortWidth, "/Recoll/prefs/query/sortWidth", 
	       Num, 100);
    SETTING_RW(prefs.sortSpec, "/Recoll/prefs/query/sortSpec", 
	       Num, 0);
    SETTING_RW(prefs.termMatchType, "/Recoll/prefs/query/termMatchType", 
	       Num, 0);

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

    // Ssearch combobox history list
    if (writing) {
	settings.writeEntry("/Recoll/prefs/query/asearchIgnFilTyps",
			    prefs.asearchIgnFilTyps);
    } else {
	prefs.asearchIgnFilTyps = 
	    settings.readListEntry("/Recoll/prefs/query/asearchIgnFilTyps");
    }

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
