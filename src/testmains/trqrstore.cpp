/* Copyright (C) 2017-2019 J.F.Dockes
 *
 * License: GPL 2.1
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include <string>
#include <iostream>
#include <set>
#include <malloc.h>
#include <unistd.h>

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free
#include <obstack.h>
#define OBSTACK_STRINGCOPY(STK, S) \
    (char *)obstack_copy(&STK, S.c_str(), S.size()+1)

#include "rclinit.h"
#include "rclconfig.h"
#include "rcldb.h"
#include "searchdata.h"
#include "rclquery.h"
#include "pathut.h"
#include "rclutil.h"
#include "wasatorcl.h"
#include "log.h"
#include "pathut.h"
#include "plaintorich.h"
#include "hldata.h"
#include "smallut.h"
#include "qresultstore.h"

//const std::string confdir{"/home/dockes/.recoll-prod"};
const std::string confdir{"/var/cache/upmpdcli/uprcl"};
const int MB = 1024 *1024;

// Docs as docs
#undef STORE_DOCS
// Docs as map<string,string>
#undef STORE_MAPS_RAW
// Docs as map<string*,string>, shared key storage
#undef STORE_MAPS_SHAREDKEYS
// Docs as map<string*,char*> shared key storage+obstack for data
#undef STORE_MAPS_SHAREDKEYS_OBSTACK
// Docs as vector<char*> shared key-to-idx storage + pointer vector
#define STORE_ARRAYS
// Minimal, not really usable, needs linear search inside record to find field
#undef STORE_ALLOBSTACK

static void meminfo(const char *msg)
{
    struct mallinfo minfo = mallinfo();
    std::cerr << msg  << " : " << minfo.uordblks / MB << " MB\n";
}

static inline bool testentry(const std::pair<std::string,std::string>& entry)
{
#undef ALLENTRIES
#ifdef ALLENTRIES
    return true;
#else
    return !entry.second.empty() &&
        entry.first != "author" && entry.first != "ipath" &&
        entry.first != "rcludi"
        && entry.first != "relevancyrating" && entry.first != "sig"
        && entry.first != "abstract" && entry.first != "caption" &&
        entry.first != "filename" && entry.first != "origcharset" &&
        entry.first != "sig";
#endif   
}


int main(int argc, char *argv[])
{
    std::string reason;
    RclConfig *rclconfig = recollinit(0, 0, 0, reason, &confdir);
    if (!rclconfig || !rclconfig->ok()) {
        std::cerr << "Recoll init failed: " << reason << "\n";
        return 1;
    }
    meminfo("Before db open");
    Rcl::Db rcldb(rclconfig);
    if (!rcldb.open(Rcl::Db::DbRO)) {
        std::cerr << "db open error\n";
        return 1;
    }
    
    Rcl::Query query(&rcldb);
    std::shared_ptr<Rcl::SearchData> sd{wasaStringToRcl(
            rclconfig, "english", "mime:*", reason)};
    if (!sd) {
        std::cerr << "wasStringToRcl: " << reason << "\n";
        return 1;
    }

    meminfo("Before setquery");

    query.setQuery(sd);

    int cnt = query.getResCnt();

    meminfo("After getResCnt");

    std::cerr << "Got " << cnt << " estimated results\n";

#if !defined(STORE_ARRAYS)
    // Store_arrays needs 2 walks anyway, to find the field names
    int i = 0;
    for (;;) {
        Rcl::Doc doc;
        if (!query.getDoc(i++, doc, false)) {
            break;
        }
    }
    int imax = i;
    meminfo("After getDocs");
    std::cerr << "Got " << imax << " docs\n";
#endif // !STORE_ARRAYS

#if defined(STORE_DOCS)
    //
    // 25 kdocs (music tags)
    // Each result stored as Rcl::Doc: 85 MB
    //
    std::vector<Rcl::Doc> docs;
    docs.resize(imax);
    meminfo("After resize");
    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, docs[i], false)) {
            break;
        }
    }


#elif defined(STORE_MAPS_RAW)
    //
    // Each result stored as map<string,string> 58 MB
    //
    std::vector<std::map<std::string,std::string>> docs;
    docs.resize(imax);
    meminfo("After resize");
    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = docs[i];
        vdoc["url"] = doc.url;
        vdoc["mimetype"] = doc.mimetype;
        vdoc["fmtime"] = doc.fmtime;
        vdoc["dmtime"] = doc.dmtime;
        vdoc["fbytes"] = doc.fbytes;
        vdoc["dbytes"] = doc.dbytes;
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                //std::cerr << entry.first << "->" << entry.second <<"\n";
                vdoc.insert(entry);
            }
        }
        //std::cerr << "\n";
    }

#elif defined(STORE_MAPS_SHAREDKEYS)
    // 
    // Each result stored as map<string*, string> with shared keys:
    // Memory: audio: 56 MB, main: 221
    // 
    std::set<std::string> keys;
    std::vector<std::map<const std::string*, std::string>> docs;
    docs.resize(imax);
    meminfo("After resize");
    std::string cstr_url{"url"};
    std::string cstr_mt{"mimetype"};
    std::string cstr_fmt{"fmtime"};
    std::string cstr_dmt{"dmtime"};
    std::string cstr_fb{"fbytes"};
    std::string cstr_db{"dbytes"};
    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = docs[i];
        vdoc[&cstr_url] = doc.url;
        vdoc[&cstr_mt] = doc.mimetype;
        vdoc[&cstr_fmt] = doc.fmtime;
        vdoc[&cstr_dmt] = doc.dmtime;
        vdoc[&cstr_fb] = doc.fbytes;
        vdoc[&cstr_db] = doc.dbytes;
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                auto it = keys.find(entry.first);
                if (it == keys.end()) {
                    keys.insert(entry.first);
                    it = keys.find(entry.first);
                }
                //std::cerr << *it << "->" << entry.second <<"\n";
                vdoc.insert({&*it, entry.second});
            }
        }
        //std::cerr << "\n";
    }

#elif defined(STORE_MAPS_SHAREDKEYS_OBSTACK)
    // 
    // Each result stored as map<string*, char*> with shared keys and
    // obstack string storage:
    // Memory: audio: 54 MB main: 213 MB
    //
    std::set<std::string> keys;
    std::vector<std::map<const std::string*, const char*>> docs;
    struct obstack obst;
    obstack_init(&obst);
    obstack_chunk_size(&obst) = 1024*1024;
    docs.resize(imax);
    meminfo("After resize");
    std::string cstr_url{"url"};
    std::string cstr_mt{"mimetype"};
    std::string cstr_fmt{"fmtime"};
    std::string cstr_dmt{"dmtime"};
    std::string cstr_fb{"fbytes"};
    std::string cstr_db{"dbytes"};
    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = docs[i];
        vdoc[&cstr_url] =OBSTACK_STRINGCOPY(obst, doc.url);
        vdoc[&cstr_mt] = OBSTACK_STRINGCOPY(obst, doc.mimetype);
        vdoc[&cstr_fmt] =OBSTACK_STRINGCOPY(obst, doc.fmtime);
        vdoc[&cstr_dmt] =OBSTACK_STRINGCOPY(obst, doc.dmtime);
        vdoc[&cstr_fb] = OBSTACK_STRINGCOPY(obst, doc.fbytes);
        vdoc[&cstr_db] = OBSTACK_STRINGCOPY(obst, doc.dbytes);
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                auto it = keys.find(entry.first);
                if (it == keys.end()) {
                    keys.insert(entry.first);
                    it = keys.find(entry.first);
                }
                const char *cp = OBSTACK_STRINGCOPY(obst, entry.second);
                //std::cerr << *it << "->" << cp <<"\n";
                vdoc.insert({&*it, cp});
            }
        }
        //std::cerr << "\n";
    }

#elif defined(STORE_ARRAYS)
    //
    // Each result stored as a vector<const char*> with a shared
    // key->intidx map to store the key name to index mapping, and and
    // obstack string storage for the values.
    //    Memory: audio: 29 MB. Main index: 95 MB
    //
    // On the audio test index, the total string size computed below
    // is 8 MB for 25169 docs, including terminating zeroes for
    // non-present fields (see the counting in the last version below).
    //
    // Residual mystery:
    //   After the obstack_free, the residual memory usage (audio) is 13 MB.
    //   So 29-13 = 16 MB for the obstack storage. Why??
    //   the Vector size should be around 25kdocs * 59keys * 8bytes = 12 MB
    //
    //   On the main db: 151350 docs 16 keys 95 MB.
    //   After free 38 MB. Array is 151k * 16 * 8 = 19 MB
    //   19 MB missing ??
    //
    // The missing bytes ? probably reflect the immense amount of
    // allocations performed by the program while building the
    // array. Valgrind:
    //  HEAP SUMMARY:
    //    in use at exit: 652,298 bytes in 3,033 blocks
    //    total heap usage: 8,613,584 allocs, 8,610,551 frees, 
    //      4,421,628,817 bytes allocated
    //
    // Possible optimizations: the record arrays don't really need to
    // be all pointers, and could use a single char * followed by byte
    // displacements which could be 32 (or even 16?) bits. This would
    // gain at least an additional 9 MB, getting the storage size to
    // around 20 MB for the audio 25k index. This is really much
    // better than the initial 85 (or even 58 for maps), with a read
    // performance impact which should be quite modest.
    //  ** This supposes that we don't use obstack though, as obstack
    //     placement is unpredictable.
    //
    // This the solution now implemented: no obstack, use struct with offsets
    // This uses 19 MB of storage for the audio index, and 72 MB for
    // the main one (less keys->less gain)
{
    Rcl::QResultStore store;
    bool result = store.storeQuery(
        query, {"author", "ipath", "rcludi", "relevancyrating", 
                "sig","abstract", "caption", "filename",  "origcharset", "sig"});
    if (!result) {
        std::cerr << "storeQuery failed\n";
        return 1;
    }
    meminfo("After storing");
    std::cerr << "url 20 " << store.fieldValue(20, "url") << "\n";
}
#elif defined(STORE_ALLOBSTACK)

    //
    // Each result stored as a single array of chars with concatenated
    // 0-terminated strings and shared key->intidx map. Slow access,
    // but should be almost minimal.
    // Memory: audio: 14 MB, maindb: 55 MB
    //
    // This is not really practical because any field access will need
    // a linear search inside a single record to find its beginning by
    // crossing/counting ending null bytes.
    //
    // Note: the obstack storage for record strings may have to be
    // replaced by continuous storage if it proves too complicated to
    // walk. This should not change the volume significantly.
    //
    // On the audio test index, the total string size computed below
    // is 8 MB for 25169 docs, including terminating zeroes for
    // non-present fields.
    //
    // The program uses 3 MB of memory prior to storing the strings,
    // meaning that the character data itself uses 11 MB.
    // I don't understand where the 3MB of overhead comes from.
    // The vector<char*> itself is 8 * 25k = 200 KB.
    //
    // With a chunk size of 1MB, the obstack overhead should be
    // negligible. Varying the chunk size seems to indicate it is.
    //
    // Retested on the main index (150k docs, 30MB of strings) yields
    // 55 MB of total storage including 15 MB of prestorage usage, so
    // 10MB of unexplained overhead. Strange is that pre-key compute
    // is 1 instead of 3 for the audio db?? And why 15 pre-storage
    // instead of 3??
    //
    std::vector<char*> docs;
    struct obstack obst;
    obstack_init(&obst);
    obstack_chunk_size(&obst) = 1024*1024;
    obstack_alignment_mask(&obst) = 0;
    docs.resize(imax);
    meminfo("After resize");
    std::map<std::string, int> keyidx {
        {"url",0},
        {"mimetype", 1},
        {"fmtime", 2},
        {"dmtime", 3},
        {"fbytes", 4},
        {"dbytes", 5},
    };

    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                auto it = keyidx.find(entry.first);
                if (it == keyidx.end()) {
                    int idx = keyidx.size();
                    keyidx.insert({entry.first, idx});
                };
            }
        }
    }

    // 49 keys on audio db, 15 for the main index.
    std::cerr << "Found " << keyidx.size() << " different keys\n";
    meminfo("After computing keys");

    int alldocstringsize{0};
    for (i=0;i<imax;i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        docs[i] = OBSTACK_STRINGCOPY(obst, doc.url);
        OBSTACK_STRINGCOPY(obst, doc.mimetype);
        alldocstringsize += doc.mimetype.size() + 1;
        OBSTACK_STRINGCOPY(obst, doc.fmtime);
        alldocstringsize += doc.fmtime.size() + 1;
        OBSTACK_STRINGCOPY(obst, doc.dmtime);
        alldocstringsize += doc.dmtime.size() + 1;
        OBSTACK_STRINGCOPY(obst, doc.fbytes);
        alldocstringsize += doc.fbytes.size() + 1;
        OBSTACK_STRINGCOPY(obst, doc.dbytes);
        alldocstringsize += doc.dbytes.size() + 1;
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                OBSTACK_STRINGCOPY(obst, entry.second);
                alldocstringsize += entry.second.size() + 1;
            }
        }
    }
    std::cerr << "Total string size: " << alldocstringsize / MB << "\n";

#else
#error must define one    
#endif
    
    meminfo("Before exit");
//    pause();
    return 0;
}

#if 0
AUDIO store_arrays:
Before db open : 0 MB
Before setquery : 0 MB
After getResCnt : 1 MB
Got 14563 estimated results
After getDocs : 3 MB
Got 25169 docs
After resize : 3 MB
Found 49 different keys
After storing : 29 MB

AUDIO all_obstack:

Before db open : 0 MB
Before setquery : 0 MB
After getResCnt : 1 MB
Got 14563 estimated results
After getDocs : 3 MB
Got 25169 docs
After resize : 3 MB
Found 49 different keys
After computing keys : 3 MB
Total string size: 8
After storing : 14 MB

MAINDB, store_arrays

Before db open : 0 MB
Before setquery : 0 MB
After getResCnt : 1 MB
Got 68746 estimated results
After getDocs : 1 MB
Got 151351 docs
After resize : 4 MB
Found 16 different keys
After storing : 95 MB

MAINDB, all_obstack:

Before db open : 0 MB
Before setquery : 0 MB
After getResCnt : 1 MB
Got 68746 estimated results
After getDocs : 1 MB
Got 151350 docs
After resize : 2 MB
Found 16 different keys
After computing keys : 15 MB
Total string size: 30
After storing : 55 MB

#endif

#if 0
STORE_ARRAYS, Obstack version
    struct obstack obst;
    obstack_init(&obst);
    obstack_chunk_size(&obst) = 1024*1024;
    std::map<std::string, int> keyidx {
        {"url",0},
        {"mimetype", 1},
        {"fmtime", 2},
        {"dmtime", 3},
        {"fbytes", 4},
        {"dbytes", 5},
    };

    int imax = 0;
    for (;;imax++) {
        Rcl::Doc doc;
        if (!query.getDoc(imax, doc, false)) {
            break;
        }
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                auto it = keyidx.find(entry.first);
                if (it == keyidx.end()) {
                    int idx = keyidx.size();
                    keyidx.insert({entry.first, idx});
                };
            }
        }
    }
    // 49 keys !
    std::cerr << "Found " << keyidx.size() << " different keys\n";
    std::vector<std::vector<const char*>> docs;
    docs.resize(imax);
    meminfo("After resize");
    
    for (int i = 0; i < imax; i++) {
        Rcl::Doc doc;
        if (!query.getDoc(i, doc, false)) {
            break;
        }
        auto& vdoc = docs[i];
        vdoc.resize(keyidx.size());
        vdoc[0] = OBSTACK_STRINGCOPY(obst, doc.url);
        vdoc[1] = OBSTACK_STRINGCOPY(obst, doc.mimetype);
        vdoc[2] = OBSTACK_STRINGCOPY(obst, doc.fmtime);
        vdoc[3] = OBSTACK_STRINGCOPY(obst, doc.dmtime);
        vdoc[4] = OBSTACK_STRINGCOPY(obst, doc.fbytes);
        vdoc[5] = OBSTACK_STRINGCOPY(obst, doc.dbytes);
        for (const auto& entry : doc.meta) {
            if (testentry(entry)) {
                auto it = keyidx.find(entry.first);
                if (it == keyidx.end()) {
                    std::cerr << "Unknown key: " << entry.first << "\n";
                    abort();
                }
                if (it->second <= 5) {
                    continue;
                }
                const char *cp = OBSTACK_STRINGCOPY(obst, entry.second);
                vdoc[it->second] = cp;
            }
        }
    }
    meminfo("After storing");
    obstack_free(&obst, nullptr);
    meminfo("After obstack_free");
#endif
