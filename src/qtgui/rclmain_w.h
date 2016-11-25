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
#ifndef RCLMAIN_W_H
#define RCLMAIN_W_H
#include "autoconfig.h"

#include <qvariant.h>
#include <qmainwindow.h>
#include <QFileSystemWatcher>

#include "sortseq.h"
#include "preview_w.h"
#include "recoll.h"
#include "advsearch_w.h"
#include "uiprefs_w.h"
#include "rcldb.h"
#include "searchdata.h"
#include "spell_w.h"
#include MEMORY_INCLUDE
#include "pathut.h"
#include "guiutils.h"

class IdxSchedW;
class ExecCmd;
class Preview;
class ResTable;
class CronToolW;
class RTIToolW;
class FragButs;
class SpecIdxW;
class WebcacheEdit;

#include "ui_rclmain.h"

namespace confgui {
class ConfIndexW;
}

using confgui::ConfIndexW;

class RclTrayIcon;

class RclMain : public QMainWindow, public Ui::RclMainBase {
    Q_OBJECT;

public:
    RclMain(QWidget * parent = 0)
        : QMainWindow(parent),
          curPreview(0),
          asearchform(0),
          uiprefs(0),
          indexConfig(0),
          indexSched(0),
          cronTool(0),
          rtiTool(0),
          spellform(0),
          fragbuts(0),
          specidx(0),
          periodictimer(0),
          webcache(0),
          restable(0),
          displayingTable(0),
          m_idNoStem(0),
          m_idAllStem(0),
          m_toolsTB(0), m_resTB(0),
          m_filtFRM(0), m_filtCMB(0), m_filtBGRP(0), m_filtMN(0),
          m_idxproc(0),
          m_idxkilled(false),
          m_catgbutvecidx(0),
          m_sortspecnochange(false),
          m_indexerState(IXST_UNKNOWN),
          m_queryActive(false),
          m_firstIndexing(false),
          m_searchIsSimple(false),
          m_pidfile(0) {
        setupUi(this);
        init();
    }
    ~RclMain() {}

    QString getQueryDescription();

    /** This is only called from main() to set an URL to be displayed (using
    recoll as a doc extracter for embedded docs */
    virtual void setUrlToView(const QString& u) {
        m_urltoview = u;
    }
    /** Same usage: actually display the current urltoview */
    virtual void viewUrl();

    bool lastSearchSimple() {
        return m_searchIsSimple;
    }

    // Takes copies of the args instead of refs. Lazy and safe.
    void newDupsW(const Rcl::Doc doc, const std::vector<Rcl::Doc> dups);

    enum  IndexerState {IXST_UNKNOWN, IXST_NOTRUNNING,
                        IXST_RUNNINGMINE, IXST_RUNNINGNOTMINE};

    IndexerState indexerState() const {
        return m_indexerState;
    }

                            
public slots:
    virtual void fileExit();
    virtual void periodic100();
    virtual void toggleIndexing();
    virtual void rebuildIndex();
    virtual void specialIndex();
    virtual void startSearch(STD_SHARED_PTR<Rcl::SearchData> sdata,
                             bool issimple);
    virtual void previewClosed(Preview *w);
    virtual void showAdvSearchDialog();
    virtual void showSpellDialog();
    virtual void showWebcacheDialog();
    virtual void showIndexStatistics();
    virtual void showFragButs();
    virtual void showSpecIdx();
    virtual void showAboutDialog();
    virtual void showMissingHelpers();
    virtual void showActiveTypes();
    virtual void startManual();
    virtual void startManual(const string&);
    virtual void showDocHistory();
    virtual void showExtIdxDialog();
    virtual void setSynEnabled(bool);
    virtual void showUIPrefs();
    virtual void showIndexConfig();
    virtual void execIndexConfig();
    virtual void showCronTool();
    virtual void execCronTool();
    virtual void showRTITool();
    virtual void execRTITool();
    virtual void showIndexSched();
    virtual void execIndexSched();
    virtual void setUIPrefs();
    virtual void enableNextPage(bool);
    virtual void enablePrevPage(bool);
    virtual void docExpand(Rcl::Doc);
    virtual void showSubDocs(Rcl::Doc);
    virtual void showSnippets(Rcl::Doc);
    virtual void startPreview(int docnum, Rcl::Doc doc, int keymods);
    virtual void startPreview(Rcl::Doc);
    virtual void startNativeViewer(Rcl::Doc, int pagenum = -1,
                                   QString term = QString());
    virtual void openWith(Rcl::Doc, string);
    virtual void saveDocToFile(Rcl::Doc);
    virtual void previewNextInTab(Preview *, int sid, int docnum);
    virtual void previewPrevInTab(Preview *, int sid, int docnum);
    virtual void previewExposed(Preview *, int sid, int docnum);
    virtual void resetSearch();
    virtual void eraseDocHistory();
    virtual void eraseSearchHistory();
    virtual void saveLastQuery();
    virtual void loadSavedQuery();
    virtual void setStemLang(QAction *id);
    virtual void adjustPrefsMenu();
    virtual void catgFilter(int);
    virtual void catgFilter(QAction *);
    virtual void onFragmentsChanged();
    virtual void initDbOpen();
    virtual void toggleFullScreen();
    virtual void on_actionSortByDateAsc_toggled(bool on);
    virtual void on_actionSortByDateDesc_toggled(bool on);
    virtual void on_actionShowResultsAsTable_toggled(bool on);
    virtual void onSortDataChanged(DocSeqSortSpec);
    virtual void resultCount(int);
    virtual void applyStyleSheet();
    virtual void setFilterCtlStyle(int stl);
    virtual void showTrayMessage(const QString& text);

private slots:
    virtual void updateIdxStatus();
    virtual void onWebcacheDestroyed(QObject *);
signals:
    void docSourceChanged(STD_SHARED_PTR<DocSequence>);
    void stemLangChanged(const QString& lang);
    void sortDataChanged(DocSeqSortSpec);
    void resultsReady();
    void searchReset();

protected:
    virtual void closeEvent(QCloseEvent *);
    virtual void showEvent(QShowEvent *);


private:
    Preview        *curPreview;
    AdvSearch      *asearchform;
    UIPrefsDialog  *uiprefs;
    ConfIndexW     *indexConfig;
    IdxSchedW      *indexSched;
    CronToolW      *cronTool;
    RTIToolW       *rtiTool;
    SpellW         *spellform;
    FragButs       *fragbuts;
    SpecIdxW       *specidx;
    QTimer         *periodictimer;
    WebcacheEdit   *webcache;
    ResTable       *restable;
    bool            displayingTable;
    QAction        *m_idNoStem;
    QAction        *m_idAllStem;
    QToolBar       *m_toolsTB;
    QToolBar       *m_resTB;
    QFrame         *m_filtFRM;
    QComboBox      *m_filtCMB;
    QButtonGroup   *m_filtBGRP;
    QMenu          *m_filtMN;
    QFileSystemWatcher m_watcher;
    vector<ExecCmd*>  m_viewers;
    ExecCmd          *m_idxproc; // Indexing process
    bool             m_idxkilled; // Killed my process
    map<QString, QAction*> m_stemLangToId;
    vector<string>    m_catgbutvec;
    int               m_catgbutvecidx;
    DocSeqFiltSpec    m_filtspec;
    bool              m_sortspecnochange;
    DocSeqSortSpec    m_sortspec;
    STD_SHARED_PTR<DocSequence> m_source;
    IndexerState      m_indexerState;
    bool              m_queryActive;
    bool              m_firstIndexing;
    bool              m_searchIsSimple; // Last search was started from simple

    // If set on init, will be displayed either through ext app, or
    // preview (if no ext app set)
    QString          m_urltoview;

    RclTrayIcon     *m_trayicon;

    // We sometimes take the indexer lock (e.g.: when editing the webcache)
    Pidfile         *m_pidfile;
    
    virtual void init();
    virtual void setupResTB(bool combo);
    virtual void previewPrevOrNextInTab(Preview *, int sid, int docnum,
                                        bool next);
    virtual void execViewer(const map<string, string>& subs, bool enterHistory,
                            const string& execpath, const vector<string>& lcmd,
                            const string& cmd, Rcl::Doc doc);
    virtual void setStemLang(const QString& lang);
    virtual void onSortCtlChanged();
    virtual void showIndexConfig(bool modal);
    virtual void showIndexSched(bool modal);
    virtual void showCronTool(bool modal);
    virtual void showRTITool(bool modal);
    virtual void updateIdxForDocs(vector<Rcl::Doc>&);
    virtual void initiateQuery();
    virtual bool containerUpToDate(Rcl::Doc& doc);
    virtual void setFiltSpec();
    virtual bool checkIdxPaths();
};


/* Qt5 moc expands macros when defining signals. The SIGNAL() macro is
   a stringification, so it does not expand macros. We have signals
   where one of the types is a #define (for the variations on
   std::shared_ptr). In qt5, the connection does not work because the
   signal string is different between the definition and the connect
   call, because of the different macro expansion. We have to use
   another level of macro in Qt5 to force macro expansion, but not in
   Qt4, so we both define the XSIGNAL and XSLOT macros here, and have
   ifdefs in the code. What a mess... */
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#define XSIGNAL(X) SIGNAL(X)
#define XSLOT(X) SLOT(X)
#endif

#endif // RCLMAIN_W_H
