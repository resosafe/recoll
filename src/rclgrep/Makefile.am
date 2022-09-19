CXXFLAGS ?= @CXXFLAGS@
XSLT_CFLAGS=@XSLT_CFLAGS@
XSLT_LINKADD=@XSLT_LINKADD@
LIBICONV=@LIBICONV@
INCICONV=@INCICONV@
DEFS=@DEFS@

COMMONCPPFLAGS = -I. \
    -I$(top_srcdir)/aspell \
    -I$(top_srcdir)/bincimapmime \
    -I$(top_srcdir)/common \
    -I$(top_srcdir)/index \
    -I$(top_srcdir)/internfile \
    -I$(top_srcdir)/query \
    -I$(top_srcdir)/rcldb \
    -I$(top_srcdir)/unac \
    -I$(top_srcdir)/utils \
    -I$(top_srcdir)/xaposix \
    -DBUILDING_RECOLL

AM_CPPFLAGS = -Wall -Wno-unused -std=c++11 \
    $(COMMONCPPFLAGS) \
    $(INCICONV) \
    $(XSLT_CFLAGS) \
    -DRECOLL_DATADIR=\"${pkgdatadir}\" \
    -DREADFILE_ENABLE_ZLIB -DREADFILE_ENABLE_MINIZ -DREADFILE_ENABLE_MD5 \
    -D_GNU_SOURCE \
    -DENABLE_LIBMAGIC \
    $(DEFS)

bin_PROGRAMS = rclgrep

rclgrep_SOURCES = \
    rclgrep.cpp \
    ../bincimapmime/convert.cc \
    ../bincimapmime/mime-parsefull.cc \
    ../bincimapmime/mime-parseonlyheader.cc \
    ../bincimapmime/mime-printbody.cc \
    ../bincimapmime/mime.cc \
    ../common/cstr.cpp \
    ../common/rclconfig.cpp \
    ../common/rclinit.cpp \
    ../common/textsplit.cpp \
    ../common/textsplitko.cpp \
    ../common/webstore.cpp \
    ../index/exefetcher.cpp \
    ../index/fetcher.cpp \
    ../index/fsfetcher.cpp \
    ../index/idxdiags.cpp \
    ../index/mimetype.cpp \
    ../index/webqueuefetcher.cpp \
    ../internfile/extrameta.cpp \
    ../internfile/htmlparse.cpp \
    ../internfile/internfile.cpp \
    ../internfile/mh_exec.cpp \
    ../internfile/mh_execm.cpp \
    ../internfile/mh_html.cpp \
    ../internfile/mh_mail.cpp \
    ../internfile/mh_mbox.cpp \
    ../internfile/mh_text.cpp \
    ../internfile/mh_xslt.cpp \
    ../internfile/mimehandler.cpp \
    ../internfile/myhtmlparse.cpp \
    ../internfile/txtdcode.cpp \
    ../internfile/uncomp.cpp \
    ../rcldb/rcldoc.cpp \
    ../unac/unac.cpp \
    ../utils/base64.cpp \
    ../utils/cancelcheck.cpp \
    ../utils/chrono.cpp \
    ../utils/circache.cpp \
    ../utils/closefrom.cpp \
    ../utils/cmdtalk.cpp \
    ../utils/conftree.cpp \
    ../utils/copyfile.cpp \
    ../utils/cpuconf.cpp \
    ../utils/execmd.cpp \
    ../utils/fileudi.cpp \
    ../utils/fstreewalk.cpp \
    ../utils/idfile.cpp \
    ../utils/log.cpp \
    ../utils/md5.cpp \
    ../utils/md5ut.cpp \
    ../utils/mimeparse.cpp \
    ../utils/miniz.cpp \
    ../utils/netcon.cpp \
    ../utils/pathut.cpp \
    ../utils/pxattr.cpp \
    ../utils/rclutil.cpp \
    ../utils/readfile.cpp \
    ../utils/smallut.cpp  \
    ../utils/transcode.cpp \
    ../utils/wipedir.cpp \
    ../utils/zlibut.cpp
    
rclgrep_LDADD = $(XSLT_LIBS) $(LIBICONV) -lmagic $(LIBTHREADS)
