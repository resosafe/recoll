#include <string>
#include <iostream>
#include <stack>

#include <QStandardItemModel>

#include "idxmodel.h"

#include "recoll.h"
#include "rclconfig.h"
#include "fstreewalk.h"
#include "log.h"

#undef USE_TREEWALK

class WalkerCB : public FsTreeWalkerCB {
public:
    WalkerCB(RclConfig *config, const std::string& topstring, FsTreeWalker& walker,
             IdxTreeModel *model, const QModelIndex& index)
        : m_config(config), m_topstring(topstring), m_walker(walker), m_model(model)
    {
        m_indexes.push(index);
        m_rows.push(0);
    }
    virtual FsTreeWalker::Status processone(
        const std::string& path, const struct PathStat *, FsTreeWalker::CbFlag flg) override;

    RclConfig *m_config;
    std::string m_topstring;
    FsTreeWalker& m_walker;
    IdxTreeModel *m_model;
    std::stack<QModelIndex> m_indexes;
    std::stack<int> m_rows;
};

FsTreeWalker::Status WalkerCB::processone(
    const std::string& path, const struct PathStat *, FsTreeWalker::CbFlag flg)
{
    if (flg == FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwDirReturn) {
        m_config->setKeyDir(path);
        // Set up filter/skipped patterns for this subtree. 
        m_walker.setOnlyNames(m_config->getOnlyNames());
        m_walker.setSkippedNames(m_config->getSkippedNames());
    }
    if (flg == FsTreeWalker::FtwDirReturn) {
        m_indexes.pop();
        m_rows.pop();
        return FsTreeWalker::FtwOk;
    }
    if (flg == FsTreeWalker::FtwDirEnter) {
        //std::cerr << "ENTER: " << path << "\n";
        if (m_model->columnCount(m_indexes.top()) == 0) {
            if (!m_model->insertColumn(0, m_indexes.top()))
                return FsTreeWalker::FtwError;
        }
        if (!m_model->insertRow(m_rows.top(), m_indexes.top()))
            return FsTreeWalker::FtwError;
        const QModelIndex child = m_model->index(m_rows.top(), 0, m_indexes.top());
        // Setting the short path in DisplayRole and the real one in EditRole does not seem to work,
        // the treeview shows the EditRole?? So use the ToolTip to store the full value
        std::string disp;
        if (m_topstring.empty()) {
            disp = path_getsimple(path);
        } else {
            disp = m_topstring;
            m_topstring.clear();
        }
        m_model->setData(child, QVariant(path2qs(disp)), Qt::DisplayRole);
        m_model->setData(child, QVariant(path2qs(path)), Qt::ToolTipRole);
        ++m_rows.top();
        m_indexes.push(child);
        m_rows.push(0);
    }
    return FsTreeWalker::FtwOk;
}

#ifdef USE_TREEWALK
static void populateDir(RclConfig *config, const std::string& topstr, IdxTreeModel *model,
                        const QModelIndex& index, const std::string& path, int depth)
{
    FsTreeWalker walker;
    walker.setSkippedPaths(config->getSkippedPaths());
//  walker.setOpts(walker.getOpts() | FsTreeWalker::FtwSkipDotFiles);
    walker.setMaxDepth(depth);
    WalkerCB cb(config, topstr, walker, model, index);
    walker.walk(path, cb);
}

#else

// Assemble a path from its components up to lst
std::string toksToPath(std::vector<std::string>& path, int lst)
{
    std::string out;
    for (int i = 0; i <= lst; i++) {
        out += "/" + path[i];
    }
    if (out.empty())
        out = "/";
    return out;
}

// Process a sorted list of directory paths, generating a sequence of enter/exit calls equivalent to
// what would happen for a recursive tree walk of the original tree.
static void treelist(const std::string& top, const std::vector<std::string>& lst, WalkerCB &cb)
{
    if (lst.empty()) {
        return;
    }
    std::vector<std::string> curpath;
    stringToTokens(top, curpath, "/");
    LOGDEB0("treelist: " << "top " << top << " TOP depth is " << curpath.size() << "\n");
    for (const auto& dir : lst) {
        LOGDEB1("DIR: " << dir << "\n");
        std::vector<std::string> npath;
        // Compute the new directory stack
        stringToTokens(dir, npath, "/");
        // Walk the stacks until we find a differing entry, and then unwind the old stack to the new
        // base, and issue enter calls for new entries over the base.
        int i = 0;
        for (; i < int(std::min(curpath.size(), npath.size())); i++) {
            if (npath[i] != curpath[i] && int(curpath.size()) > 0) {
                // Differing at i, unwind old stack and break the main loop
                for (int j = int(curpath.size()) - 1; j >= i; j--) {
                    //std::cerr << "Exiting  " <<  toksToPath(curpath, j) << "\n";
                    cb.processone(toksToPath(curpath, j), nullptr, FsTreeWalker::FtwDirReturn);
                }
                break;
            }
        }
        // Callbacks for new entries above the base.
        for (int j = i; j < int(npath.size()); j++) {
            //std::cerr << "Entering " << toksToPath(npath, j) << "\n";
            cb.processone(toksToPath(npath, j), nullptr, FsTreeWalker::FtwDirEnter);
        }
        curpath.swap(npath);
    }
}
#endif // USE_TREEWALK

void IdxTreeModel::populate()
{
    LOGDEB0("IdxTreeModel::populate\n");
    QModelIndex index = this->index(0,0);
    if (this->columnCount(index) == 0) {
        if (!this->insertColumn(0, index))
            return;
    }
    int row = 0;

#ifdef USE_TREEWALK
    auto topdirs = m_config->getTopdirs();
    auto prefix = commonprefix(topdirs);
    for (const auto& topdir : topdirs) {
        const QModelIndex child = this->index(row, 0, index);
        std::string topdisp;
        if (prefix.size() > 1) {
            topdisp = topdir.substr(prefix.size());
        } else {
            topdisp = topdir;
        }
        populateDir(m_config, topdisp, this, child, topdir, m_depth);
        ++row;
    }
    sort(0, Qt::AscendingOrder);
#else
    std::vector<std::string> thedirs;
    std::string prefix;
    rcldb->dirlist(m_depth, prefix, thedirs);
    LOGDEB0("IdxTreeModel::populate: prefix [" << prefix << "] thedirs: " <<
            stringsToString(thedirs) << "\n");
    const QModelIndex child = this->index(row, 0, index);
    FsTreeWalker walker;
    WalkerCB cb(m_config, prefix == "/" ? std::string() : prefix, walker, this, child);
    if (prefix.empty())
        prefix = "/";
    treelist(path_getfather(prefix), thedirs, cb);
#endif
}
