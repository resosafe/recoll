#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(

recollq author:ben '"Consideration of the high correlation"'
recollq '"The Circassian Education Foundation"' date:2008-01-20
recollq author:"Johnny Walker" '"Thin Lizzy"'
recollq '"Objekt steht im Akkusativ"'

) 2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
