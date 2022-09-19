#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
    tmpfile=$(
        echo 'mkstemp(template)' |
            m4 -D template="${TMPDIR:-/tmp}/rcltsttmpXXXXXX"
           ) || exit 1
    trap "rm -f -- '$tmpfile'" EXIT
    rm -f "$tmpfile"
    recollq --extract-to "$tmpfile" -q HtmlAttachment_uniqueTerm
    md5sum "$tmpfile" | awk '{print $1}'
)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
