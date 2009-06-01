#ifndef lint
static char rcsid[] = "@(#$Id: main.cpp,v 1.73 2008-12-17 08:01:40 dockes Exp $ (C) 2005 J.F.Dockes";
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

#include <cstdlib>

#include "autoconfig.h"

#include <unistd.h>

//#define WITH_KDE
#ifdef WITH_KDE
#include <kapplication.h>
#include <kmainwindow.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <klocale.h>
#else
#include <qapplication.h>
#include <qtranslator.h>
#endif

#include <qtextcodec.h> 
#include <qthread.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qcheckbox.h>
#include <qcombobox.h>

#include "rcldb.h"
#include "rclconfig.h"
#include "pathut.h"
#include "recoll.h"
#include "smallut.h"
#include "rclinit.h"
#include "debuglog.h"
#ifdef WITH_KDE
#include "rclversion.h"
#endif
#include "rclmain_w.h"
#include "ssearch_w.h"
#include "guiutils.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif
#include "smallut.h"
#include "recollq.h"

#ifdef WITH_KDE
static const char description[] =
    I18N_NOOP("A KDE fulltext search application");

static KCmdLineOptions options[] =
{
//    { "+[URL]", I18N_NOOP( "Document to open" ), 0 },
    KCmdLineLastOption
};
#endif

RclConfig *rclconfig;
Rcl::Db *rcldb;
#ifdef RCL_USE_ASPELL
Aspell *aspell;
#endif

RclConfig* RclConfig::getMainConfig()
{
    return rclconfig;
}

RclHistory *g_dynconf;
int recollNeedsExit;
int startIndexingAfterConfig;
RclMain *mainWindow;
static string recollsharedir;

void startManual(const string& helpindex)
{
    if (mainWindow)
	mainWindow->startManual(helpindex);
}

bool maybeOpenDb(string &reason, bool force)
{
    if (!rcldb) {
	reason = "Internal error: db not created";
	return false;
    }

    if (force)
	rcldb->close();
    rcldb->rmQueryDb("");
    for (list<string>::const_iterator it = prefs.activeExtraDbs.begin();
	 it != prefs.activeExtraDbs.end(); it++) {
	LOGDEB(("main: adding [%s]\n", it->c_str()));
	rcldb->addQueryDb(*it);
    }
    if (!rcldb->isopen() && !rcldb->open(Rcl::Db::DbRO)) {
	reason = "Could not open database in " + 
	    rclconfig->getDbDir() + " wait for indexing to complete?";
	return false;
    }
    rcldb->setAbstractParams(-1, prefs.syntAbsLen, prefs.syntAbsCtx);
    return true;
}

static void recollCleanup()
{
    LOGDEB(("recollCleanup: writing settings\n"));
    rwSettings(true);
    LOGDEB2(("recollCleanup: stopping idx thread\n"));
    stop_idxthread();
    LOGDEB2(("recollCleanup: closing database\n"));
    deleteZ(rcldb);
    deleteZ(rclconfig);
#ifdef RCL_USE_ASPELL
    deleteZ(aspell);
#endif
    LOGDEB2(("recollCleanup: done\n"));
}

static void sigcleanup(int)
{
    fprintf(stderr, "sigcleanup called\n");
    // Cant call exit from here, because the atexit cleanup does some
    // thread stuff that we can't do from signal context.
    // Just set a flag and let the watchdog timer do the work
    recollNeedsExit = 1;
}

extern void qInitImages_recoll();

static const char *thisprog;

// ATTENTION A LA COMPATIBILITE AVEC LES OPTIONS DE recollq
static int    op_flags;
#define OPT_h     0x4 
#define OPT_c     0x20
#define OPT_q     0x40
#define OPT_o     0x80
#define OPT_l     0x100
#define OPT_f     0x200
#define OPT_a     0x400
#define OPT_t     0x800

static const char usage [] =
"\n"
"recoll [-h] [-c <configdir>] [-q options]\n"
"  -h : Print help and exit\n"
"  -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
"  [-o|l|f|a] [-t] -q 'query' : search query to be executed as if entered\n"
"      into simple search. The default is to interpret the argument as a \n"
"      query language string (but see modifier options)\n"
"      In most cases, the query string should be quoted with single-quotes to\n"
"      avoid shell interpretation\n"
"     -a : the query will be interpreted as an AND query.\n"
"     -o : the query will be interpreted as an OR query.\n"
"     -f : the query will be interpreted as a filename search\n"
"     -l : the query will be interpreted as a query language string (default)\n"
"     -t : terminal display: no gui. Results go to stdout. MUST be given\n"
"          explicitely as -t (not ie, -at), and -q <query> MUST\n"
"          be last on the command line if this is used.\n"
;
static void
Usage(void)
{
    FILE *fp = (op_flags & OPT_h) ? stdout : stderr;
    fprintf(fp, "%s: Usage: %s", thisprog, usage);
    exit((op_flags & OPT_h)==0);
}

int main(int argc, char **argv)
{
    for (int i = 0; i < argc; i++) {
	if (!strcmp(argv[i], "-t")) {
	    exit(recollq(&rclconfig, argc, argv));
	}
    }

#ifdef WITH_KDE
    KAboutData about("recoll", I18N_NOOP("Recoll"), rclversion, description,
                     KAboutData::License_GPL, "(C) 2006 Jean-Francois Dockes", 0, 0, "jean-francois.dockes@wanadoo.fr");
    about.addAuthor( "Jean-Francois Dockes", 0, 
		     "jean-francois.dockes@wanadoo.fr" );
    KCmdLineArgs::init(argc, argv, &about);
    KCmdLineArgs::addCmdLineOptions( options );
    KApplication app;
#else
    QApplication app(argc, argv);
#endif

    //    fprintf(stderr, "Application created\n");
    string a_config;
    string question;

    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'a': op_flags |= OPT_a; break;
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
	    case 'f': op_flags |= OPT_f; break;
	    case 'h': op_flags |= OPT_h; Usage();break;
	    case 'l': op_flags |= OPT_l; break;
	    case 'o': op_flags |= OPT_o; break;
	    case 'q':	op_flags |= OPT_q; if (argc < 2)  Usage();
		question = *(++argv);
		argc--; goto b1;
	    case 't': op_flags |= OPT_t; break;
	    }
    b1: argc--; argv++;
    }

    // If -q was given, all remaining non-option args are concatenated
    // to the query. This is for the common case recoll -q x y z to
    // avoid needing quoting "x y z"
    if (op_flags & OPT_q)
	while (argc--) {
	    question += " ";
	    question += *argv++;
	}

    // Translation file for Qt
    QTranslator qt( 0 );
    qt.load( QString( "qt_" ) + QTextCodec::locale(), "." );
    app.installTranslator( &qt );

    string reason;
    rclconfig = recollinit(recollCleanup, sigcleanup, reason, &a_config);
    if (!rclconfig || !rclconfig->ok()) {
	QString msg = "Configuration problem: ";
	msg += QString::fromUtf8(reason.c_str());
	QMessageBox::critical(0, "Recoll",  msg);
	exit(1);
    }
    //    fprintf(stderr, "recollinit done\n");

    // Translations for Recoll
    string translatdir = path_cat(rclconfig->getDatadir(), "translations");
    QTranslator translator( 0 );
    // QTextCodec::locale() returns $LANG
    translator.load( QString("recoll_") + QTextCodec::locale(), 
		     translatdir.c_str() );
    app.installTranslator( &translator );

    //    fprintf(stderr, "Translations installed\n");

#ifdef RCL_USE_ASPELL
    aspell = new Aspell(rclconfig);
    aspell->init(reason);
    if (!aspell || !aspell->ok()) {
	LOGDEB(("Aspell speller creation failed %s\n", reason.c_str()));
	aspell = 0;
    }
#endif

    string historyfile = path_cat(rclconfig->getConfDir(), "history");
    g_dynconf = new RclHistory(historyfile);
    if (!g_dynconf || !g_dynconf->ok()) {
	QString msg = app.translate("Main", "Configuration problem (dynconf");
	QMessageBox::critical(0, "Recoll",  msg);
	exit(1);
    }

    //    fprintf(stderr, "History done\n");
    rwSettings(false);
    //    fprintf(stderr, "Settings done\n");


    // Create main window and set its size to previous session's
#ifdef WITH_KDE
#if 0
    if (app.isRestored()) {
	RESTORE(RclMain);
    } else {
#endif
        // no session.. just start up normally
        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
        /// @todo do something with the command line args here
	qInitImages_recoll();
        mainWindow = new RclMain;
        app.setMainWidget( mainWindow );
        mainWindow->show();

        args->clear();

	//    }
#else
	RclMain w;
	mainWindow = &w;
#endif

    if (prefs.mainwidth > 100) {
	QSize s(prefs.mainwidth, prefs.mainheight);
	mainWindow->resize(s);
    }

    mainWindow->sSearch->searchTypCMB->setCurrentItem(prefs.ssearchTyp);
    mainWindow->sSearch->searchTypeChanged(prefs.ssearchTyp);
    string dbdir = rclconfig->getDbDir();
    if (dbdir.empty()) {
	// Note: this will have to be replaced by a call to a
	// configuration buildin dialog for initial configuration
	QMessageBox::critical(0, "Recoll",
			      app.translate("Main", 
					  "No db directory in configuration"));
	exit(1);
    }

    rcldb = new Rcl::Db(rclconfig);

    bool needindexconfig = false;
    if (!maybeOpenDb(reason)) {

	switch (QMessageBox::
#if (QT_VERSION >= 0x030200)
		question
#else
		information
#endif
		(0, "Recoll",
		 app.translate("Main", "Could not open database in ") +
		 QString::fromLocal8Bit(dbdir.c_str()) +
		 app.translate("Main", 
			       ".\n"
			       "Click Cancel if you want to edit the configuration file before indexation starts, or Ok to let it proceed."),
		 "Ok", "Cancel", 0,   0)) {

	case 0: // Ok: indexing is going to start.
	    start_indexing(true);
	    break;

	case 1: // Cancel
	    needindexconfig = true;
	    break;
	}
    }

    mainWindow->show();
    if (needindexconfig) {
	startIndexingAfterConfig = 1;
	mainWindow->showIndexConfig();
    } else {
	if (prefs.startWithAdvSearchOpen)
	    mainWindow->showAdvSearchDialog();
	if (prefs.startWithSortToolOpen)
	    mainWindow->showSortDialog();
    }

    // Connect exit handlers etc.. Beware, apparently this must come
    // after mainWindow->show() , else the QMessageBox above never
    // returns.
    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    QTimer *timer = new QTimer(&app);
    mainWindow->connect(timer, SIGNAL(timeout()), 
			mainWindow, SLOT(periodic100()));
    timer->start(100);
    app.connect(&app, SIGNAL(aboutToQuit()), mainWindow, SLOT(close()));

    start_idxthread(*rclconfig);
    if (op_flags & OPT_q) {
	SSearch::SSearchType stype;
	if (op_flags & OPT_o) {
	    stype = SSearch::SST_ANY;
	} else if (op_flags & OPT_f) {
	    stype = SSearch::SST_FNM;
	} else if (op_flags & OPT_a) {
	    stype = SSearch::SST_ALL;
	} else {
	    stype = SSearch::SST_LANG;
	}
	mainWindow->sSearch->searchTypCMB->setCurrentItem(int(stype));
	mainWindow->
	    sSearch->setSearchString(QString::fromLocal8Bit(question.c_str()));
	// The 200 ms are a hack to jump over the first db close by 
	// periodic100()... 
	QTimer::singleShot(200, mainWindow->sSearch, SLOT(startSimpleSearch()));
    }
    //    fprintf(stderr, "Go\n");
    // Let's go
    return app.exec();
}

