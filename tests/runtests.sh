#!/bin/sh

export TMPDIR=$HOME/tmp

fatal()
{
    echo $*;exit 1
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
      mimemap

    cd $savedcd
}

iscmd()
{
    cmd=$1
    case $cmd in
    */*)
	if test -x $cmd -a ! -d $cmd ; then return 0; else return 1; fi ;;
    *)
      oldifs=$IFS; IFS=":"; set -- $PATH; IFS=$oldifs
      for d in $*;do test -x $d/$cmd -a ! -d $d/$cmd && \
          iscmdresult=$d/$cmd && return 0;done
      return 1 ;;
    esac
}

checkcmds()
{
    result=0
    for cmd in $*;do
      if iscmd $cmd 
      then 
        echo $cmd is $iscmdresult
      else 
        echo $cmd not found
        result=1
      fi
    done
    return $result
}

makeindex() {
  echo "Zeroing Index" 
  rm -rf $RECOLL_CONFDIR/xapiandb $RECOLL_CONFDIR/aspdict.*.rws
  rm -rf $RECOLL_CONFDIR/ocrcache
  echo "Indexing" 
  recollindex -c $RECOLL_CONFDIR -z
}

if test ! -f shared.sh ; then
    fatal must be run in the top test directory
fi
checkcmds recollq recollindex pxattr xadump pdftk || exit 1

iscmd pdftk
pdftk=$iscmdresult
tmpdir=${RECOLL_TMPDIR:-$TMPDIR}
case "$pdftk" in
    /snap/*)
        if test X$tmpdir = X -o "$tmpdir" = /tmp;then
            fatal pdftk as snap need '$TMPDIR' to belong to you
        fi
    ;;
esac

if test ! x$reroot = x ; then
    rerootResults
fi

# Temp directory for test results
# Make sure this is computed in the same way as in shared.sh
toptmp=${TMPDIR:-/tmp}/recolltsttmp

test X"$toptmp" = X && fatal "empty toptmp??"
test X"$toptmp" = X/ && fatal "toptmp == / ??"
if test -d "$toptmp" ; then
   rm -rf $toptmp/*
fi
mkdir -p $toptmp || fatal cant create temp dir $toptmp

# Unset DISPLAY because xdg-mime may be affected by the desktop
# environment on the X server
unset DISPLAY
export LC_ALL=en_US.UTF-8

RECOLL_TESTS=`pwd`
RECOLL_TESTDATA=${RECOLL_TESTDATA:-/home/dockes/projets/fulltext/testrecoll}
export RECOLL_CONFDIR=$RECOLL_TESTS/config/
# Some test need to access RECOLL_TESTCACHEDIR
export RECOLL_TESTCACHEDIR=$toptmp

sed -e "s,@RECOLL_TESTS@,$RECOLL_TESTS,g" \
    -e "s,@RECOLL_TESTDATA@,$RECOLL_TESTDATA,g" \
    -e "s,@RECOLL_TESTCACHEDIR@,$RECOLL_TESTCACHEDIR,g" \
    < $RECOLL_CONFDIR/recoll.conf.in \
    > $RECOLL_CONFDIR/recoll.conf || exit 1

if test x$noindex = x ; then
  makeindex
fi


dirs=`ls -F | grep / | grep -v CVS | grep -v non-auto | grep -v config`

echo
echo "Running query tests:"

for dir in $dirs ; do
    cd $dir && echo -n "$dir "
    sh `basename $dir`.sh
    cd ..
done

echo
