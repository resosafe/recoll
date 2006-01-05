# @(#$Id: Makefile,v 1.21.2.2 2006-01-05 10:05:41 dockes Exp $  (C) 2005 J.F.Dockes

all: mk/sysconf common/rclversion.h
	cd lib; ${MAKE}
	cd bincimapmime; ${MAKE}
	cd index; ${MAKE} recollindex
	cd qtgui; qmake recoll.pro; \
	   cat ../mk/sysconf Makefile | \
	       ${MAKE} -f -

mk/sysconf:
	./configure

common/rclversion.h: VERSION
	(echo -n "static const char *rclversion= \"" && \
	 echo -n `cat VERSION` && \
	 echo "\";") > common/rclversion.h

static:
	cd lib; ${MAKE}
	cd bincimapmime; ${MAKE}
	cd index; rm -f recollindex; \
	   ${MAKE} BSTATIC=-Wl,-Bstatic BDYNAMIC=-Wl,-Bdynamic recollindex
	cd qtgui; qmake recoll.pro; rm -f recoll; \
	   cat ../mk/sysconf ../mk/localdefs Makefile | ${MAKE} -f - \
	       BSTATIC=-Wl,-Bstatic BDYNAMIC=-Wl,-Bdynamic

clean:
	cd common; ${MAKE} clean
	cd index; ${MAKE} clean
	cd lib; ${MAKE} clean
	cd bincimapmime; ${MAKE} clean
	cd query; ${MAKE} clean
	cd utils; ${MAKE} clean
	@-cd qtgui; ${MAKE} clean
	rm -f qtgui/Makefile qtgui/preview/Makefile qtgui/recoll

distclean: clean
	rm -f mk/sysconf mk/localdefs sampleconf/recoll.conf recollinstall

install: all
	bindir=${bindir} datadir=${datadir} ./recollinstall ${prefix}

.PHONY: all static clean distclean install
