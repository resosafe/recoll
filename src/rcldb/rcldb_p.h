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

#ifndef _rcldb_p_h_included_
#define _rcldb_p_h_included_

#include "xapian.h"

namespace Rcl {

// Omega compatible values. We leave a hole for future omega values. Not sure 
// it makes any sense to keep any level of omega compat given that the index
// is incompatible anyway.
enum value_slot {
    VALUE_LASTMOD = 0,	// 4 byte big endian value - seconds since 1970.
    VALUE_MD5 = 1,	// 16 byte MD5 checksum of original document.
    VALUE_SIG = 10      // Doc sig as chosen by app (ex: mtime+size
};

// Generic Xapian exception catching code. We do this quite often,
// and I have no idea how to do this except for a macro
#define XCATCHERROR(MSG) \
 catch (const Xapian::Error &e) {		   \
    MSG = e.get_msg();				   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (const string &s) {			   \
    MSG = s;					   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (const char *s) {			   \
    MSG = s;					   \
    if (MSG.empty()) MSG = "Empty error message";  \
 } catch (...) {				   \
    MSG = "Caught unknown xapian exception";	   \
 } 

#define XAPTRY(STMTTOTRY, XAPDB, ERSTR)       \
    for (int tries = 0; tries < 2; tries++) { \
	try {                                 \
            STMTTOTRY;                        \
            ERSTR.erase();                    \
            break;                            \
	} catch (const Xapian::DatabaseModifiedError &e) { \
            ERSTR = e.get_msg();                           \
	    XAPDB.reopen();                                \
            continue;                                      \
	} XCATCHERROR(ERSTR);                              \
        break;                                             \
    }

class Query;

// A class for data and methods that would have to expose
// Xapian-specific stuff if they were in Rcl::Db. There could actually be
// 2 different ones for indexing or query as there is not much in
// common.
class Db::Native {
 public:
    Db  *m_rcldb; // Parent
    bool m_isopen;
    bool m_iswritable;
    bool m_noversionwrite; //Set if open failed because of version mismatch!

    // Indexing 
    Xapian::WritableDatabase xwdb;
    // Querying (active even if the wdb is too)
    Xapian::Database xrdb;

    // We sometimes go through the wdb for some query ops, don't
    // really know if this makes sense
    Xapian::Database& xdb() {return m_iswritable ? xwdb : xrdb;}

    Native(Db *db) 
	: m_rcldb(db), m_isopen(false), m_iswritable(false),
          m_noversionwrite(false)
    { }

    ~Native() {
    }

    string makeAbstract(Xapian::docid id, Query *query);

    bool dbDataToRclDoc(Xapian::docid docid, std::string &data, Doc &doc);

    /** Compute list of subdocuments for a given udi. We look for documents 
     * indexed by a parent term matching the udi, the posting list for the 
     * parentterm(udi)  (As suggested by James Aylett)
     *
     * Note that this is not currently recursive: all subdocs are supposed 
     * to be children of the file doc.
     * Ie: in a mail folder, all messages, attachments, attachments of
     * attached messages etc. must have the folder file document as
     * parent. 
     * Parent-child relationships are defined by the indexer (rcldb user)
     * 
     * The file-system indexer currently works this way (flatly), 
     * subDocs() could be relatively easily changed to support full recursivity
     * if needed.
     */
    bool subDocs(const string &udi, vector<Xapian::docid>& docids);

};

}
#endif /* _rcldb_p_h_included_ */
