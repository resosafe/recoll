/* Copyright (C) 2007 J.F.Dockes
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
#ifdef HAVE_CONFIG_H
#include "autoconfig.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <sstream>
using std::ostringstream;
using std::endl;

#include "reslistpager.h"
#include "debuglog.h"
#include "rclconfig.h"
#include "smallut.h"
#include "plaintorich.h"
#include "mimehandler.h"

// Default highlighter. No need for locking, this is query-only.
static const string cstr_hlfontcolor("<font color=\"blue\">");
static const string cstr_hlendfont("</font>");
class PlainToRichHtReslist : public PlainToRich {
public:
    virtual ~PlainToRichHtReslist() {}
    virtual string startMatch() {return cstr_hlfontcolor;}
    virtual string endMatch() {return cstr_hlendfont;}
};
static PlainToRichHtReslist g_hiliter;

ResListPager::ResListPager(int pagesize) 
    : m_pagesize(pagesize),
      m_newpagesize(pagesize),
      m_winfirst(-1),
      m_hasNext(false),
      m_hiliter(&g_hiliter)
{
}

void ResListPager::resultPageNext()
{
    if (m_docSource.isNull()) {
	LOGDEB(("ResListPager::resultPageNext: null source\n"));
	return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB(("ResListPager::resultPageNext: rescnt %d, winfirst %d\n", 
	    resCnt, m_winfirst));

    if (m_winfirst < 0) {
	m_winfirst = 0;
    } else {
	m_winfirst += m_respage.size();
    }
    // Get the next page of results.
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize);

    if (pagelen <= 0) {
	// No results ? This can only happen on the first page or if the
	// actual result list size is a multiple of the page pref (else
	// there would have been no Next on the last page)
	if (m_winfirst > 0) {
	    // Have already results. Let them show, just disable the
	    // Next button. We'd need to remove the Next link from the page
	    // too.
	    // Restore the m_winfirst value, let the current result vector alone
	    m_winfirst -= m_respage.size();
	} else {
	    // No results at all (on first page)
	    m_winfirst = -1;
	}
	return;
    }
    m_respage = npage;
}

void ResListPager::resultPageFor(int docnum)
{
    if (m_docSource.isNull()) {
	LOGDEB(("ResListPager::resultPageFor: null source\n"));
	return;
    }

    int resCnt = m_docSource->getResCnt();
    LOGDEB(("ResListPager::resultPageFor(%d): rescnt %d, winfirst %d\n", 
	    docnum, resCnt, m_winfirst));
    m_winfirst = (docnum / m_pagesize) * m_pagesize;

    // Get the next page of results.
    vector<ResListEntry> npage;
    int pagelen = m_docSource->getSeqSlice(m_winfirst, m_pagesize, npage);

    // If page was truncated, there is no next
    m_hasNext = (pagelen == m_pagesize);

    if (pagelen <= 0) {
	m_winfirst = -1;
	return;
    }
    m_respage = npage;
}

void ResListPager::displayDoc(RclConfig *config,
			      int i, Rcl::Doc& doc, const HiliteData& hdata,
			      const string& sh)
{
    ostringstream chunk;
    int percent;
    if (doc.pc == -1) {
	percent = 0;
	// Document not available, maybe other further, will go on.
	doc.meta[Rcl::Doc::keyabs] = string(trans("Unavailable document"));
    } else {
	percent = doc.pc;
    }

    // Determine icon to display if any
    string iconpath = iconPath(config, doc.mimetype);

    // Printable url: either utf-8 if transcoding succeeds, or url-encoded
    string url;
    printableUrl(config->getDefCharset(), doc.url, url);

    // Make title out of file name if none yet
    if (doc.meta[Rcl::Doc::keytt].empty()) {
	doc.meta[Rcl::Doc::keytt] = path_getsimple(url);
    }

    // Result number
    char numbuf[20];
    int docnumforlinks = m_winfirst + 1 + i;
    sprintf(numbuf, "%d", docnumforlinks);

    // Document date: either doc or file modification time
    char datebuf[100];
    datebuf[0] = 0;
    if (!doc.dmtime.empty() || !doc.fmtime.empty()) {
	time_t mtime = doc.dmtime.empty() ?
	    atol(doc.fmtime.c_str()) : atol(doc.dmtime.c_str());
	struct tm *tm = localtime(&mtime);
	strftime(datebuf, 99, "&nbsp;%Y-%m-%d&nbsp;%H:%M:%S&nbsp;%z", tm);
    }

    // Size information. We print both doc and file if they differ a lot
    off_t fsize = -1, dsize = -1;
    if (!doc.dbytes.empty())
	dsize = atol(doc.dbytes.c_str());
    if (!doc.fbytes.empty())
	fsize = atol(doc.fbytes.c_str());
    string sizebuf;
    if (dsize > 0) {
	sizebuf = displayableBytes(dsize);
	if (fsize > 10 * dsize && fsize - dsize > 1000)
	    sizebuf += string(" / ") + displayableBytes(fsize);
    } else if (fsize >= 0) {
	sizebuf = displayableBytes(fsize);
    }

    string richabst;
    bool needabstract = parFormat().find("%A") != string::npos;
    if (needabstract && m_docSource.isNotNull()) {
	vector<string> vabs;
	m_docSource->getAbstract(doc, vabs);

	for (vector<string>::const_iterator it = vabs.begin();
	     it != vabs.end(); it++) {
	    if (!it->empty()) {
		// No need to call escapeHtml(), plaintorich handles it
		list<string> lr;
		m_hiliter->set_inputhtml(false);
		m_hiliter->plaintorich(*it, lr, hdata);
		richabst += lr.front();
		richabst += absSep();
	    }
	}
    }

    // Links;
    ostringstream linksbuf;
    if (canIntern(doc.mimetype, config)) { 
	linksbuf << "<a href=\"P" << docnumforlinks << "\">" 
		 << trans("Preview") << "</a>&nbsp;&nbsp;";
    }

    string apptag;
    map<string,string>::const_iterator it;
    if ((it = doc.meta.find(Rcl::Doc::keyapptg)) != doc.meta.end())
	apptag = it->second;

    if (!config->getMimeViewerDef(doc.mimetype, apptag).empty()) {
	linksbuf << "<a href=\"E" << docnumforlinks << "\">"  
		 << trans("Open") << "</a>";
    }

    // Build the result list paragraph:

    // Subheader: this is used by history
    if (!sh.empty())
	chunk << "<p style='clear: both;'><b>" << sh << "</p>\n<p>";
    else
	chunk << "<p style='margin: 0px;padding: 0px;clear: both;'>";

    // Configurable stuff
    map<string,string> subs;
    subs["A"] = !richabst.empty() ? richabst : "";
    subs["D"] = datebuf;
    subs["I"] = iconpath;
    subs["i"] = doc.ipath;
    subs["K"] = !doc.meta[Rcl::Doc::keykw].empty() ? 
	string("[") + escapeHtml(doc.meta[Rcl::Doc::keykw]) + "]" : "";
    subs["L"] = linksbuf.rdbuf()->str();
    subs["N"] = numbuf;
    subs["M"] = doc.mimetype;
    subs["R"] = doc.meta[Rcl::Doc::keyrr];
    subs["S"] = sizebuf;
    subs["T"] = escapeHtml(doc.meta[Rcl::Doc::keytt]);
    subs["U"] = url;

    // Let %(xx) access all metadata.
    subs.insert(doc.meta.begin(), doc.meta.end());

    string formatted;
    pcSubst(parFormat(), formatted, subs);
    chunk << formatted;

    chunk << "</p>" << endl;
    // This was to force qt 4.x to clear the margins (which it should do
    // anyway because of the paragraph's style), but we finally took
    // the table approach for 1.15 for now (in guiutils.cpp)
//	chunk << "<br style='clear:both;height:0;line-height:0;'>" << endl;

    LOGDEB2(("Chunk: [%s]\n", (const char *)chunk.rdbuf()->str().c_str()));
    append(chunk.rdbuf()->str(), i, doc);
}

void ResListPager::displayPage(RclConfig *config)
{
    LOGDEB(("ResListPager::displayPage\n"));
    if (m_docSource.isNull()) {
	LOGDEB(("ResListPager::displayPage: null source\n"));
	return;
    }
    if (m_winfirst < 0 && !pageEmpty()) {
	LOGDEB(("ResListPager::displayPage: sequence error: winfirst < 0\n"));
	return;
    }

    ostringstream chunk;

    // Display list header
    // We could use a <title> but the textedit doesnt display
    // it prominently
    // Note: have to append text in chunks that make sense
    // html-wise. If we break things up too much, the editor
    // gets confused. Hence the use of the 'chunk' text
    // accumulator
    // Also note that there can be results beyond the estimated resCnt.
    chunk << "<html><head>" << endl
	  << "<meta http-equiv=\"content-type\""
	  << " content=\"text/html; charset=utf-8\">" << endl
	  << "</head><body>" << endl
	  << pageTop()
	  << "<p><font size=+1><b>"
	  << m_docSource->title()
	  << "</b></font>&nbsp;&nbsp;&nbsp;";

    if (pageEmpty()) {
	chunk << trans("<p><b>No results found</b><br>");
        vector<string>uterms;
        m_docSource->getUTerms(uterms);
        if (!uterms.empty()) {
            vector<string> spellings;
            suggest(uterms, spellings);
            if (!spellings.empty()) {
                chunk << 
                 trans("<p><i>Alternate spellings (accents suppressed): </i>");
                for (vector<string>::iterator it = spellings.begin();
                     it != spellings.end(); it++) {
                    chunk << *it;
                    chunk << " ";
                }
                chunk << "</p>";
            }
        }
    } else {
	unsigned int resCnt = m_docSource->getResCnt();
	if (m_winfirst + m_respage.size() < resCnt) {
	    chunk << trans("Documents") << " <b>" << m_winfirst + 1
		  << "-" << m_winfirst + m_respage.size() << "</b> " 
		  << trans("out of at least") << " " 
		  << resCnt << " " << trans("for") << " " ;
	} else {
	    chunk << trans("Documents") << " <b>" 
		  << m_winfirst + 1 << "-" << m_winfirst + m_respage.size()
		  << "</b> " << trans("for") << " ";
	}
    }
    chunk << detailsLink();
    if (hasPrev() || hasNext()) {
	chunk << "&nbsp;&nbsp;";
	if (hasPrev()) {
	    chunk << "<a href=\"" + prevUrl() + "\"><b>"
		  << trans("Previous")
		  << "</b></a>&nbsp;&nbsp;&nbsp;";
	}
	if (hasNext()) {
	    chunk << "<a href=\""+ nextUrl() + "\"><b>"
		  << trans("Next")
		  << "</b></a>";
	}
    }
    chunk << "</p>" << endl;

    append(chunk.rdbuf()->str());
    chunk.rdbuf()->str("");
    if (pageEmpty())
	return;

    HiliteData hdata;
    m_docSource->getTerms(hdata.terms, hdata.groups, hdata.gslks);

    // Emit data for result entry paragraph. Do it in chunks that make sense
    // html-wise, else our client may get confused
    for (int i = 0; i < (int)m_respage.size(); i++) {
	Rcl::Doc &doc(m_respage[i].doc);
	string& sh(m_respage[i].subHeader);
	displayDoc(config, i, doc, hdata, sh);
    }

    // Footer
    chunk << "<p align=\"center\">";
    if (hasPrev() || hasNext()) {
	if (hasPrev()) {
	    chunk << "<a href=\"" + prevUrl() + "\"><b>" 
		  << trans("Previous")
		  << "</b></a>&nbsp;&nbsp;&nbsp;";
	}
	if (hasNext()) {
	    chunk << "<a href=\""+ nextUrl() + "\"><b>"
		  << trans("Next")
		  << "</b></a>";
	}
    }
    chunk << "</p>" << endl;
    chunk << "</body></html>" << endl;
    append(chunk.rdbuf()->str());
}

// Default implementations for things that should be implemented by 
// specializations
string ResListPager::nextUrl()
{
    return "n-1";
}

string ResListPager::prevUrl()
{
    return "p-1";
}

string ResListPager::iconPath(RclConfig *config, const string& mtype)
{
    string iconpath;
    config->getMimeIconName(mtype, &iconpath);
    iconpath = string("file://") + iconpath;
    return iconpath;
}

bool ResListPager::append(const string& data)
{
    fprintf(stderr, "%s", data.c_str());
    return true;
}

string ResListPager::trans(const string& in) 
{
    return in;
}

string ResListPager::detailsLink()
{
    string chunk = "<a href=\"H-1\">";
    chunk += trans("(show query)") + "</a>";
    return chunk;
}

const string &ResListPager::parFormat()
{
    static const string cstr_format("<img src=\"%I\" align=\"left\">"
				    "%R %S %L &nbsp;&nbsp;<b>%T</b><br>"
				    "%M&nbsp;%D&nbsp;&nbsp;&nbsp;<i>%U</i><br>"
				    "%A %K");
    return cstr_format;
}

