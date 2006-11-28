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
#ifndef _GUIUTILS_H_INCLUDED_
#define _GUIUTILS_H_INCLUDED_
/* 
 * @(#$Id: guiutils.h,v 1.17.2.1 2006-11-28 06:50:51 dockes Exp $  (C) 2005 Jean-Francois Dockes 
 *                         jean-francois.dockes@wanadoo.fr
 *
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

#include <string>
#include <list>
#include <vector>

#include <qstring.h>
#include <qstringlist.h>

#ifndef NO_NAMESPACES
using std::string;
using std::list;
using std::vector;
#endif

/** Retrieve configured stemming languages */
bool getStemLangs(list<string>& langs);

/** Start a browser on the help document */
extern bool startHelpBrowser(const string& url = "");

/** Try to make an utf8 printable url */
extern bool printableUrl(const string &in, string &out);

/** Holder for preferences (gets saved to user Qt prefs) */
class PrefsPack {
 public:
    bool showicons;
    bool autoSearchOnWS;
    int respagesize;
    QString reslistfontfamily;
    int reslistfontsize;
    // Result list format string
    QString reslistformat;
    QString queryStemLang;
    int mainwidth;
    int mainheight;
    int pvwidth; // Preview window geom
    int pvheight;
    int ssearchTyp;
    QString htmlBrowser;
    bool queryBuildAbstract;
    bool queryReplaceAbstract;
    bool startWithAdvSearchOpen;
    bool startWithSortToolOpen;
    // Extra query indexes. This are encoded to base64 before storing
    // to the qt settings file to avoid any bin string/ charset conv issues
    list<string> allExtraDbs;
    list<string> activeExtraDbs;
    // Advanced search subdir restriction: we don't activate the last value
    // but just remember previously entered values
    QStringList asearchSubdirHist;
    // Textual history of simple searches (this is just the combobox list)
    QStringList ssearchHistory;
    // Make phrase out of search terms and add to search in simple search
    bool ssearchAutoPhrase;
    // Ignored file types in adv search (startup default)
    QStringList asearchIgnFilTyps;
    
    // Synthetized abstract length and word context size
    int syntAbsLen;
    int syntAbsCtx;

    // Sort specs (sort_w.cpp knows how to deal with the values
    int sortWidth;
    int sortSpec;

    // Remembered term match mode
    int termMatchType;

    // Advanced search window clause list state
    vector<int> advSearchClauses;

    PrefsPack() :
	showicons(true), 
	respagesize(8), 
	reslistfontsize(10),
	ssearchTyp(0),
	queryBuildAbstract(true),
	queryReplaceAbstract(false),
	startWithAdvSearchOpen(false),
	    startWithSortToolOpen(false),
	    termMatchType(0)
	    {
	    }
};

/** Global preferences record */
extern PrefsPack prefs;

/** Read write settings from disk file */
extern void rwSettings(bool dowrite);

#endif /* _GUIUTILS_H_INCLUDED_ */
