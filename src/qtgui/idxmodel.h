/* Copyright (C) 2022 J.F.Dockes
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

#ifndef _IDXMODEL_H
#define _IDXMODEL_H

#include <QStandardItemModel>

class RclConfig;

class IdxTreeModel : public QStandardItemModel {
    Q_OBJECT;
public:
    IdxTreeModel(RclConfig *config, int depth, QWidget *parent = nullptr)
        : QStandardItemModel(0, 0, (QObject*)parent), m_config(config), m_depth(depth) {
    }
    ~IdxTreeModel() {}
    void populate();
    int getDepth() {return m_depth;}
private:
    RclConfig *m_config{nullptr};
    int m_depth;
};

#endif // _IDXMODEL_H
