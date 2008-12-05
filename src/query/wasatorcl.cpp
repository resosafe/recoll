#ifndef lint
static char rcsid[] = "@(#$Id: wasatorcl.cpp,v 1.16.2.2 2008-12-05 13:13:02 dockes Exp $ (C) 2006 J.F.Dockes";
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
#include <cstdio>
#include <string>
#include <list>
using std::string;
using std::list;

#include "wasastringtoquery.h"
#include "rcldb.h"
#include "searchdata.h"
#include "wasatorcl.h"
#include "debuglog.h"
#include "smallut.h"
#include "rclconfig.h"
#include "refcntr.h"

Rcl::SearchData *wasaStringToRcl(const string &qs, string &reason)
{
    StringToWasaQuery parser;
    WasaQuery *wq = parser.stringToQuery(qs, reason);
    if (wq == 0) 
	return 0;
    Rcl::SearchData *rq = wasaQueryToRcl(wq);
    if (rq == 0) {
	reason = "Failed translating wasa query structure to recoll";
	return 0;
    }
    return rq;
}

Rcl::SearchData *wasaQueryToRcl(WasaQuery *wasa)
{
    if (wasa == 0)
	return 0;
    if (wasa->m_op != WasaQuery::OP_AND && wasa->m_op != WasaQuery::OP_OR) {
	LOGERR(("wasaQueryToRcl: top query neither AND nor OR!\n"));
	return 0;
    }

    Rcl::SearchData *sdata = new 
	Rcl::SearchData(wasa->m_op == WasaQuery::OP_AND ? Rcl::SCLT_AND : 
			Rcl::SCLT_OR);
    LOGDEB2(("wasaQueryToRcl: %s chain\n", wasa->m_op == WasaQuery::OP_AND ? 
	     "AND" : "OR"));

    WasaQuery::subqlist_t::iterator it;
    Rcl::SearchDataClause *nclause;

    for (it = wasa->m_subs.begin(); it != wasa->m_subs.end(); it++) {
	switch ((*it)->m_op) {
	case WasaQuery::OP_NULL:
	case WasaQuery::OP_AND:
	default:
	    LOGINFO(("wasaQueryToRcl: found bad NULL or AND q type in list\n"));
	    continue;
	case WasaQuery::OP_LEAF: {
	    LOGDEB2(("wasaQueryToRcl: leaf clause [%s]:[%s]\n", 
		     (*it)->m_fieldspec.c_str(), (*it)->m_value.c_str()));
	    unsigned int mods = (unsigned int)(*it)->m_modifiers;
	    // Special cases (mime, category, dir filter ...). Not pretty.
	    if (!stringicmp("mime", (*it)->m_fieldspec) ||
		!stringicmp("format", (*it)->m_fieldspec)
		) {
		sdata->addFiletype((*it)->m_value);
		break;
	    } 

	    // Xesam uses "type", we also support "rclcat", for broad
	    // categories like "audio", "presentation", etc.
	    if (!stringicmp("rclcat", (*it)->m_fieldspec) ||
		!stringicmp("type", (*it)->m_fieldspec)) {
		RclConfig *conf = RclConfig::getMainConfig();
		list<string> mtypes;
		if (conf && conf->getMimeCatTypes((*it)->m_value, mtypes)) {
		    for (list<string>::iterator mit = mtypes.begin();
			 mit != mtypes.end(); mit++) {
			sdata->addFiletype(*mit);
		    }
		}
		break;
	    } 
	    if (!stringicmp("dir", (*it)->m_fieldspec)) {
		sdata->setTopdir((*it)->m_value);
		break;
	    } 

	    if (TextSplit::hasVisibleWhite((*it)->m_value)) {
		int slack = (mods & WasaQuery::WQM_PHRASESLACK) ? 10 : 0;
		Rcl::SClType tp = Rcl::SCLT_PHRASE;
		if (mods & WasaQuery::WQM_PROX) {
		    tp = Rcl::SCLT_NEAR;
		    slack = 10;
		}
		nclause = new Rcl::SearchDataClauseDist(tp, (*it)->m_value,
							slack,
							(*it)->m_fieldspec);
	    } else {
		nclause = new Rcl::SearchDataClauseSimple(Rcl::SCLT_AND, 
							  (*it)->m_value, 
							  (*it)->m_fieldspec);
	    }
	    if (nclause == 0) {
		LOGERR(("wasaQueryToRcl: out of memory\n"));
		return 0;
	    }
	    if ((*it)->m_modifiers & WasaQuery::WQM_NOSTEM) {
		fprintf(stderr, "Setting NOSTEM\n");
		nclause->setModifiers(Rcl::SearchDataClause::SDCM_NOSTEMMING);
	    }
	    sdata->addClause(nclause);
	}
	    break;
	    
	case WasaQuery::OP_EXCL:
	    LOGDEB2(("wasaQueryToRcl: excl clause [%s]:[%s]\n", 
		     (*it)->m_fieldspec.c_str(), (*it)->m_value.c_str()));
	    if (wasa->m_op != WasaQuery::OP_AND) {
		LOGERR(("wasaQueryToRcl: negative clause inside OR list!\n"));
		continue;
	    }
	    // Note: have to add dquotes which will be translated to
	    // phrase if there are several words in there. Not pretty
	    // but should work. If there is actually a single
	    // word, it will not be taken as a phrase, and
	    // stem-expansion will work normally
	    nclause = new Rcl::SearchDataClauseSimple(Rcl::SCLT_EXCL, 
						      string("\"") + 
						      (*it)->m_value + "\"",
						      (*it)->m_fieldspec);
	    
	    if (nclause == 0) {
		LOGERR(("wasaQueryToRcl: out of memory\n"));
		return 0;
	    }
	    if ((*it)->m_modifiers & WasaQuery::WQM_NOSTEM)
		nclause->setModifiers(Rcl::SearchDataClause::SDCM_NOSTEMMING);
	    sdata->addClause(nclause);
	    break;

	case WasaQuery::OP_OR:
	    LOGDEB2(("wasaQueryToRcl: OR clause [%s]:[%s]\n", 
		     (*it)->m_fieldspec.c_str(), (*it)->m_value.c_str()));
	    // Create a subquery.
	    Rcl::SearchData *sub = wasaQueryToRcl(*it);
	    if (sub == 0) {
		continue;
	    }
	    nclause = 
		new Rcl::SearchDataClauseSub(Rcl::SCLT_SUB, 
					     RefCntr<Rcl::SearchData>(sub));
	    if (nclause == 0) {
		LOGERR(("wasaQueryToRcl: out of memory\n"));
		return 0;
	    }
	    if ((*it)->m_modifiers & WasaQuery::WQM_NOSTEM)
		nclause->setModifiers(Rcl::SearchDataClause::SDCM_NOSTEMMING);
	    sdata->addClause(nclause);
	}
    }

    return sdata;
}
