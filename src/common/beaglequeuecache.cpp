#include "autoconfig.h"

#include "beaglequeuecache.h"
#include "circache.h"
#include "debuglog.h"
#include "rclconfig.h"
#include "pathut.h"
#include "rcldoc.h"

BeagleQueueCache::BeagleQueueCache(RclConfig *cnf) 
{
    string ccdir;
    cnf->getConfParam("webcachedir", ccdir);
    if (ccdir.empty())
        ccdir = "webcache";
    ccdir = path_tildexpand(ccdir);
    // If not an absolute path, compute relative to config dir
    if (ccdir.at(0) != '/')
        ccdir = path_cat(cnf->getConfDir(), ccdir);

    int maxmbs = 40;
    cnf->getConfParam("webcachemaxmbs", &maxmbs);
    m_cache = new CirCache(ccdir);
    m_cache->create(off_t(maxmbs)*1000*1024, CirCache::CC_CRUNIQUE);
}

BeagleQueueCache::~BeagleQueueCache()
{
    delete m_cache;
}

// Read  document from cache. Return the metadata as an Rcl::Doc
// @param htt Beagle Hit Type 
bool BeagleQueueCache::getFromCache(const string& udi, Rcl::Doc &dotdoc, 
                                      string& data, string *htt)
{
    string dict;

    if (!m_cache->get(udi, dict, data))
        return false;

    ConfSimple cf(dict, 1);
    
    if (htt)
        cf.get(Rcl::Doc::keybght, *htt, "");

    // Build a doc from saved metadata 
    cf.get("url", dotdoc.url, "");
    cf.get("mimetype", dotdoc.mimetype, "");
    cf.get("fmtime", dotdoc.fmtime, "");
    cf.get("fbytes", dotdoc.fbytes, "");
    dotdoc.sig = "";
    list<string> names = cf.getNames("");
    for (list<string>::const_iterator it = names.begin();
         it != names.end(); it++) {
        cf.get(*it, dotdoc.meta[*it], "");
    }
    dotdoc.meta[Rcl::Doc::keyudi] = udi;
    return true;
}
