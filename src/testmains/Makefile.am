CXXFLAGS ?= @CXXFLAGS@
LIBXAPIAN=@LIBXAPIAN@
XAPIANCXXFLAGS=@XAPIANCXXFLAGS@
XSLT_CFLAGS=@XSLT_CFLAGS@
XSLT_LINKADD=@XSLT_LINKADD@
LIBICONV=@LIBICONV@
INCICONV=@INCICONV@
LIBFAM = @LIBFAM@
RCLLIBVERSION=@RCLLIBVERSION@
X_CFLAGS=@X_CFLAGS@
X_PRE_LIBS=@X_PRE_LIBS@
X_LIBS=@X_LIBS@
X_EXTRA_LIBS=@X_EXTRA_LIBS@
X_LIBX11=@X_LIBX11@
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
    $(XAPIANCXXFLAGS) \
    $(XSLT_CFLAGS) \
    $(X_CFLAGS) \
    -DRECOLL_DATADIR=\"${pkgdatadir}\" \
    -DREADFILE_ENABLE_ZLIB -DREADFILE_ENABLE_MINIZ -DREADFILE_ENABLE_MD5 \
    -D_GNU_SOURCE \
    $(DEFS)

noinst_PROGRAMS = plaintorich textsplit fstreewalk rclconfig hldata unac mbox \
    circache wipedir mimetype fileudi x11mon trqrstore ecrontab rcldb rclutil

ecrontab_SOURCES = trecrontab.cpp
ecrontab_LDADD = ../librecoll.la

circache_SOURCES = trcircache.cpp
circache_LDADD = ../librecoll.la

fileudi_SOURCES = trfileudi.cpp
fileudi_LDADD = ../librecoll.la

fstreewalk_SOURCES = trfstreewalk.cpp
fstreewalk_LDADD = ../librecoll.la

hldata_SOURCES = trhldata.cpp
hldata_LDADD = ../librecoll.la

mbox_SOURCES = trmbox.cpp
mbox_LDADD = ../librecoll.la

mimetype_SOURCES = trmimetype.cpp
mimetype_LDADD = ../librecoll.la

rclconfig_SOURCES = trrclconfig.cpp
rclconfig_LDADD = ../librecoll.la

rcldb_SOURCES = trrcldb.cpp
rcldb_LDADD = ../librecoll.la

rclutil_SOURCES = trrclutil.cpp
rclutil_LDADD = ../librecoll.la

textsplit_SOURCES = trtextsplit.cpp
textsplit_LDADD = ../librecoll.la

plaintorich_SOURCES = trplaintorich.cpp
plaintorich_LDADD = ../librecoll.la

unac_SOURCES = trunac.cpp
unac_LDADD = ../librecoll.la

wipedir_SOURCES = trwipedir.cpp
wipedir_LDADD = ../librecoll.la

x11mon_SOURCES = trx11mon.cpp
x11mon_LDADD = ../utils/x11mon.o ../librecoll.la -lX11

trqrstore_SOURCES = trqrstore.cpp
trqrstore_LDADD = ../librecoll.la
