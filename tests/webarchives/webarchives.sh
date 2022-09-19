#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

(
  recollq -S url '"Le témoignage émouvant de Loubna Abidar"'
  recollq -S url '"ils ont declare que ma mere etait communiste"'
  recollq -S url '"We borrow a lot of code from other packages"' dir:webarchives
)  2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
