#ifndef _RESLIST_H_INCLUDED_
#define _RESLIST_H_INCLUDED_
/* @(#$Id: reslist.h,v 1.17 2008-12-16 14:20:10 dockes Exp $  (C) 2005 J.F.Dockes */

#include <list>

#ifndef NO_NAMESPACES
using std::list;
#endif

#if (QT_VERSION < 0x040000)
#include <qtextbrowser.h>
class QPopupMenu;
#define RCLPOPUP QPopupMenu
#define QTEXTBROWSER QTextBrowser
#else
#include <q3textbrowser.h>
class Q3PopupMenu;
#define RCLPOPUP Q3PopupMenu
#define QTEXTBROWSER Q3TextBrowser
#endif

#include "docseq.h"
#include "sortseq.h"
#include "filtseq.h"
#include "refcntr.h"
#include "rcldoc.h"
#include "reslistpager.h"

class ResList;

class QtGuiResListPager : public ResListPager {
public:
    QtGuiResListPager(ResList *p, int ps) 
	: ResListPager(ps), m_parent(p) 
    {}
    virtual bool append(const string& data);
    virtual bool append(const string& data, int idx, const Rcl::Doc& doc);
    virtual string trans(const string& in);
    virtual string detailsLink();
    virtual const string &parFormat();
    virtual string nextUrl();
    virtual string prevUrl();
    virtual string pageTop();
    virtual string iconPath(const string& mt);
private:
    ResList *m_parent;
};

/**
 * Display a list of document records. The data can be out of the history 
 * manager or from an index query, both abstracted as a DocSequence. 
 * Sorting and filtering are applied by stacking Sort/Filter DocSequences.
 * This is nice because history and index result are handled the same, but 
 * not nice because we can't use the sort/filter capabilities in the index 
 * engine, and do it instead on the index output, which duplicates code and 
 * may be sometimes slower.
 */
class ResList : public QTEXTBROWSER
{
    Q_OBJECT;

    friend class QtGuiResListPager;
 public:
    ResList(QWidget* parent = 0, const char* name = 0);
    virtual ~ResList();
    
    // Return document for given docnum. We act as an intermediary to
    // the docseq here. This has also the side-effect of making the
    // entry current (visible and highlighted), and only work if the
    // num is inside the current page or its immediate neighbours.
    bool getDoc(int docnum, Rcl::Doc &);

    QString getDescription(); // Printable actual query performed on db
    int getResCnt(); // Return total result list size
    void setDocSource(RefCntr<DocSequence> ndocsource);
    bool displayingHistory();
    bool getTerms(vector<string>& terms, 
		  vector<vector<string> >& groups, vector<int>& gslks);
    list<string> expand(Rcl::Doc& doc);
    int listId() const {return m_listId;}

 public slots:
    virtual void resetList();     // Erase current list
    virtual void doubleClicked(int, int);
    virtual void resPageUpOrBack(); // Page up pressed
    virtual void resPageDownOrNext(); // Page down pressed
    virtual void resultPageBack(); // Previous page of results
    virtual void resultPageFirst(); // First page of results
    virtual void resultPageNext(); // Next (or first) page of results
    virtual void displayPage(); // Display current page
    virtual void menuPreview();
    virtual void menuSaveToFile();
    virtual void menuEdit();
    virtual void menuCopyFN();
    virtual void menuCopyURL();
    virtual void menuExpand();
    virtual void menuSeeParent();
    virtual void previewExposed(int);
    virtual void append(const QString &text);
    // Only used for qt ver >=4 but seems we cant undef it
    virtual void selecChanged();
    virtual void setDocSource();
    virtual void setSortParams(DocSeqSortSpec spec);
    virtual void setFilterParams(const DocSeqFiltSpec &spec);

 signals:
    void nextPageAvailable(bool);
    void prevPageAvailable(bool);
    void docEditClicked(int);
    void docPreviewClicked(int, int);
    void docSaveToFileClicked(int);
    void previewRequested(Rcl::Doc);
    void editRequested(Rcl::Doc);
    void headerClicked();
    void docExpand(int);
    void wordSelect(QString);
    void linkClicked(const QString&, int); // See emitLinkClicked()

 protected:
    void keyPressEvent(QKeyEvent *e);
    void contentsMouseReleaseEvent(QMouseEvent *e);

 protected slots:
    virtual void languageChange();
    virtual void linkWasClicked(const QString &, int);
    virtual void showQueryDetails();

 private:
    QtGuiResListPager           *m_pager;
    // Raw doc source
    RefCntr<DocSequence>  m_baseDocSource;
    // Possibly filtered/sorted docsource (the one displayed)
    RefCntr<DocSequence>  m_docSource;
    // Current sort and filtering parameters
    DocSeqSortSpec        m_sortspecs;
    DocSeqFiltSpec        m_filtspecs;
    // Docs for current page
    std::vector<Rcl::Doc> m_curDocs;

    // Translate from textedit paragraph number to relative
    // docnum. Built while we insert text into the qtextedit
    std::map<int,int>  m_pageParaToReldocnums;
    int                m_popDoc; // Docnum for the popup menu.
    int                m_curPvDoc;// Docnum for current preview
    int                m_lstClckMod; // Last click modifier. 
    list<int>          m_selDocs;
    int                m_listId;

    
    virtual int docnumfromparnum(int);
    virtual int parnumfromdocnum(int);

    // Don't know why this is necessary but it is
    void emitLinkClicked(const QString &s) {
	emit linkClicked(s, m_lstClckMod);
    };
    virtual RCLPOPUP *createPopupMenu(const QPoint& pos);
    static int newListId();
};


#endif /* _RESLIST_H_INCLUDED_ */
