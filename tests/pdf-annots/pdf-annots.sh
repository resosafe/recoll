#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
    recollq '"new test JF annotation using Adobe Acrobat X"'

    # This supposes that the fields file is customized, which is not
    # the case by default
    echo 
    echo "Extracting the value for an annotation field:"
    recollq -F annotation pdfannot:'"DAVID: Test of a highlight"'  | \
        tail -1 | base64 -d

)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
