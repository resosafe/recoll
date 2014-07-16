# @(#$Id: shared.sh,v 1.4 2009-01-06 18:47:33 dockes Exp $  (C) 2006 J.F.Dockes
# shared code and variables for all tests

RECOLL_TESTDATA=/home/dockes/projets/fulltext/testrecoll

# All source'rs should set topdir as a relative path from their location to
# this directory
topdir=${topdir:-.}

RECOLL_CONFDIR=$topdir/config/
export RECOLL_CONFDIR

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

rerootResults()
{
    savedcd=`pwd`
    dirs=`ls -F | grep / | grep -v CVS | grep -v non-auto | grep -v config`
    for dir in $dirs ; do
    	cd $dir
	resfile=`basename $dir`.txt
	sed -i.bak \
	  -e "s!file:///.*/testrecoll/!file://$RECOLL_TESTDATA/!g" \
	  $resfile
    	cd ..
    done

    cd $RECOLL_CONFDIR
    sed -i.bak \
  -e "s!/.*/testrecoll/!$RECOLL_TESTDATA/!g" \
      recoll.conf
    sed -i.bak \
  -e "s!/.*/testrecoll/!$RECOLL_TESTDATA/!g" \
      mimemap

    cd $savedcd
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

