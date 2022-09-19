#!/usr/bin/python3
# Copyright (C) 2020 J.F.Dockes
#
# License: GPL 2.1
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the
# Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


import hashlib
import base64
import sys

# The following 2 methods replicate the file UDI computation in fileudi.cpp:
# process a file path so that it is usable (short enough) for use as a Xapian
# term. We truncate and replace the truncated part by its md5 hash.
def pathHash(path, maxlen):
    HASHLEN = 22
    if len(path) <= maxlen:
        return path
    digest = hashlib.md5(path[maxlen-HASHLEN:]).digest()
    adigest = base64.b64encode(digest)
    adigest = adigest[:-2]
    return path[:maxlen-HASHLEN] + adigest

def make_udi(path, ipath=b""):
    PATHHASHLEN = 150
    path +=  b"|"
    path += ipath
    return pathHash(path, PATHHASHLEN)



if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: rclfilestatus.py <outfile>", file=sys.stderr)
        sys.exit(1)

    fnout = sys.argv[1]
    fout = open(fnout, "wb")

    for line in sys.stdin.buffer:
        path = line.rstrip(b"\n")
        udi = make_udi(path)
        fout.write(udi + b"\n")

    fout.close()
