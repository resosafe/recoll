#!/bin/sh

topdir=`dirname $0`/..
. $topdir/shared.sh

initvariables $0

# See how many Maildir files were indexed. All Maildir messages have had 
# MAILDIR_UNIQUEXXX added at the end of the 'Subject: ' line. Note that
# because of the attachments the relationship of file count to result count
# is not trivial. We do not use recollq here because any change in the
# indexing process is going to change the order of results.

xadump -d $RECOLL_TESTCACHEDIR/xapiandb -t maildir_uniquexxx -F | \
       grep FreqFor > $mystdout 2> $mystderr

diff -w ${myname}.txt $mystdout > $mydiffs 2>&1

checkresult
