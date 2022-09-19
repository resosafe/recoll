#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
  recollq '"maps image file tags to xapian tags"'
  recollq '"PMC = Performance Monitor Control MSR"'
  recollq shellscriptUUnique
  # Python: query.py
  recollq Xesam QueryException
  
) 2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
