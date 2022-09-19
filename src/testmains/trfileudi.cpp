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

#include "fileudi.h"

#include <string.h>

#include <iostream>
#include <string>

using namespace std;

int main(int argc, char **argv)
{
    if (argc > 3) {
        std::cerr << "Usage: trfileudi <path> [ipath]\n";
        std::cerr << "Usage: trfileudi #reads stdin for paths\n";
        return 1;
    }
    string udi;
    if (argc == 1) {
        char buffer[2048];
        while (fgets(buffer, 2048, stdin)) {
            std::string path(buffer, strlen(buffer)-1);
            make_udi(path, "", udi);
            std::cout << udi << "\n";
        }
        return 0;
    } else {
        string path = argv[1];
        string ipath;
        if (argc == 3) {
            ipath = argv[2];
        }
        make_udi(path, ipath, udi);
        std::cout << udi << "\n";
        return 0;
    }
}
