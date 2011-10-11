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
#ifndef _GUIUTILS_H_INCLUDED_
#define _GUIUTILS_H_INCLUDED_

#include <string>
#include <list>
#include <vector>

#include <qstring.h>
#include <qstringlist.h>

#include "dynconf.h"
extern RclDynConf *g_dynconf;

#ifndef NO_NAMESPACES
using std::string;
using std::list;
using std::vector;
#endif

/** Holder for preferences (gets saved to user Qt prefs) */
class PrefsPack {
 public:
    bool autoSearchOnWS;
    // Decide if we display the doc category filter control as a
    // toolbar+combobox or as a button group under simple search
    bool catgToolBar;
    int respagesize;
    int maxhltextmbs;
    QString reslistfontfamily;
    QString qtermcolor; // Color for query terms in reslist and preview
    int reslistfontsize;
    // Result list format string
    QString reslistformat;
    string  creslistformat;
    // Abstract snippet separator
    QString abssep;
    QString queryStemLang;
    int mainwidth;
    int mainheight;
    int pvwidth; // Preview window geom
    int pvheight;
    int ssearchTyp;
    // Use single app (default: xdg-open), instead of per-mime settings
    bool useDesktopOpen; 
    // Remember sort state between invocations ?
    bool keepSort;   
    QString sortField;
    bool sortActive; 
    bool sortDesc; 
    // Abstract preferences. Building abstracts can slow result display
    bool queryBuildAbstract;
    bool queryReplaceAbstract;
    bool startWithAdvSearchOpen;
    // Try to display html if it exists in the internfile stack.
    bool previewHtml;
    // Use <pre> tag to display highlighted text/plain inside html (else
    // we use <br> at end of lines, which lets textedit wrap lines).
    bool previewPlainPre; 
    bool collapseDuplicates;
    bool showResultsAsTable;

    // Extra query indexes. This are stored in the history file, not qt prefs
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
    bool        fileTypesByCats;
    // Words that are automatically turned to ext:xx specs in the query
    // language entry. 
    QString autoSuffs;
    bool    autoSuffsEnable;

    QStringList restableFields;
    vector<int> restableColWidths;

    // Synthetized abstract length and word context size
    int syntAbsLen;
    int syntAbsCtx;

    // Remembered term match mode
    int termMatchType;

    // Program version that wrote this. Not used for now, in prevision
    // of the case where we might need an incompatible change
    int rclVersion;

    // Advanced search window clause list state
    vector<int> advSearchClauses;

    // Default paragraph format for result list
    static const char *dfltResListFormat;

    PrefsPack() :
	respagesize(8), 
	reslistfontsize(10),
	ssearchTyp(0),
	queryBuildAbstract(true),
	queryReplaceAbstract(false),
	startWithAdvSearchOpen(false),
	termMatchType(0),
	rclVersion(1505)
    { }
};

/** Global preferences record */
extern PrefsPack prefs;

/** Read write settings from disk file */
extern void rwSettings(bool dowrite);

extern QString g_stringAllStem, g_stringNoStem;

/** Specialized version of the qt file dialog. Can't use getOpenFile()
   etc. cause they hide dot files... */
extern QString myGetFileName(bool isdir, QString caption = QString());

#endif /* _GUIUTILS_H_INCLUDED_ */
