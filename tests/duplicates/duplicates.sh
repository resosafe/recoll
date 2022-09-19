#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
recollq -S url -C -q duplicate_uniqueterm
recollq -S url    -q duplicate_uniqueterm
recollq -S url -C -q '"STARTTLS is supported in both POP and IMAP"'
recollq -S url    -q '"STARTTLS is supported in both POP and IMAP"'
)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
