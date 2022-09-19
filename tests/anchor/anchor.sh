#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0
(
for q in \
'"^anchortermeaudebut"' \
'"^anchortermeaudebut"o0' \
'"^anchortermeaudebut"o1' \
'"^ anchortermeunpeuplusloin"' \
'"^anchortermeunpeuplusloin"o30' \
'"^  anchortermeunpeuplusloin"o30' \
'"anchortermenullepart"' \
'"^anchortermenullepart"' \
'"anchortermenullepart $"' \
'"anchortermeunpeumoinsloin$"o30' \
'"anchortermeunpeumoinsloin$"' \
'"anchortermealafin$"' \
'"anchortermealafin$"o0' \
'"anchortermealafin$"o1' \
'title:"^anchortitlebegin"' \
'title:"^anchortitlebegin"o0' \
'title:"^anchortitlebegin"o1' \
'title:"^anchortitleend"' \
'title:"anchortitleend$"' \
'title:"anchortitleend$"o0' \
'title:"anchortitleend$"o1' \
; do 
    echo $q
    recollq -S url -q $q
done

) 2> $mystderr | egrep -v '^Recoll query: ' > $mystdout

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
