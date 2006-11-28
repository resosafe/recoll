#ifndef lint
static char rcsid[] = "@(#$Id: main.cpp,v 1.52.2.1 2006-11-28 06:50:51 dockes Exp $ (C) 2005 J.F.Dockes";
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
#include "guiutils.h"
#ifdef RCL_USE_ASPELL
#include "rclaspell.h"
#endif

#ifdef WITH_KDE
static const char description[] =
    I18N_NOOP("A KDE fulltext search application");

static KCmdLineOptions options[] =
{
//    { "+[URL]", I18N_NOOP( "Document to open" ), 0 },
    KCmdLineLastOption
};
#endif


const string recoll_datadir = RECOLL_DATADIR;

RclConfig *rclconfig;
Rcl::Db *rcldb;
#ifdef RCL_USE_ASPELL
Aspell *aspell;
#endif

RclHistory *g_dynconf;
int recollNeedsExit;
static string dbdir;
static RclMain *mainWindow;
static string recollsharedir;

bool maybeOpenDb(string &reason, bool force)
{
    if (!rcldb) {
	reason = "Internal error: db not created";
	return false;
    }

    int qopts = Rcl::Db::QO_NONE;
    if (prefs.queryBuildAbstract) {
	qopts |= Rcl::Db::QO_BUILD_ABSTRACT;
	if (prefs.queryReplaceAbstract)
	    qopts |= Rcl::Db::QO_REPLACE_ABSTRACT;
    }
    if (prefs.queryStemLang.length() > 0)
	qopts |= Rcl::Db::QO_STEM;
    if (force)
	rcldb->close();
    rcldb->rmQueryDb("");
    for (list<string>::const_iterator it = prefs.activeExtraDbs.begin();
	 it != prefs.activeExtraDbs.end(); it++) {
	LOGDEB(("main: adding [%s]\n", it->c_str()));
	rcldb->addQueryDb(*it);
    }
    if (!rcldb->isopen() && !rcldb->open(dbdir, Rcl::Db::DbRO, qopts)) {
	reason = "Could not open database in " + 
	    dbdir + " wait for indexing to complete?";
	return false;
    }
    rcldb->setAbstractParams(-1, prefs.syntAbsLen, prefs.syntAbsCtx);
    return true;
}

static void recollCleanup()
{
    LOGDEB(("recollCleanup: writing settings\n"));
    if (mainWindow) {
	prefs.mainwidth = mainWindow->width();
	prefs.mainheight = mainWindow->height();
	prefs.ssearchTyp = mainWindow->sSearch->searchTypCMB->currentItem();
    }
    rwSettings(true);
    LOGDEB2(("recollCleanup: stopping idx thread\n"));
    stop_idxthread();
    LOGDEB2(("recollCleanup: closing database\n"));
    delete rcldb;
    rcldb = 0;
    delete rclconfig;
    rclconfig = 0;
#ifdef RCL_USE_ASPELL
    delete aspell;
    aspell = 0;
#endif
    LOGDEB2(("recollCleanup: done\n"));
}

static void sigcleanup(int)
{
    // fprintf(stderr, "sigcleanup\n");
    // Cant call exit from here, because the atexit cleanup does some
    // thread stuff that we can't do from signal context.
    // Just set a flag and let the watchdog timer do the work
    recollNeedsExit = 1;
}

extern void qInitImages_recoll();

static const char *thisprog;
static int    op_flags;
#define OPT_MOINS 0x1
#define OPT_h     0x4 
#define OPT_c     0x20
static const char usage [] =
"\n"
"recoll [-h] [-c <configdir>]\n"
"   -h : Print help and exit\n"
"   -c <configdir> : specify config directory, overriding $RECOLL_CONFDIR\n"
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
    thisprog = argv[0];
    argc--; argv++;

    while (argc > 0 && **argv == '-') {
	(*argv)++;
	if (!(**argv))
	    Usage();
	while (**argv)
	    switch (*(*argv)++) {
	    case 'c':	op_flags |= OPT_c; if (argc < 2)  Usage();
		a_config = *(++argv);
		argc--; goto b1;
	    case 'h': op_flags |= OPT_h; Usage();break;
	    }
    b1: argc--; argv++;
    }

    // Translation file for Qt
    QTranslator qt( 0 );
    qt.load( QString( "qt_" ) + QTextCodec::locale(), "." );
    app.installTranslator( &qt );

    // Translations for Recoll
    string translatdir = path_cat(recoll_datadir, "translations");
    QTranslator translator( 0 );
    // QTextCodec::locale() returns $LANG
    translator.load( QString("recoll_") + QTextCodec::locale(), 
		     translatdir.c_str() );
    app.installTranslator( &translator );

    //    fprintf(stderr, "Translations installed\n");

    string reason;
    rclconfig = recollinit(recollCleanup, sigcleanup, reason, &a_config);
    if (!rclconfig || !rclconfig->ok()) {
	QString msg = app.translate("Main", "Configuration problem: ");
	msg += QString::fromUtf8(reason.c_str());
	QMessageBox::critical(0, "Recoll",  msg);
	exit(1);
    }
    //    fprintf(stderr, "recollinit done\n");

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
    dbdir = rclconfig->getDbDir();
    if (dbdir.empty()) {
	// Note: this will have to be replaced by a call to a
	// configuration buildin dialog for initial configuration
	QMessageBox::critical(0, "Recoll",
			      app.translate("Main", 
					  "No db directory in configuration"));
	exit(1);
    }

    rcldb = new Rcl::Db;

    // Connect exit handlers etc..
    app.connect(&app, SIGNAL(lastWindowClosed()), &app, SLOT(quit()));
    QTimer *timer = new QTimer(&app);
    mainWindow->connect(timer, SIGNAL(timeout()), 
			mainWindow, SLOT(periodic100()));
    timer->start(100);

    if (!maybeOpenDb(reason)) {
	startindexing = 1;

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
		 "Ok",
		 "Cancel", 0, 0, 1 )) {
	case 0: // Ok
	    break;
	case 1: // Cancel
	    exit(0);
	}
    }
    //    fprintf(stderr, "Db opened\n");

    mainWindow->show();
    if (prefs.startWithAdvSearchOpen)
	mainWindow->showAdvSearchDialog();
    if (prefs.startWithSortToolOpen)
	mainWindow->showSortDialog();

    start_idxthread(*rclconfig);

    //    fprintf(stderr, "Go\n");
    // Let's go
    return app.exec();
}

