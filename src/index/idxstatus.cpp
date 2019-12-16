/* Copyright (C) 2017-2018 J.F.Dockes
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


#include "idxstatus.h"

#include "rclconfig.h"
#include "conftree.h"

void readIdxStatus(RclConfig *config, DbIxStatus &status)
{
    ConfSimple cs(config->getIdxStatusFile().c_str(), 1);
    status.phase = DbIxStatus::Phase(cs.getInt("phase", 0));
    cs.get("fn", status.fn);
    status.docsdone = cs.getInt("docsdone", 0);
    status.filesdone = cs.getInt("filesdone", 0);
    status.fileerrors = cs.getInt("fileerrors", 0);
    status.dbtotdocs = cs.getInt("dbtotdocs", 0);
    status.totfiles = cs.getInt("totfiles", 0);
    status.hasmonitor = cs.getBool("hasmonitor", false);
}
