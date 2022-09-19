# @(#$Id: shared.sh,v 1.4 2009-01-06 18:47:33 dockes Exp $  (C) 2006 J.F.Dockes
# shared code and variables for all tests

# TMPDIR has to be something which belongs to the user because of pdftk
# issues (see runtests.sh)
export TMPDIR=$HOME/tmp

RECOLL_TESTDATA=${RECOLL_TESTDATA:-/home/dockes/projets/fulltext/testrecoll}

# All source'rs should set topdir as a relative path from their location to
# this directory. Computing RECOLL_CONFDIR this way allows to rerun an
# individual test from its directory.
topdir=${topdir:-.}

export RECOLL_CONFDIR=$topdir/config/

ECHON="/bin/echo -n"

# Call this with the script's $0 as argument
initvariables() {
  tstdata=${RECOLL_TESTDATA}
  toptmp=${TMPDIR:-/tmp}/recolltsttmp
  myname=`basename $1 .sh`
  mystderr=$toptmp/${myname}.err
  mystdout=$toptmp/${myname}.out
  mydiffs=$toptmp/${myname}.diffs
}

fatal () {
  set -f
  echo
  echo $*
  exit 1
}

checkresult() {
  if test -s "$mydiffs" ; then
    fatal '*** ' $myname FAILED
  else
    rm -f $mydiffs

    # for tests with a local index
    rm -rf history idxstatus.txt index.pid missing recoll.conf xapiandb mimeview
    rm -rf aspdict.* 
    exit 0
  fi
}

