#include "autoconfig.h"
#ifdef RCL_MONITOR
/* Copyright (C) 2006-2021 J.F.Dockes 
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

/* The code for the Win32 version of the monitor was largely copied from efsw:
 * https://github.com/SpartanJ/efsw
 * LICENSE for the original WIN32 code:
 * Copyright (c) 2020 Martín Lucas Golini
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * This software is a fork of the "simplefilewatcher" by James Wynn (james@jameswynn.com)
 * http://code.google.com/p/simplefilewatcher/ also MIT licensed.
 */


#include "autoconfig.h"

#include <errno.h>
#include <cstdio>
#include <cstring>
#include "safeunistd.h"

#include "log.h"
#include "rclmon.h"
#include "rclinit.h"
#include "fstreewalk.h"
#include "pathut.h"

/**
 * Recoll real time monitor event receiver. This file has code to interface 
 * to FAM, inotify, etc. and place events on the event queue.
 */

/** Virtual interface for the actual filesystem monitoring module. */
class RclMonitor {
public:
    RclMonitor() {}
    virtual ~RclMonitor() {}

    virtual bool addWatch(const string& path, bool isDir) = 0;
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1) = 0;
    virtual bool ok() const = 0;
    // Does this monitor generate 'exist' events at startup?
    virtual bool generatesExist() const {
        return false;
    }
    virtual bool isRecursive() const {
        return false;
    }
    // Save significant errno after monitor calls
    int saved_errno{0};
};

// Monitor factory. We only have one compiled-in kind at a time, no
// need for a 'kind' parameter
static RclMonitor *makeMonitor();

/** 
 * Create directory watches during the initial file system tree walk.
 *
 * This class is a callback for the file system tree walker
 * class. The callback method alternatively creates the directory
 * watches and flushes the event queue (to avoid a possible overflow
 * while we create the watches)
 */
class WalkCB : public FsTreeWalkerCB {
public:
    WalkCB(RclConfig *conf, RclMonitor *mon, RclMonEventQueue *queue, FsTreeWalker& walker)
        : m_config(conf), m_mon(mon), m_queue(queue), m_walker(walker) {}
    virtual ~WalkCB() {}

    virtual FsTreeWalker::Status processone(
        const string &fn, const struct PathStat *st, FsTreeWalker::CbFlag flg) {
        MONDEB("walkerCB: processone " << fn <<  " m_mon " << m_mon <<
               " m_mon->ok " << (m_mon ? m_mon->ok() : false) << "\n");

        if (flg == FsTreeWalker::FtwDirEnter || flg == FsTreeWalker::FtwDirReturn) {
            m_config->setKeyDir(fn);
            // Set up skipped patterns for this subtree. 
            m_walker.setSkippedNames(m_config->getSkippedNames());
        }

        if (flg == FsTreeWalker::FtwDirEnter) {
            // Create watch when entering directory, but first empty
            // whatever events we may already have on queue
            while (m_queue->ok() && m_mon->ok()) {
                RclMonEvent ev;
                if (m_mon->getEvent(ev, 0)) {
                    if (ev.m_etyp !=  RclMonEvent::RCLEVT_NONE)
                        m_queue->pushEvent(ev);
                } else {
                    MONDEB("walkerCB: no event pending\n");
                    break;
                }
            }
            if (!m_mon || !m_mon->ok())
                return FsTreeWalker::FtwError;
            // We do nothing special if addWatch fails for a reasonable reason
            if (!m_mon->isRecursive() && !m_mon->addWatch(fn, true)) {
                if (m_mon->saved_errno != EACCES && m_mon->saved_errno != ENOENT) {
                    LOGINF("walkerCB: addWatch failed\n");
                    return FsTreeWalker::FtwError;
                }
            }
        } else if (!m_mon->generatesExist() && flg == FsTreeWalker::FtwRegular) {
            // Have to synthetize events for regular files existence
            // at startup because the monitor does not do it
            // Note 2011-09-29: no sure this is actually needed. We just ran
            //  an incremental indexing pass (before starting the
            //  monitor). Why go over the files once more ? The only
            //  reason I can see would be to catch modifications that
            //  happen between the incremental and the start of
            //  monitoring ? There should be another way: maybe start
            //  monitoring without actually handling events (just
            //  queue), then run incremental then start handling
            //  events ? ** But we also have to do it on a directory
            //  move! So keep it ** We could probably skip it on the initial run though.
            RclMonEvent ev;
            ev.m_path = fn;
            ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
            m_queue->pushEvent(ev);
        }
        return FsTreeWalker::FtwOk;
    }

private:
    RclConfig         *m_config;
    RclMonitor        *m_mon;
    RclMonEventQueue  *m_queue;
    FsTreeWalker&      m_walker;
};

static bool rclMonAddTopWatches(
    FsTreeWalker& walker, RclConfig& lconfig, RclMonitor *mon, RclMonEventQueue *queue)
{
    // Get top directories from config. Special monitor sublist if
    // set, else full list.
    vector<string> tdl = lconfig.getTopdirs(true);
    if (tdl.empty()) {
        LOGERR("rclMonRcvRun:: top directory list (topdirs param.) not found "
               "in configuration or topdirs list parse error");
        queue->setTerminate();
        return false;
    }
    // Walk the directory trees to add watches
    WalkCB walkcb(&lconfig, mon, queue, walker);
    for (const auto& dir : tdl) {
        lconfig.setKeyDir(dir);
        // Adjust the follow symlinks options
        bool follow;
        if (lconfig.getConfParam("followLinks", &follow) && follow) {
            walker.setOpts(FsTreeWalker::FtwFollow);
        } else {
            walker.setOpts(FsTreeWalker::FtwOptNone);
        }
        if (path_isdir(dir, follow)) {
            LOGDEB("rclMonRcvRun: walking "  << dir << "\n");
            // If the fs watcher is recursive, we add the watches for the topdirs here, and walk the
            // tree just for generating initial events.
            if (mon->isRecursive() && !mon->addWatch(dir, true)) {
                if (mon->saved_errno != EACCES && mon->saved_errno != ENOENT) {
                    LOGERR("rclMonAddTopWatches: addWatch failed for [" << dir << "]\n");
                    return false;
                }
            }
            if (walker.walk(dir, walkcb) != FsTreeWalker::FtwOk) {
                LOGERR("rclMonRcvRun: tree walk failed\n");
                return false;
            }
            if (walker.getErrCnt() > 0) {
                LOGINFO("rclMonRcvRun: fs walker errors: " << walker.getReason() << "\n");
            }
        } else {
            // We have to special-case regular files which are part of the topdirs list because the
            // tree walker only adds watches for directories
            if (!mon->addWatch(dir, false)) {
                LOGSYSERR("rclMonRcvRun", "addWatch", dir);
            }
        }
    }

    bool doweb = false;
    lconfig.getConfParam("processwebqueue", &doweb);
    if (doweb) {
        string webqueuedir = lconfig.getWebQueueDir();
        if (!mon->addWatch(webqueuedir, true)) {
            LOGERR("rclMonRcvRun: addwatch (webqueuedir) failed\n");
            if (mon->saved_errno != EACCES && mon->saved_errno != ENOENT)
                return false;
        }
    }
    return true;
}

static bool rclMonAddSubWatches(
    const std::string& path, FsTreeWalker& walker, RclConfig& lconfig,
    RclMonitor *mon, RclMonEventQueue *queue)
{
    WalkCB walkcb(&lconfig, mon, queue, walker);
    if (walker.walk(path, walkcb) != FsTreeWalker::FtwOk) {
        LOGERR("rclMonRcvRun: walking new dir " << path << " : " << walker.getReason() << "\n");
        return false;
    }
    if (walker.getErrCnt() > 0) {
        LOGINFO("rclMonRcvRun: fs walker errors: " << walker.getReason() << "\n");
    }
    return true;
}

// Don't push events for skipped files. This would get filtered on the processing side
// anyway, but causes unnecessary wakeups and messages. Do not test skippedPaths here,
// this would be incorrect (because a topdir can be under a skippedPath and this was
// handled while adding the watches). Also we let the other side process onlyNames.
static bool rclMonShouldSkip(const std::string& path, RclConfig& lconfig, FsTreeWalker& walker)
{
    lconfig.setKeyDir(path_getfather(path));
    walker.setSkippedNames(lconfig.getSkippedNames());
    if (walker.inSkippedNames(path_getsimple(path)))
        return true;
    return false;
}

// Main thread routine: create watches, then forever wait for and queue events
void *rclMonRcvRun(void *q)
{
    RclMonEventQueue *queue = (RclMonEventQueue *)q;

    LOGDEB("rclMonRcvRun: running\n");
    recoll_threadinit();
    // Make a local copy of the configuration as it doesn't like
    // concurrent accesses. It's ok to copy it here as the other
    // thread will not work before we have sent events.
    RclConfig lconfig(*queue->getConfig());

    // Create the fam/whatever interface object
    RclMonitor *mon;
    if ((mon = makeMonitor()) == 0) {
        LOGERR("rclMonRcvRun: makeMonitor failed\n");
        queue->setTerminate();
        return 0;
    }

    FsTreeWalker walker;
    walker.setSkippedPaths(lconfig.getDaemSkippedPaths());

    if (!rclMonAddTopWatches(walker, lconfig, mon, queue)) {
        LOGERR("rclMonRcvRun: addtopwatches failed\n");
        goto terminate;
    }

    // Forever wait for monitoring events and add them to queue:
    MONDEB("rclMonRcvRun: waiting for events. q->ok(): " << queue->ok() << "\n");
    while (queue->ok() && mon->ok()) {
        RclMonEvent ev;
        // Note: I could find no way to get the select call to return when a signal is delivered to
        // the process (it goes to the main thread, from which I tried to close or write to the
        // select fd, with no effect). So set a timeout so that an intr will be detected
        if (mon->getEvent(ev, 2000)) {
            if (rclMonShouldSkip(ev.m_path, lconfig, walker))
                continue;

            if (ev.m_etyp == RclMonEvent::RCLEVT_DIRCREATE) {
                // Recursive addwatch: there may already be stuff inside this directory. E.g.: files
                // were quickly created, or this is actually the target of a directory move. This is
                // necessary for inotify, but it seems that fam/gamin is doing the job for us so
                // that we are generating double events here (no big deal as prc will sort/merge).
                LOGDEB("rclMonRcvRun: walking new dir " << ev.m_path << "\n");
                if (!rclMonAddSubWatches(ev.m_path, walker, lconfig, mon, queue)) {
                    goto terminate;
                }
            }

            if (ev.m_etyp !=  RclMonEvent::RCLEVT_NONE)
                queue->pushEvent(ev);
        }
    }

terminate:
    queue->setTerminate();
    LOGINFO("rclMonRcvRun: monrcv thread routine returning\n");
    return 0;
}

// Utility routine used by both the fam/gamin and inotify versions to get 
// rid of the id-path translation for a moved dir
bool eraseWatchSubTree(map<int, string>& idtopath, const string& top)
{
    bool found = false;
    MONDEB("Clearing map for [" << top << "]\n");
    map<int,string>::iterator it = idtopath.begin();
    while (it != idtopath.end()) {
        if (it->second.find(top) == 0) {
            found = true;
            it = idtopath.erase(it);
        } else {
            it++;
        }
    }
    return found;
}

// We dont compile both the inotify and the fam interface and inotify
// has preference
#ifndef RCL_USE_INOTIFY
#ifdef RCL_USE_FAM
//////////////////////////////////////////////////////////////////////////
/** Fam/gamin -based monitor class */
#include <fam.h>
#include <sys/select.h>
#include <setjmp.h>
#include <signal.h>

/** FAM based monitor class. We have to keep a record of FAM watch
    request numbers to directory names as the event only contain the
    request number and file name, not the full path */
class RclFAM : public RclMonitor {
public:
    RclFAM();
    virtual ~RclFAM();
    virtual bool addWatch(const string& path, bool isdir);
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1);
    bool ok() const {return m_ok;}
    virtual bool generatesExist() const {return true;}

private:
    bool m_ok;
    FAMConnection m_conn;
    void close() {
        FAMClose(&m_conn);
        m_ok = false;
    }
    map<int,string> m_idtopath;
    const char *event_name(int code);
};

// Translate event code to string (debug)
const char *RclFAM::event_name(int code)
{
    static const char *famevent[] = {
        "",
        "FAMChanged",
        "FAMDeleted",
        "FAMStartExecuting",
        "FAMStopExecuting",
        "FAMCreated",
        "FAMMoved",
        "FAMAcknowledge",
        "FAMExists",
        "FAMEndExist"
    };
    static char unknown_event[30];
 
    if (code < FAMChanged || code > FAMEndExist) {
        sprintf(unknown_event, "unknown (%d)", code);
        return unknown_event;
    }
    return famevent[code];
}

RclFAM::RclFAM()
    : m_ok(false)
{
    if (FAMOpen2(&m_conn, "Recoll")) {
        LOGERR("RclFAM::RclFAM: FAMOpen2 failed, errno " << errno << "\n");
        return;
    }
    m_ok = true;
}

RclFAM::~RclFAM()
{
    if (ok())
        FAMClose(&m_conn);
}

static jmp_buf jbuf;
static void onalrm(int sig)
{
    longjmp(jbuf, 1);
}
bool RclFAM::addWatch(const string& path, bool isdir)
{
    if (!ok())
        return false;
    bool ret = false;

    MONDEB("RclFAM::addWatch: adding " << path << "\n");

    // It happens that the following call block forever. 
    // We'd like to be able to at least terminate on a signal here, but
    // gamin forever retries its write call on EINTR, so it's not even useful
    // to unblock signals. SIGALRM is not used by the main thread, so at least
    // ensure that we exit after gamin gets stuck.
    if (setjmp(jbuf)) {
        LOGERR("RclFAM::addWatch: timeout talking to FAM\n");
        return false;
    }
    signal(SIGALRM, onalrm);
    alarm(20);
    FAMRequest req;
    if (isdir) {
        if (FAMMonitorDirectory(&m_conn, path.c_str(), &req, 0) != 0) {
            LOGERR("RclFAM::addWatch: FAMMonitorDirectory failed\n");
            goto out;
        }
    } else {
        if (FAMMonitorFile(&m_conn, path.c_str(), &req, 0) != 0) {
            LOGERR("RclFAM::addWatch: FAMMonitorFile failed\n");
            goto out;
        }
    }
    m_idtopath[req.reqnum] = path;
    ret = true;

out:
    alarm(0);
    return ret;
}

// Note: return false only for queue empty or error 
// Return EVT_NONE for bad event to keep queue processing going
bool RclFAM::getEvent(RclMonEvent& ev, int msecs)
{
    if (!ok())
        return false;
    MONDEB("RclFAM::getEvent:\n");

    fd_set readfds;
    int fam_fd = FAMCONNECTION_GETFD(&m_conn);
    FD_ZERO(&readfds);
    FD_SET(fam_fd, &readfds);

    MONDEB("RclFAM::getEvent: select. fam_fd is " << fam_fd << "\n");
    // Fam / gamin is sometimes a bit slow to send events. Always add
    // a little timeout, because if we fail to retrieve enough events,
    // we risk deadlocking in addwatch()
    if (msecs == 0)
        msecs = 2;
    struct timeval timeout;
    if (msecs >= 0) {
        timeout.tv_sec = msecs / 1000;
        timeout.tv_usec = (msecs % 1000) * 1000;
    }
    int ret;
    if ((ret=select(fam_fd+1, &readfds, 0, 0, msecs >= 0 ? &timeout : 0)) < 0) {
        LOGERR("RclFAM::getEvent: select failed, errno " << errno << "\n");
        close();
        return false;
    } else if (ret == 0) {
        // timeout
        MONDEB("RclFAM::getEvent: select timeout\n");
        return false;
    }

    MONDEB("RclFAM::getEvent: select returned " << ret << "\n");

    if (!FD_ISSET(fam_fd, &readfds))
        return false;

    // ?? 2011/03/15 gamin v0.1.10. There is initially a single null
    // byte on the connection so the first select always succeeds. If
    // we then call FAMNextEvent we stall. Using FAMPending works
    // around the issue, but we did not need this in the past and this
    // is most weird.
    if (FAMPending(&m_conn) <= 0) {
        MONDEB("RclFAM::getEvent: FAMPending says no events\n");
        return false;
    }

    MONDEB("RclFAM::getEvent: call FAMNextEvent\n");
    FAMEvent fe;
    if (FAMNextEvent(&m_conn, &fe) < 0) {
        LOGERR("RclFAM::getEvent: FAMNextEvent: errno " << errno << "\n");
        close();
        return false;
    }
    MONDEB("RclFAM::getEvent: FAMNextEvent returned\n");
    
    map<int,string>::const_iterator it;
    if ((!path_isabsolute(fe.filename)) && 
        (it = m_idtopath.find(fe.fr.reqnum)) != m_idtopath.end()) {
        ev.m_path = path_cat(it->second, fe.filename);
    } else {
        ev.m_path = fe.filename;
    }

    MONDEB("RclFAM::getEvent: " << event_name(fe.code) < " " << ev.m_path << "\n");

    switch (fe.code) {
    case FAMCreated:
        if (path_isdir(ev.m_path)) {
            ev.m_etyp = RclMonEvent::RCLEVT_DIRCREATE;
            break;
        }
        /* FALLTHROUGH */
    case FAMChanged:
    case FAMExists:
        // Let the other side sort out the status of this file vs the db
        ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
        break;

    case FAMMoved: 
    case FAMDeleted:
        ev.m_etyp = RclMonEvent::RCLEVT_DELETE;
        // We would like to signal a directory here to enable cleaning
        // the subtree (on a dir move), but can't test the actual file
        // which is gone, and fam doesn't tell us if it's a dir or reg. 
        // Let's rely on the fact that a directory should be watched
        if (eraseWatchSubTree(m_idtopath, ev.m_path)) 
            ev.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
        break;

    case FAMStartExecuting:
    case FAMStopExecuting:
    case FAMAcknowledge:
    case FAMEndExist:
    default:
        // Have to return something, this is different from an empty queue,
        // esp if we are trying to empty it...
        if (fe.code != FAMEndExist)
            LOGDEB("RclFAM::getEvent: got other event " << fe.code << "!\n");
        ev.m_etyp = RclMonEvent::RCLEVT_NONE;
        break;
    }
    return true;
}
#endif // RCL_USE_FAM
#endif // ! INOTIFY

#ifdef RCL_USE_INOTIFY
//////////////////////////////////////////////////////////////////////////
/** Inotify-based monitor class */
#include <sys/inotify.h>
#include <sys/select.h>

class RclIntf : public RclMonitor {
public:
    RclIntf()
        : m_ok(false), m_fd(-1), m_evp(0), m_ep(0) {
        if ((m_fd = inotify_init()) < 0) {
            LOGERR("RclIntf:: inotify_init failed, errno " << errno << "\n");
            return;
        }
        m_ok = true;
    }
    virtual ~RclIntf() {
        close();
    }

    virtual bool addWatch(const string& path, bool isdir);
    virtual bool getEvent(RclMonEvent& ev, int msecs = -1);
    bool ok() const {return m_ok;}

private:
    bool m_ok;
    int m_fd;
    map<int,string> m_idtopath; // Watch descriptor to name
#define EVBUFSIZE (32*1024)
    char m_evbuf[EVBUFSIZE]; // Event buffer
    char *m_evp; // Pointer to next event or 0
    char *m_ep;  // Pointer to end of events
    const char *event_name(int code);
    void close() {
        if (m_fd >= 0) {
            ::close(m_fd);
            m_fd = -1;
        }
        m_ok = false;
    }
};

const char *RclIntf::event_name(int code) 
{
    code &= ~(IN_ISDIR|IN_ONESHOT);
    switch (code) {
    case IN_ACCESS: return "IN_ACCESS";
    case IN_MODIFY: return "IN_MODIFY";
    case IN_ATTRIB: return "IN_ATTRIB";
    case IN_CLOSE_WRITE: return "IN_CLOSE_WRITE";
    case IN_CLOSE_NOWRITE: return "IN_CLOSE_NOWRITE";
    case IN_CLOSE: return "IN_CLOSE";
    case IN_OPEN: return "IN_OPEN";
    case IN_MOVED_FROM: return "IN_MOVED_FROM";
    case IN_MOVED_TO: return "IN_MOVED_TO";
    case IN_MOVE: return "IN_MOVE";
    case IN_CREATE: return "IN_CREATE";
    case IN_DELETE: return "IN_DELETE";
    case IN_DELETE_SELF: return "IN_DELETE_SELF";
    case IN_MOVE_SELF: return "IN_MOVE_SELF";
    case IN_UNMOUNT: return "IN_UNMOUNT";
    case IN_Q_OVERFLOW: return "IN_Q_OVERFLOW";
    case IN_IGNORED: return "IN_IGNORED";
    default: {
        static char msg[50];
        sprintf(msg, "Unknown event 0x%x", code);
        return msg;
    }
    };
}

bool RclIntf::addWatch(const string& path, bool)
{
    if (!ok())
        return false;
    MONDEB("RclIntf::addWatch: adding " << path << "\n");
    // CLOSE_WRITE is covered through MODIFY. CREATE is needed for mkdirs
    uint32_t mask = IN_MODIFY | IN_CREATE
        | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE
        // IN_ATTRIB used to be not needed to receive extattr
        // modification events, which was a bit weird because only ctime is
        // set, and now it is...
        | IN_ATTRIB
#ifdef IN_DONT_FOLLOW
        | IN_DONT_FOLLOW
#endif
#ifdef IN_EXCL_UNLINK
        | IN_EXCL_UNLINK
#endif
        ;
    int wd;
    if ((wd = inotify_add_watch(m_fd, path.c_str(), mask)) < 0) {
        saved_errno = errno;
        LOGERR("RclIntf::addWatch: inotify_add_watch failed. errno " <<
               saved_errno << "\n");
        if (errno == ENOSPC) {
            LOGERR("RclIntf::addWatch: ENOSPC error may mean that you should "
                   "increase the inotify kernel constants. See inotify(7)\n");
        }
        return false;
    }
    m_idtopath[wd] = path;
    return true;
}

// Note: return false only for queue empty or error 
// Return EVT_NONE for bad event to keep queue processing going
bool RclIntf::getEvent(RclMonEvent& ev, int msecs)
{
    if (!ok())
        return false;
    ev.m_etyp = RclMonEvent::RCLEVT_NONE;
    MONDEB("RclIntf::getEvent:\n");

    if (m_evp == 0) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(m_fd, &readfds);
        struct timeval timeout;
        if (msecs >= 0) {
            timeout.tv_sec = msecs / 1000;
            timeout.tv_usec = (msecs % 1000) * 1000;
        }
        int ret;
        MONDEB("RclIntf::getEvent: select\n");
        if ((ret = select(m_fd + 1, &readfds, 0, 0, msecs >= 0 ? &timeout : 0)) < 0) {
            LOGSYSERR("RclIntf::getEvent", "select", "");
            close();
            return false;
        } else if (ret == 0) {
            MONDEB("RclIntf::getEvent: select timeout\n");
            // timeout
            return false;
        }
        MONDEB("RclIntf::getEvent: select returned\n");

        if (!FD_ISSET(m_fd, &readfds))
            return false;
        int rret;
        if ((rret=read(m_fd, m_evbuf, sizeof(m_evbuf))) <= 0) {
            LOGSYSERR("RclIntf::getEvent", "read", sizeof(m_evbuf));
            close();
            return false;
        }
        m_evp = m_evbuf;
        m_ep = m_evbuf + rret;
    }

    struct inotify_event *evp = (struct inotify_event *)m_evp;
    m_evp += sizeof(struct inotify_event);
    if (evp->len > 0)
        m_evp += evp->len;
    if (m_evp >= m_ep)
        m_evp = m_ep = 0;
    
    map<int,string>::const_iterator it;
    if ((it = m_idtopath.find(evp->wd)) == m_idtopath.end()) {
        LOGERR("RclIntf::getEvent: unknown wd " << evp->wd << "\n");
        return true;
    }
    ev.m_path = it->second;

    if (evp->len > 0) {
        ev.m_path = path_cat(ev.m_path, evp->name);
    }

    MONDEB("RclIntf::getEvent: " << event_name(evp->mask) << " " << ev.m_path << "\n");

    if ((evp->mask & IN_MOVED_FROM) && (evp->mask & IN_ISDIR)) {
        // We get this when a directory is renamed. Erase the subtree
        // entries in the map. The subsequent MOVED_TO will recreate
        // them. This is probably not needed because the watches
        // actually still exist in the kernel, so that the wds
        // returned by future addwatches will be the old ones, and the
        // map will be updated in place. But still, this feels safer
        eraseWatchSubTree(m_idtopath, ev.m_path);
    }

    // IN_ATTRIB used to be not needed, but now it is
    if (evp->mask & (IN_MODIFY|IN_ATTRIB)) {
        ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
    } else if (evp->mask & (IN_DELETE | IN_MOVED_FROM)) {
        ev.m_etyp = RclMonEvent::RCLEVT_DELETE;
        if (evp->mask & IN_ISDIR)
            ev.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
    } else if (evp->mask & (IN_CREATE | IN_MOVED_TO)) {
        if (evp->mask & IN_ISDIR) {
            ev.m_etyp = RclMonEvent::RCLEVT_DIRCREATE;
        } else {
            // We used to return null event because we would get a
            // modify event later, but it seems not to be the case any
            // more (10-2011). So generate MODIFY event
            ev.m_etyp = RclMonEvent::RCLEVT_MODIFY;
        }
    } else if (evp->mask & (IN_IGNORED)) {
        if (!m_idtopath.erase(evp->wd)) {
            LOGDEB0("Got IGNORE event for unknown watch\n");
        } else {
            eraseWatchSubTree(m_idtopath, ev.m_path);
        }
    } else {
        LOGDEB("RclIntf::getEvent: unhandled event " << event_name(evp->mask) <<
               " " << evp->mask << " " << ev.m_path << "\n");
        return true;
    }
    return true;
}

#endif // RCL_USE_INOTIFY



#ifdef _WIN32

/*
 * WIN32 VERSION NOTES: 
 *
 * - When using non-recursive watches (one per dir), it appeared that
 *   watching a subdirectory of a given directory prevented renaming
 *   the top directory, Windows says: can't rename because open or a
 *   file in it is open. This is mostly why we use recursive watches 
 *   on the topdirs only.
 */
#include <string>
#include <vector>
#include <thread>
#include <queue>

#include "safewindows.h"

typedef long WatchID;
class WatcherWin32;
class RclFSWatchWin32;

enum class Action {Add = 1, Delete = 2, Modify = 3, Move = 4};

// Virtual interface for the monitor callback. Note: this for compatibility with the efsw code, as
// rclmon uses a pull, not push interface. The callback pushes the events to a local queue from
// which they are then pulled by the upper level code.
class FileWatchListener {
public:
    virtual ~FileWatchListener() {}
    virtual void handleFileAction(WatchID watchid, const std::string& dir, const std::string& fn,
                                  Action action, bool isdir, std::string oldfn = "" ) = 0;
};

// Internal watch data. This piggy-back our actual data pointer to the MS overlapped pointer. This
// is a bit of a hack, and we could probably use event Ids instead.
struct WatcherStructWin32
{
    OVERLAPPED Overlapped;
    WatcherWin32 *Watch;
};

// Actual data structure for one directory watch
class WatcherWin32 {
public:
    WatchID ID;
    FileWatchListener *Listener{nullptr};
    bool Recursive;
    std::string DirName;
    std::string	OldFileName;

    HANDLE DirHandle{nullptr};
    // do NOT make this bigger than 64K because it will fail if the folder being watched is on the
    // network! (see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365465(v=vs.85).aspx)
    BYTE Buffer[8 * 1024];
    DWORD NotifyFilter{0};
    bool StopNow{false};
    RclFSWatchWin32 *Watch{nullptr};
};

// The efsw top level file system watcher: manages all the directory watches.
class RclFSWatchWin32 {
public:
    RclFSWatchWin32();

    virtual ~RclFSWatchWin32();

    // Add a directory watch
    // On error returns -1
    WatchID addWatch(const std::string& directory, FileWatchListener *watcher, bool recursive);

    // 2nd stage of action processing (after the static handler which just reads the data)
    void handleAction(WatcherWin32 *watch, const std::string& fn, unsigned long action);

    bool ok() const {
        return mInitOK;
    }

    // Fetch events, with msecs timeout if there are no more
    void run(DWORD msecs);

private:
    HANDLE mIOCP;
    // Using a vector because we don't remove watches. Change to list if needed.
    std::vector<WatcherStructWin32*> mWatches;
    bool mInitOK{false};
    WatchID mLastWatchID{0};

    std::mutex mWatchesLock;

    bool pathInWatches(const std::string& path);
    /// Remove all directory watches.
    void removeAllWatches();
};

// Adapter for the rclmon interface
class RclMonitorWin32 : public RclMonitor, public FileWatchListener {
public:
    virtual ~RclMonitorWin32() {}

    virtual bool addWatch(const string& path, bool /*isDir*/) override {
        MONDEB("RclMonitorWin32::addWatch: " << path << "\n");
        return m_fswatcher.addWatch(path, this, true) != -1;
    }

    virtual bool getEvent(RclMonEvent& ev, int msecs = -1) {
        PRETEND_USE(msecs);
        if (!m_events.empty()) {
            ev = m_events.front();
            m_events.pop();
            return true;
        }
        m_fswatcher.run(msecs);
        if (!m_events.empty()) {
            ev = m_events.front();
            m_events.pop();
            return true;
        }
        return false;
    }

    virtual bool ok() const override {
        return m_fswatcher.ok();
    }
    // Does this monitor generate 'exist' events at startup?
    virtual bool generatesExist() const override {
        return false;
    }
    // Can the caller avoid setting watches on subdirs ?
    virtual bool isRecursive() const override {
        return true;
    }
    virtual void handleFileAction(WatchID watchid, const std::string& dir, const std::string& fn,
                                  Action action, bool isdir, std::string oldfn = "") {
        MONDEB("RclMonitorWin32::handleFileAction: dir [" << dir << "] fn [" << fn << "] act " <<
               int(action) << " isdir " << isdir << " oldfn [" << oldfn << "]\n");
        RclMonEvent event;
        switch (action) {
        case Action::Move:
        case Action::Add: event.m_etyp = isdir ?
                RclMonEvent::RCLEVT_DIRCREATE : RclMonEvent::RCLEVT_MODIFY; break;
        case Action::Delete:
            event.m_etyp = RclMonEvent::RCLEVT_DELETE;
            if (isdir) {
                event.m_etyp |= RclMonEvent::RCLEVT_ISDIR;
            }
            break;
        case Action::Modify: event.m_etyp = RclMonEvent::RCLEVT_MODIFY; break;
        }
        event.m_path = path_cat(dir, fn);
        m_events.push(event);
    }

    // Save significant errno after monitor calls
    int saved_errno{0};
private:
    std::queue<RclMonEvent> m_events;
    RclFSWatchWin32 m_fswatcher;
};


/// Stops monitoring a directory.
void DestroyWatch(WatcherStructWin32 *pWatch)
{
    if (pWatch) {
        WatcherWin32 *ww32 = pWatch->Watch;
        ww32->StopNow = true;
        CancelIoEx(ww32->DirHandle, &pWatch->Overlapped);
        CloseHandle(ww32->DirHandle);
        delete ww32;
        // Shouldn't we call heapfree on the parameter here ??
    }
}

/// Refreshes the directory monitoring.
bool RefreshWatch(WatcherStructWin32 *pWatch)
{
    WatcherWin32 *ww32 = pWatch->Watch;
    return ReadDirectoryChangesW(
        ww32->DirHandle,
        ww32->Buffer,
        sizeof(ww32->Buffer),
        ww32->Recursive,
        ww32->NotifyFilter,
        NULL,
        &pWatch->Overlapped,
        NULL
        ) != 0;
}

/// Starts monitoring a directory.
WatcherStructWin32 *CreateWatch(LPCWSTR szDirectory, bool recursive, DWORD NotifyFilter, HANDLE iocp)
{
    WatcherStructWin32 *wsw32;
    size_t ptrsize = sizeof(*wsw32);
    wsw32 =static_cast<WatcherStructWin32*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptrsize));

    WatcherWin32 *ww32 = new WatcherWin32();
    wsw32->Watch = ww32;

    ww32->DirHandle = CreateFileW(
        szDirectory,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
        );

    if (ww32->DirHandle != INVALID_HANDLE_VALUE &&
        CreateIoCompletionPort(ww32->DirHandle, iocp, 0, 1)) {
        ww32->NotifyFilter = NotifyFilter;
        ww32->Recursive = recursive;

        if (RefreshWatch(wsw32)) {
            return wsw32;
        }
    }

    CloseHandle(ww32->DirHandle);
    delete ww32;
    HeapFree(GetProcessHeap(), 0, wsw32);
    return NULL;
}


RclFSWatchWin32::RclFSWatchWin32()
    : mLastWatchID(0)
{
    mIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (mIOCP && mIOCP != INVALID_HANDLE_VALUE)
        mInitOK = true;
}

RclFSWatchWin32::~RclFSWatchWin32()
{
    mInitOK = false;

    if (mIOCP && mIOCP != INVALID_HANDLE_VALUE) {
        PostQueuedCompletionStatus(mIOCP, 0, reinterpret_cast<ULONG_PTR>(this), NULL);
    }

    removeAllWatches();

    CloseHandle(mIOCP);
}

WatchID RclFSWatchWin32::addWatch(const std::string& _dir,FileWatchListener *watcher,bool recursive)
{
    LOGDEB("RclFSWatchWin32::addWatch: " << _dir << " recursive " << recursive << "\n");
    std::string dir(_dir);
    path_slashize(dir);
    if (!path_isdir(dir)) {
        LOGDEB("RclFSWatchWin32::addWatch: not a directory: " << dir << "\n");
        return 0;
    }
    if (!path_readable(dir)) {
        LOGINF("RclFSWatchWin32::addWatch: not readable: " << dir << "\n");
        return 0;
    }
    path_catslash(dir);
    auto wdir = utf8towchar(dir);

    std::unique_lock<std::mutex> lock(mWatchesLock);

    if (pathInWatches(dir)) {
        MONDEB("RclFSWatchWin32::addWatch: already in watches: " << dir << "\n");
        return 0;
    }

    WatchID watchid = ++mLastWatchID;

    WatcherStructWin32 *watch = CreateWatch(
        wdir.get(), recursive,
        FILE_NOTIFY_CHANGE_CREATION |
        FILE_NOTIFY_CHANGE_LAST_WRITE |
        FILE_NOTIFY_CHANGE_FILE_NAME |
        FILE_NOTIFY_CHANGE_DIR_NAME |
        FILE_NOTIFY_CHANGE_SIZE,
        mIOCP
        );

    if (nullptr == watch) {
        LOGINF("RclFSWatchWin32::addWatch: CreateWatch failed\n");
        return -1;
    }

    // Add the handle to the handles vector
    watch->Watch->ID = watchid;
    watch->Watch->Watch = this;
    watch->Watch->Listener = watcher;
    watch->Watch->DirName = dir;

    mWatches.push_back(watch);

    return watchid;
}

void RclFSWatchWin32::removeAllWatches()
{
    std::unique_lock<std::mutex> lock(mWatchesLock);
    for( auto& watchp : mWatches) {
        DestroyWatch(watchp);
    }
    mWatches.clear();
}

// Unpacks events and passes them to the event processor
void CALLBACK WatchCallback(DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
    if (dwNumberOfBytesTransfered == 0 || NULL == lpOverlapped) {
        return;
    }

    WatcherStructWin32 *wsw32 = (WatcherStructWin32*)lpOverlapped;
    WatcherWin32 *ww32 = wsw32->Watch;

    PFILE_NOTIFY_INFORMATION pNotify;
    size_t offset = 0;
    do {
        pNotify = (PFILE_NOTIFY_INFORMATION) &ww32->Buffer[offset];
        offset += pNotify->NextEntryOffset;

        std::string sfn;
        wchartoutf8(pNotify->FileName, sfn, pNotify->FileNameLength / sizeof(WCHAR));
        ww32->Watch->handleAction(ww32, sfn, pNotify->Action);
    } while (pNotify->NextEntryOffset != 0);

    if (!ww32->StopNow) {
        RefreshWatch(wsw32);
    }
}

void RclFSWatchWin32::run(DWORD msecs)
{
    if (!mWatches.empty()) {
        DWORD numOfBytes = 0;
        OVERLAPPED* ov = NULL;
        ULONG_PTR compKey = 0;
        BOOL res = FALSE;
        DWORD ms = msecs == -1 ? INFINITE : msecs;
        while ((res = GetQueuedCompletionStatus(mIOCP, &numOfBytes, &compKey, &ov, ms))) {
            if (compKey != 0 && compKey == reinterpret_cast<ULONG_PTR>(this)) {
                // Called from ~RclFSWatchWin32. Must exit.
                MONDEB("RclFSWatchWin32::run: queuedcompletion said need exit\n");
                return;
            } else {
                std::unique_lock<std::mutex> lock(mWatchesLock);
                WatchCallback(numOfBytes, ov);
            }
        }
    } else {
        // No watches yet.
        MONDEB("RclFSWatchWin32::run: no watches yet\n");
        DWORD ms = msecs == -1 ? 1000 : msecs;
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

void RclFSWatchWin32::handleAction(WatcherWin32 *watch, const std::string& _fn, unsigned long action)
{
    std::string fn(_fn);
    Action fwAction;
    path_slashize(fn);
    MONDEB("handleAction: fn [" << fn << "] action " << action << "\n");

    // In case fn is not a simple name but a relative path (probably
    // possible/common if recursive is set ?), sort out the directory
    // path and simple file name.
    std::string newpath = path_cat(watch->DirName, fn);
    bool isdir = path_isdir(newpath);
    std::string simplefn = path_getsimple(newpath);
    std::string folderPath = path_getfather(newpath);

    switch (action) {
    case FILE_ACTION_RENAMED_OLD_NAME:
        watch->OldFileName = fn;
        /* FALLTHROUGH */
    case FILE_ACTION_REMOVED:
        fwAction = Action::Delete;
        // The system does not tell us if this was a directory, but we
        // need the info. Check if it was in the watches.
        // TBD: for a delete, we should delete all watches on the subtree !
        path_catslash(newpath);
        for (auto& watchp : mWatches) {
            if (watchp->Watch->DirName == newpath) {
                isdir = true;
                break;
            }
        }
        break;
    case FILE_ACTION_ADDED:
        fwAction = Action::Add;
        break;
    case FILE_ACTION_MODIFIED:
        fwAction = Action::Modify;
        break;
    case FILE_ACTION_RENAMED_NEW_NAME: {
        fwAction = Action::Move;

        // If this is a directory, possibly update the watches.  TBD: this seems wrong because we
        // should process the whole subtree ? Also probably not needed at all because we are
        // recursive and only set watches on the top directories.
        if (isdir) {
            // Update the new directory path
            std::string oldpath = path_cat(watch->DirName, watch->OldFileName);
            path_catslash(oldpath);
            for (auto& watchp : mWatches) {
                if (watchp->Watch->DirName == oldpath) {
                    watchp->Watch->DirName = newpath;
                    break;
                }
            }
        }

        std::string oldFolderPath = watch->DirName +
            watch->OldFileName.substr(0, watch->OldFileName.find_last_of("/\\"));

        if (folderPath == oldFolderPath) {
            watch->Listener->handleFileAction(watch->ID, folderPath, simplefn, fwAction, isdir,
                                              path_getsimple(watch->OldFileName));
        } else {
            // Calling the client with non-simple paths??
            watch->Listener->handleFileAction(watch->ID, watch->DirName, fn, fwAction, isdir,
                                              watch->OldFileName);
        }
        return;
    }
    default:
        return;
    };

    watch->Listener->handleFileAction(watch->ID, folderPath, simplefn, fwAction, isdir);
}

bool RclFSWatchWin32::pathInWatches(const std::string& path)
{
    for (const auto& wsw32 : mWatches) {
        if (wsw32->Watch->DirName == path ) {
            return true;
        }
    }
    return false;
}

#endif // _WIN32


///////////////////////////////////////////////////////////////////////
// The monitor 'factory'
static RclMonitor *makeMonitor()
{
#ifdef _WIN32
    return new RclMonitorWin32;
#else
#  ifdef RCL_USE_INOTIFY
    return new RclIntf;
#  elif defined(RCL_USE_FAM)
    return new RclFAM;
#  endif
#endif
    LOGINFO("RclMonitor: neither Inotify nor Fam was compiled as file system "
            "change notification interface\n");
    return 0;
}

#endif // RCL_MONITOR
