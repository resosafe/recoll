/* Copyright (C) 2005 J.F.Dockes 
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "autoconfig.h"
#ifdef USE_XALAN
    #include <sstream>
    #include <xercesc/dom/DOMDocument.hpp>
    #include <xercesc/dom/DOMImplementation.hpp>
    #include <xercesc/framework/URLInputSource.hpp>
    #include <xalanc/PlatformSupport/URISupport.hpp>
    #include <xalanc/XercesParserLiaison/XercesDOMSupport.hpp>

#if defined(HAVE_MALLOC_H)
#include <malloc.h>
#elif defined(HAVE_MALLOC_MALLOC_H)
#include <malloc/malloc.h>
#endif

    XALAN_USING_XALAN(XSLTInputSource)
    XALAN_USING_XALAN(XalanTransformer)
    XALAN_USING_XALAN(XSLTResultTarget)
    XALAN_USING_XALAN(XalanCompiledStylesheet)
    XALAN_USING_XERCES(XMLPlatformUtils)
    #define XSLTStyleSheet const XalanCompiledStylesheet

    pthread_once_t  thread_once_control = PTHREAD_ONCE_INIT;                                                                                      
    void xalan_init() {                                                                               
        XMLPlatformUtils::Initialize();
        XalanTransformer::initialize();                                                           
    }

#else
    #include <libxml/parser.h>
    #include <libxml/tree.h>
    #include <libxslt/transform.h>
    #include <libxslt/xsltInternals.h>
    #include <libxslt/xsltutils.h>

    #define XSLTStyleSheet xsltStylesheet

#endif
#include "cstr.h"
#include "mh_xslt.h"
#include "log.h"
#include "smallut.h"
#include "md5ut.h"
#include "rclconfig.h"
#include "readfile.h"

#ifndef _WIN32
#include "malloc.h"
#endif

using namespace std;

// Do we need this? It would need to be called from recollinit
// Call once, not reentrant
// xmlInitParser();
// LIBXML_TEST_VERSION;
// Probably not:    xmlCleanupParser();
        

<<<<<<< HEAD
#ifdef USE_XALAN
    class FileScanXML : public FileScanDo {
    public:
        FileScanXML(const string& fn) : m_doc(m_docBuf), m_fn(fn)  {}
       
        XSLTInputSource& getDoc() {
            return m_doc;            
=======
class FileScanXML : public FileScanDo {
public:
    FileScanXML(const string& fn) : m_fn(fn) {}
    virtual ~FileScanXML() {
        if (ctxt) {
            xmlFreeParserCtxt(ctxt);
            // This should not be necessary (done by free), but see
            // http://xmlsoft.org/xmlmem.html#Compacting The
            // malloc_trim() and mallopt() doc seems to be a bit
            // misleading, there is probably a frag size under which
            // free() does not try to malloc_trim() at all
#ifdef HAVE_MALLOC_TRIM
            malloc_trim(0);
#endif /* HAVE_MALLOC_TRIM */
>>>>>>> upstream/master
        }

<<<<<<< HEAD
        virtual bool init(int64_t size, string *) {
=======
    xmlDocPtr getDoc() {
        int ret;
        if ((ret = xmlParseChunk(ctxt, nullptr, 0, 1))) {
            xmlError *error = xmlGetLastError();
            LOGERR("FileScanXML: final xmlParseChunk failed with error " <<
                   ret << " error: " <<
                   (error ? error->message :
                    " null return from xmlGetLastError()") << "\n");
            return nullptr;
        }
        return ctxt->myDoc;
    }

    virtual bool init(int64_t, string *) {
        ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, m_fn.c_str());
        if (ctxt == nullptr) {
            LOGERR("FileScanXML: xmlCreatePushParserCtxt failed\n");
            return false;
        } else {
>>>>>>> upstream/master
            return true;
        }
        
        virtual bool data(const char *buf, int cnt, string*) {
            if (0) {
                string dt(buf, cnt);
                LOGDEB1("FileScanXML: data: cnt " << cnt << " data " << dt << endl);
            } else {
                LOGDEB1("FileScanXML: data: cnt " << cnt << endl);
            }            

            m_docBuf.write(buf, cnt);
            return true;
        }

    private:
        XSLTInputSource m_doc;
        stringstream m_docBuf;
        string m_fn;
    };

#else
    class FileScanXML : public FileScanDo {
    public:
        FileScanXML(const string& fn) : m_fn(fn) {}
        virtual ~FileScanXML() {
            if (ctxt) {
                xmlFreeParserCtxt(ctxt);
#ifndef _WIN32
                malloc_trim(0);
#endif
            }
        }

        xmlDocPtr getDoc() {
            int ret;
            if ((ret = xmlParseChunk(ctxt, nullptr, 0, 1))) {
                xmlError *error = xmlGetLastError();
                LOGERR("FileScanXML: final xmlParseChunk failed with error " <<
                    ret << " error: " <<
                    (error ? error->message :
                        " null return from xmlGetLastError()") << "\n");
                return nullptr;
            }
            return ctxt->myDoc;
        }

        virtual bool init(int64_t size, string *) {
            LOGDEB1("FileScanXML: init: size " << size << endl);
            ctxt = xmlCreatePushParserCtxt(NULL, NULL, NULL, 0, m_fn.c_str());
            if (ctxt == nullptr) {
                LOGERR("FileScanXML: xmlCreatePushParserCtxt failed\n");
                return false;
            } else {
                return true;
            }
        }
        
        virtual bool data(const char *buf, int cnt, string*) {
            if (0) {
                string dt(buf, cnt);
                LOGDEB1("FileScanXML: data: cnt " << cnt << " data " << dt << endl);
            } else {
                LOGDEB1("FileScanXML: data: cnt " << cnt << endl);
            }            
            int ret;
            if ((ret = xmlParseChunk(ctxt, buf, cnt, 0))) {
                xmlError *error = xmlGetLastError();
                LOGERR("FileScanXML: xmlParseChunk failed with error " <<
                    ret << " for [" << buf << "] error " <<
                    (error ? error->message :
                        " null return from xmlGetLastError()") << "\n");
                return false;
            } else {
                LOGDEB1("xmlParseChunk ok (sent " << cnt << " bytes)\n");
                return true;
            }
        }

    private:
        xmlParserCtxtPtr ctxt{nullptr};
        string m_fn;
    };
#endif

class MimeHandlerXslt::Internal {
public:
    Internal(MimeHandlerXslt *_p)
        : p(_p) {}
    ~Internal() {
<<<<<<< HEAD
        if (metaOrAllSS) {
#ifdef USE_XALAN            
            transformer.destroyStylesheet(metaOrAllSS);
#else
            xsltFreeStylesheet(metaOrAllSS);
#endif            
        }
        if (bodySS) {
#ifdef USE_XALAN
            transformer.destroyStylesheet(bodySS);
#else
            xsltFreeStylesheet(bodySS);
#endif            
=======
        for (auto& entry : metaOrAllSS) {
            xsltFreeStylesheet(entry.second);
        }
        for (auto& entry : bodySS) {
            xsltFreeStylesheet(entry.second);
>>>>>>> upstream/master
        }
    }
    
    XSLTStyleSheet *prepare_stylesheet(const string& ssnm);

    bool process_doc_or_string(bool forpv, const string& fn, const string& data);
    bool apply_stylesheet(
        const string& fn, const string& member, const string& data,
        XSLTStyleSheet *ssp, string& result, string *md5p);

    MimeHandlerXslt *p;
    bool ok{false};
<<<<<<< HEAD
    string metamember;

#ifdef USE_XALAN
    XalanTransformer    transformer;
#endif    
    XSLTStyleSheet *metaOrAllSS{nullptr};
    XSLTStyleSheet *bodySS{nullptr};

    string bodymember;
=======

    // Pairs of zip archive member names and style sheet names for the
    // metadata, and map of style sheets refd by their names.
    // Exception: there can be a single entry which does meta and
    // body, in which case bodymembers/bodySS are empty.
    vector<pair<string,string>> metaMembers;
    map <string, xsltStylesheet*> metaOrAllSS;
    // Same for body data
    vector<pair<string,string>> bodyMembers;
    map<string, xsltStylesheet*> bodySS;
>>>>>>> upstream/master
    string result;
    string filtersdir;
};

 
MimeHandlerXslt::~MimeHandlerXslt()
{
    delete m;
}
bool MimeHandlerXslt::m_isInit=false;

MimeHandlerXslt::MimeHandlerXslt(RclConfig *cnf, const std::string& id,
                                 const std::vector<std::string>& params)
    : RecollFilter(cnf, id)
{
#ifdef USE_XALAN
    pthread_once(&thread_once_control, xalan_init);
#else
    xmlSubstituteEntitiesDefault(0);
    xmlLoadExtDtdDefaultValue = 0;
#endif
    m=new Internal(this);
    LOGDEB("MimeHandlerXslt: params: " << stringsToString(params) << endl);
    m->filtersdir = path_cat(cnf->getDatadir(), "filters");
    // params can be "xslt stylesheetall" or
    // "xslt meta/body memberpath stylesheetnm [... ... ...] ...
    if (params.size() == 2) {
        auto ss = m->prepare_stylesheet(params[1]);
        if (ss) {
            m->ok = true;
            m->metaOrAllSS[""] = ss;
        }
    } else if (params.size() > 3 && params.size() % 3 == 1) {
        auto it = params.begin();
        it++;
        while (it != params.end()) {
            // meta/body membername ssname
            const string& tp = *it++;
            const string& znm = *it++;
            const string& ssnm = *it++;
            vector<pair<string,string>> *mbrv;
            map<string,xsltStylesheet*> *ssmp;
            if (tp == "meta") {
                mbrv = &m->metaMembers;
                ssmp = &m->metaOrAllSS;
            } else if (tp == "body") {
                mbrv = &m->bodyMembers;
                ssmp = &m->bodySS;
            } else {
                LOGERR("MimeHandlerXslt: bad member type " << tp << endl);
                return;
            }
            if (ssmp->find(ssnm) == ssmp->end()) {
                auto ss = m->prepare_stylesheet(ssnm);
                if (nullptr == ss) {
                    return;
                }
                ssmp->insert({ssnm, ss});
            }
            mbrv->push_back({znm, ssnm});
        }
        m->ok = true;
    } else {
        LOGERR("MimeHandlerXslt: constructor with wrong param vector: " <<
               stringsToString(params) << endl);
    }
}

XSLTStyleSheet *MimeHandlerXslt::Internal::prepare_stylesheet(const string& ssnm)
{
    string ssfn = path_cat(filtersdir, ssnm);

#ifdef USE_XALAN    
    const XSLTInputSource   stylesheetInputSource(ssfn.c_str());
    const XalanCompiledStylesheet*      stylesheet = 0;

    if (transformer.compileStylesheet(stylesheetInputSource, stylesheet) != 0)
    {
        LOGERR("An error occurred compiling the stylesheet: "
                << transformer.getLastError()
                << endl);
        return nullptr;
    }
    return stylesheet;
#else
    FileScanXML XMLstyle(ssfn);
    string reason;
    if (!file_scan(ssfn, &XMLstyle, &reason)) {
        LOGERR("MimeHandlerXslt: file_scan failed for style sheet " <<
               ssfn << " : " << reason << endl);
        return nullptr;
    }
    xmlDoc *stl = XMLstyle.getDoc();
    if (stl == nullptr) {
        LOGERR("MimeHandlerXslt: getDoc failed for style sheet " <<
               ssfn << endl);
        return nullptr;
    }
    return xsltParseStylesheetDoc(stl);
#endif    
}

bool MimeHandlerXslt::Internal::apply_stylesheet(
    const string& fn, const string& member, const string& data,
    XSLTStyleSheet *ssp, string& result, string *md5p)
{
    FileScanXML XMLdoc(fn);
    string md5, reason;
    bool res;
    if (!fn.empty()) {
        if (member.empty()) {
            res = file_scan(fn, &XMLdoc, 0, -1, &reason, md5p);
        } else {
            res = file_scan(fn, member, &XMLdoc, &reason);
        }
    } else {
        if (member.empty()) {
            res = string_scan(data.c_str(), data.size(), &XMLdoc, &reason, md5p);
        } else {
            res = string_scan(data.c_str(), data.size(), member, &XMLdoc,
                              &reason);
        }
    }
    if (!res) {
        LOGERR("MimeHandlerXslt::set_document_: file_scan failed for "<<
               fn << " " << member << " : " << reason << endl);
        return false;
    }
#ifdef USE_XALAN
    XSLTInputSource doc=XMLdoc.getDoc();
    stringstream ss;
    const XSLTResultTarget  resultTarget(ss);

    int ret = transformer.transform(doc, ssp, resultTarget);

    result=ss.str();

    return ret == 0;

#else

    xmlDocPtr doc = XMLdoc.getDoc();
    if (nullptr == doc) {
        LOGERR("MimeHandlerXslt::set_document_: no parsed doc\n");
        return false;
    }
    xmlDocPtr transformed = xsltApplyStylesheet(ssp, doc, NULL);
    if (nullptr == transformed) {
        LOGERR("MimeHandlerXslt::set_document_: xslt transform failed\n");
        xmlFreeDoc(doc);
        return false;
    }
    xmlChar *outstr;
    int outlen;
    xsltSaveResultToString(&outstr, &outlen, transformed, ssp);
    result = string((const char*)outstr, outlen);
    xmlFree(outstr);
    xmlFreeDoc(transformed);
    xmlFreeDoc(doc);
    return true;
#endif    
}

bool MimeHandlerXslt::Internal::process_doc_or_string(
    bool forpreview, const string& fn, const string& data)
{
    p->m_metaData[cstr_dj_keycharset] = cstr_utf8;
    if (bodySS.empty()) {
        auto ssp = metaOrAllSS.find("");
        if (ssp == metaOrAllSS.end()) {
            LOGERR("MimeHandlerXslt::process: no style sheet !\n");
            return false;
        }
        string md5;
        if (apply_stylesheet(fn, string(), data, ssp->second, result,
                             forpreview ? nullptr : &md5)) {
            if (!forpreview) {
                p->m_metaData[cstr_dj_keymd5] = md5;
            }
            return true;
        }
        return false;
    } else {
        result = "<html>\n<head>\n<meta http-equiv=\"Content-Type\""
            "content=\"text/html; charset=UTF-8\">";
        for (auto& member : metaMembers) {
            auto it = metaOrAllSS.find(member.second);
            if (it == metaOrAllSS.end()) {
                LOGERR("MimeHandlerXslt::process: no style sheet found for " <<
                       member.first << ":" << member.second << "!\n");
                return false;
            }
            string part;
            if (!apply_stylesheet(fn, member.first, data, it->second, part, nullptr)) {
                return false;
            }
            result += part;
        }
        result += "</head>\n<body>\n";
        
        for (auto& member : bodyMembers) {
            auto it = bodySS.find(member.second);
            if (it == bodySS.end()) {
                LOGERR("MimeHandlerXslt::process: no style sheet found for " <<
                       member.first << ":" << member.second << "!\n");
                return false;
            }
            string part;
            if (!apply_stylesheet(fn, member.first, data, it->second, part, nullptr)) {
                return false;
            }
            result += part;
        }
        result += "</body></html>";
    }
    return true;
}

bool MimeHandlerXslt::set_document_file_impl(const string&, const string &fn)
{
    LOGDEB0("MimeHandlerXslt::set_document_file_: fn: " << fn << endl);
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, fn, string());
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::set_document_string_impl(const string&, const string& txt)
{
    LOGDEB0("MimeHandlerXslt::set_document_string_\n");
    if (!m || !m->ok) {
        return false;
    }
    bool ret = m->process_doc_or_string(m_forPreview, string(), txt);
    if (ret) {
        m_havedoc = true;
    }
    return ret;
}

bool MimeHandlerXslt::next_document()
{
    if (!m || !m->ok) {
        return false;
    }
    if (m_havedoc == false)
        return false;
    m_havedoc = false;
    m_metaData[cstr_dj_keymt] = cstr_texthtml;
    m_metaData[cstr_dj_keycontent].swap(m->result);
    LOGDEB1("MimeHandlerXslt::next_document: result: [" <<
            m_metaData[cstr_dj_keycontent] << "]\n");
    return true;
}

void MimeHandlerXslt::clear_impl()
{
    m_havedoc = false;
    m->result.clear();
}
