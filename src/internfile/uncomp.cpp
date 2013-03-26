/* Copyright (C) 2013 J.F.Dockes
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

#include "autoconfig.h"

#include <string>
#include <vector>
#include <map>
using std::map;
using std::string;
using std::vector;

#include "uncomp.h"
#include "debuglog.h"
#include "smallut.h"
#include "execmd.h"

Uncomp::UncompCache Uncomp::o_cache;

bool Uncomp::uncompressfile(const string& ifn, 
			    const vector<string>& cmdv, string& tfile)
{
    if (m_docache) {
	PTMutexLocker lock(o_cache.m_lock);
	if (!o_cache.m_srcpath.compare(ifn)) {
	    m_dir = o_cache.m_dir;
	    m_tfile = tfile = o_cache.m_tfile;
	    m_srcpath = ifn;
	    o_cache.m_dir = 0;
	    o_cache.m_srcpath.clear();
	    return true;
	}
    }

    m_srcpath.clear();
    m_tfile.clear();
    if (m_dir == 0) {
	m_dir = new TempDir;
    }
    // Make sure tmp dir is empty. we guarantee this to filters
    if (!m_dir || !m_dir->ok() || !m_dir->wipe()) {
	LOGERR(("uncompressfile: can't clear temp dir %s\n", m_dir->dirname()));
	return false;
    }
    string cmd = cmdv.front();

    // Substitute file name and temp dir in command elements
    vector<string>::const_iterator it = cmdv.begin();
    ++it;
    vector<string> args;
    map<char, string> subs;
    subs['f'] = ifn;
    subs['t'] = m_dir->dirname();
    for (; it != cmdv.end(); it++) {
	string ns;
	pcSubst(*it, ns, subs);
	args.push_back(ns);
    }

    // Execute command and retrieve output file name, check that it exists
    ExecCmd ex;
    int status = ex.doexec(cmd, args, 0, &tfile);
    if (status || tfile.empty()) {
	LOGERR(("uncompressfile: doexec: failed for [%s] status 0x%x\n", 
		ifn.c_str(), status));
	if (!m_dir->wipe()) {
	    LOGERR(("uncompressfile: wipedir failed\n"));
	}
	return false;
    }
    if (tfile[tfile.length() - 1] == '\n')
	tfile.erase(tfile.length() - 1, 1);
    m_tfile = tfile;
    m_srcpath = ifn;
    return true;
}

Uncomp::~Uncomp()
{
    if (m_docache) {
	PTMutexLocker lock(o_cache.m_lock);
	delete o_cache.m_dir;
	o_cache.m_dir = m_dir;
	o_cache.m_tfile = m_tfile;
	o_cache.m_srcpath = m_srcpath;
    } else {
	delete m_dir;
    }
}
