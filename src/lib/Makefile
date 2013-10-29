# DONT EDIT BY HAND: created by script mkMake
depth = ..
include $(depth)/mk/sysconf

LIBRECOLL=librecoll.so.$(RCLLIBVERSION)
LIBS = librecoll.a
LIBS = librecoll.a $(LIBRECOLL)

all: $(LIBS) 

OBJS =  rclaspell.o beaglequeuecache.o cstr.o rclconfig.o rclinit.o textsplit.o unacpp.o beaglequeue.o bglfetcher.o fetcher.o fsfetcher.o fsindexer.o indexer.o mimetype.o subtreelist.o htmlparse.o internfile.o mh_exec.o mh_execm.o mh_html.o mh_mail.o mh_mbox.o mh_text.o mimehandler.o myhtmlparse.o txtdcode.o uncomp.o docseq.o docseqdb.o docseqhist.o filtseq.o dynconf.o plaintorich.o recollq.o reslistpager.o sortseq.o wasastringtoquery.o wasatorcl.o daterange.o expansiondbs.o rclabstract.o rcldb.o rcldoc.o rcldups.o rclquery.o rclterms.o searchdata.o searchdataxml.o stemdb.o stoplist.o synfamily.o unac.o base64.o circache.o closefrom.o conftree.o copyfile.o cpuconf.o debuglog.o ecrontab.o execmd.o fstreewalk.o idfile.o fileudi.o md5.o mimeparse.o netcon.o pathut.o pxattr.o rclionice.o readfile.o smallut.o strmatcher.o transcode.o wipedir.o x11mon.o mime-parsefull.o mime-parseonlyheader.o mime-printbody.o mime.o convert.o iodevice.o iofactory.o
DEPS =  rclaspell.dep.stamp beaglequeuecache.dep.stamp cstr.dep.stamp rclconfig.dep.stamp rclinit.dep.stamp textsplit.dep.stamp unacpp.dep.stamp beaglequeue.dep.stamp bglfetcher.dep.stamp fetcher.dep.stamp fsfetcher.dep.stamp fsindexer.dep.stamp indexer.dep.stamp mimetype.dep.stamp subtreelist.dep.stamp htmlparse.dep.stamp internfile.dep.stamp mh_exec.dep.stamp mh_execm.dep.stamp mh_html.dep.stamp mh_mail.dep.stamp mh_mbox.dep.stamp mh_text.dep.stamp mimehandler.dep.stamp myhtmlparse.dep.stamp txtdcode.dep.stamp uncomp.dep.stamp docseq.dep.stamp docseqdb.dep.stamp docseqhist.dep.stamp filtseq.dep.stamp dynconf.dep.stamp plaintorich.dep.stamp recollq.dep.stamp reslistpager.dep.stamp sortseq.dep.stamp wasastringtoquery.dep.stamp wasatorcl.dep.stamp daterange.dep.stamp expansiondbs.dep.stamp rclabstract.dep.stamp rcldb.dep.stamp rcldoc.dep.stamp rcldups.dep.stamp rclquery.dep.stamp rclterms.dep.stamp searchdata.dep.stamp searchdataxml.dep.stamp stemdb.dep.stamp stoplist.dep.stamp synfamily.dep.stamp unac.dep.stamp base64.dep.stamp circache.dep.stamp closefrom.dep.stamp conftree.dep.stamp copyfile.dep.stamp cpuconf.dep.stamp debuglog.dep.stamp ecrontab.dep.stamp execmd.dep.stamp fstreewalk.dep.stamp idfile.dep.stamp fileudi.dep.stamp md5.dep.stamp mimeparse.dep.stamp netcon.dep.stamp pathut.dep.stamp pxattr.dep.stamp rclionice.dep.stamp readfile.dep.stamp smallut.dep.stamp strmatcher.dep.stamp transcode.dep.stamp wipedir.dep.stamp x11mon.dep.stamp mime-parsefull.dep.stamp mime-parseonlyheader.dep.stamp mime-printbody.dep.stamp mime.dep.stamp convert.dep.stamp iodevice.dep.stamp iofactory.dep.stamp

librecoll.a : $(DEPS) $(OBJS)
	ar ru librecoll.a $(OBJS)
	$(RANLIB) librecoll.a

# Note that we are using the Recoll release number in the soname. There is
# no effort whatsoever to maintain any kind of ABI compat in this lib. Only
# programs or modules from the same Recoll release can be compatible.
$(LIBRECOLL): $(DEPS) $(OBJS)
	$(CXX) -shared -Wl,--no-undefined -Wl,--warn-unresolved-symbols           -Wl,-soname=$(LIBRECOLL) -o $(LIBRECOLL) $(OBJS)            $(LIBXAPIAN) $(LIBICONV) $(X_LIBX11) $(LIBSYS)
	rm -f librecoll.so
	ln -s $(LIBRECOLL) librecoll.so

rclaspell.o : ../aspell/rclaspell.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../aspell/rclaspell.cpp
beaglequeuecache.o : ../common/beaglequeuecache.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/beaglequeuecache.cpp
cstr.o : ../common/cstr.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/cstr.cpp
rclconfig.o : ../common/rclconfig.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/rclconfig.cpp
rclinit.o : ../common/rclinit.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/rclinit.cpp
textsplit.o : ../common/textsplit.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/textsplit.cpp
unacpp.o : ../common/unacpp.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/unacpp.cpp
beaglequeue.o : ../index/beaglequeue.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/beaglequeue.cpp
bglfetcher.o : ../index/bglfetcher.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/bglfetcher.cpp
fetcher.o : ../index/fetcher.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/fetcher.cpp
fsfetcher.o : ../index/fsfetcher.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/fsfetcher.cpp
fsindexer.o : ../index/fsindexer.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/fsindexer.cpp
indexer.o : ../index/indexer.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/indexer.cpp
mimetype.o : ../index/mimetype.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/mimetype.cpp
subtreelist.o : ../index/subtreelist.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/subtreelist.cpp
htmlparse.o : ../internfile/htmlparse.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/htmlparse.cpp
internfile.o : ../internfile/internfile.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/internfile.cpp
mh_exec.o : ../internfile/mh_exec.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_exec.cpp
mh_execm.o : ../internfile/mh_execm.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_execm.cpp
mh_html.o : ../internfile/mh_html.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_html.cpp
mh_mail.o : ../internfile/mh_mail.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_mail.cpp
mh_mbox.o : ../internfile/mh_mbox.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_mbox.cpp
mh_text.o : ../internfile/mh_text.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_text.cpp
mimehandler.o : ../internfile/mimehandler.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mimehandler.cpp
myhtmlparse.o : ../internfile/myhtmlparse.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/myhtmlparse.cpp
txtdcode.o : ../internfile/txtdcode.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/txtdcode.cpp
uncomp.o : ../internfile/uncomp.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/uncomp.cpp
docseq.o : ../query/docseq.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/docseq.cpp
docseqdb.o : ../query/docseqdb.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/docseqdb.cpp
docseqhist.o : ../query/docseqhist.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/docseqhist.cpp
filtseq.o : ../query/filtseq.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/filtseq.cpp
dynconf.o : ../query/dynconf.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/dynconf.cpp
plaintorich.o : ../query/plaintorich.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/plaintorich.cpp
recollq.o : ../query/recollq.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/recollq.cpp
reslistpager.o : ../query/reslistpager.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/reslistpager.cpp
sortseq.o : ../query/sortseq.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/sortseq.cpp
wasastringtoquery.o : ../query/wasastringtoquery.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/wasastringtoquery.cpp
wasatorcl.o : ../query/wasatorcl.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../query/wasatorcl.cpp
daterange.o : ../rcldb/daterange.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/daterange.cpp
expansiondbs.o : ../rcldb/expansiondbs.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/expansiondbs.cpp
rclabstract.o : ../rcldb/rclabstract.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rclabstract.cpp
rcldb.o : ../rcldb/rcldb.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldb.cpp
rcldoc.o : ../rcldb/rcldoc.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldoc.cpp
rcldups.o : ../rcldb/rcldups.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldups.cpp
rclquery.o : ../rcldb/rclquery.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rclquery.cpp
rclterms.o : ../rcldb/rclterms.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rclterms.cpp
searchdata.o : ../rcldb/searchdata.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/searchdata.cpp
searchdataxml.o : ../rcldb/searchdataxml.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/searchdataxml.cpp
stemdb.o : ../rcldb/stemdb.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/stemdb.cpp
stoplist.o : ../rcldb/stoplist.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/stoplist.cpp
synfamily.o : ../rcldb/synfamily.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/synfamily.cpp
unac.o : ../unac/unac.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../unac/unac.cpp
base64.o : ../utils/base64.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/base64.cpp
circache.o : ../utils/circache.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/circache.cpp
closefrom.o : ../utils/closefrom.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/closefrom.cpp
conftree.o : ../utils/conftree.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/conftree.cpp
copyfile.o : ../utils/copyfile.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/copyfile.cpp
cpuconf.o : ../utils/cpuconf.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/cpuconf.cpp
debuglog.o : ../utils/debuglog.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/debuglog.cpp
ecrontab.o : ../utils/ecrontab.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/ecrontab.cpp
execmd.o : ../utils/execmd.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/execmd.cpp
fstreewalk.o : ../utils/fstreewalk.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/fstreewalk.cpp
idfile.o : ../utils/idfile.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/idfile.cpp
fileudi.o : ../utils/fileudi.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/fileudi.cpp
md5.o : ../utils/md5.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/md5.cpp
mimeparse.o : ../utils/mimeparse.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/mimeparse.cpp
netcon.o : ../utils/netcon.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/netcon.cpp
pathut.o : ../utils/pathut.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/pathut.cpp
pxattr.o : ../utils/pxattr.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/pxattr.cpp
rclionice.o : ../utils/rclionice.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/rclionice.cpp
readfile.o : ../utils/readfile.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/readfile.cpp
smallut.o : ../utils/smallut.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/smallut.cpp
strmatcher.o : ../utils/strmatcher.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/strmatcher.cpp
transcode.o : ../utils/transcode.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/transcode.cpp
wipedir.o : ../utils/wipedir.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/wipedir.cpp
x11mon.o : ../utils/x11mon.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/x11mon.cpp
mime-parsefull.o : ../bincimapmime/mime-parsefull.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-parsefull.cc
mime-parseonlyheader.o : ../bincimapmime/mime-parseonlyheader.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-parseonlyheader.cc
mime-printbody.o : ../bincimapmime/mime-printbody.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-printbody.cc
mime.o : ../bincimapmime/mime.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime.cc
convert.o : ../bincimapmime/convert.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/convert.cc
iodevice.o : ../bincimapmime/iodevice.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/iodevice.cc
iofactory.o : ../bincimapmime/iofactory.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/iofactory.cc
depend: $(DEPS)
clean:
	rm -f $(OBJS) $(LIBS) $(DEPS) librecoll.so *.stamp
	for i in *.dep;do test -f $$i && cp /dev/null $$i;done
distclean: clean
	rm -f *.dep   
rclaspell.dep.stamp : ../aspell/rclaspell.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../aspell/rclaspell.cpp > rclaspell.dep
	touch rclaspell.dep.stamp
beaglequeuecache.dep.stamp : ../common/beaglequeuecache.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/beaglequeuecache.cpp > beaglequeuecache.dep
	touch beaglequeuecache.dep.stamp
cstr.dep.stamp : ../common/cstr.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/cstr.cpp > cstr.dep
	touch cstr.dep.stamp
rclconfig.dep.stamp : ../common/rclconfig.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/rclconfig.cpp > rclconfig.dep
	touch rclconfig.dep.stamp
rclinit.dep.stamp : ../common/rclinit.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/rclinit.cpp > rclinit.dep
	touch rclinit.dep.stamp
textsplit.dep.stamp : ../common/textsplit.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/textsplit.cpp > textsplit.dep
	touch textsplit.dep.stamp
unacpp.dep.stamp : ../common/unacpp.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/unacpp.cpp > unacpp.dep
	touch unacpp.dep.stamp
beaglequeue.dep.stamp : ../index/beaglequeue.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/beaglequeue.cpp > beaglequeue.dep
	touch beaglequeue.dep.stamp
bglfetcher.dep.stamp : ../index/bglfetcher.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/bglfetcher.cpp > bglfetcher.dep
	touch bglfetcher.dep.stamp
fetcher.dep.stamp : ../index/fetcher.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/fetcher.cpp > fetcher.dep
	touch fetcher.dep.stamp
fsfetcher.dep.stamp : ../index/fsfetcher.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/fsfetcher.cpp > fsfetcher.dep
	touch fsfetcher.dep.stamp
fsindexer.dep.stamp : ../index/fsindexer.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/fsindexer.cpp > fsindexer.dep
	touch fsindexer.dep.stamp
indexer.dep.stamp : ../index/indexer.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/indexer.cpp > indexer.dep
	touch indexer.dep.stamp
mimetype.dep.stamp : ../index/mimetype.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/mimetype.cpp > mimetype.dep
	touch mimetype.dep.stamp
subtreelist.dep.stamp : ../index/subtreelist.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/subtreelist.cpp > subtreelist.dep
	touch subtreelist.dep.stamp
htmlparse.dep.stamp : ../internfile/htmlparse.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/htmlparse.cpp > htmlparse.dep
	touch htmlparse.dep.stamp
internfile.dep.stamp : ../internfile/internfile.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/internfile.cpp > internfile.dep
	touch internfile.dep.stamp
mh_exec.dep.stamp : ../internfile/mh_exec.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_exec.cpp > mh_exec.dep
	touch mh_exec.dep.stamp
mh_execm.dep.stamp : ../internfile/mh_execm.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_execm.cpp > mh_execm.dep
	touch mh_execm.dep.stamp
mh_html.dep.stamp : ../internfile/mh_html.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_html.cpp > mh_html.dep
	touch mh_html.dep.stamp
mh_mail.dep.stamp : ../internfile/mh_mail.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_mail.cpp > mh_mail.dep
	touch mh_mail.dep.stamp
mh_mbox.dep.stamp : ../internfile/mh_mbox.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_mbox.cpp > mh_mbox.dep
	touch mh_mbox.dep.stamp
mh_text.dep.stamp : ../internfile/mh_text.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_text.cpp > mh_text.dep
	touch mh_text.dep.stamp
mimehandler.dep.stamp : ../internfile/mimehandler.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mimehandler.cpp > mimehandler.dep
	touch mimehandler.dep.stamp
myhtmlparse.dep.stamp : ../internfile/myhtmlparse.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/myhtmlparse.cpp > myhtmlparse.dep
	touch myhtmlparse.dep.stamp
txtdcode.dep.stamp : ../internfile/txtdcode.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/txtdcode.cpp > txtdcode.dep
	touch txtdcode.dep.stamp
uncomp.dep.stamp : ../internfile/uncomp.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/uncomp.cpp > uncomp.dep
	touch uncomp.dep.stamp
docseq.dep.stamp : ../query/docseq.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/docseq.cpp > docseq.dep
	touch docseq.dep.stamp
docseqdb.dep.stamp : ../query/docseqdb.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/docseqdb.cpp > docseqdb.dep
	touch docseqdb.dep.stamp
docseqhist.dep.stamp : ../query/docseqhist.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/docseqhist.cpp > docseqhist.dep
	touch docseqhist.dep.stamp
filtseq.dep.stamp : ../query/filtseq.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/filtseq.cpp > filtseq.dep
	touch filtseq.dep.stamp
dynconf.dep.stamp : ../query/dynconf.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/dynconf.cpp > dynconf.dep
	touch dynconf.dep.stamp
plaintorich.dep.stamp : ../query/plaintorich.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/plaintorich.cpp > plaintorich.dep
	touch plaintorich.dep.stamp
recollq.dep.stamp : ../query/recollq.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/recollq.cpp > recollq.dep
	touch recollq.dep.stamp
reslistpager.dep.stamp : ../query/reslistpager.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/reslistpager.cpp > reslistpager.dep
	touch reslistpager.dep.stamp
sortseq.dep.stamp : ../query/sortseq.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/sortseq.cpp > sortseq.dep
	touch sortseq.dep.stamp
wasastringtoquery.dep.stamp : ../query/wasastringtoquery.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/wasastringtoquery.cpp > wasastringtoquery.dep
	touch wasastringtoquery.dep.stamp
wasatorcl.dep.stamp : ../query/wasatorcl.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../query/wasatorcl.cpp > wasatorcl.dep
	touch wasatorcl.dep.stamp
daterange.dep.stamp : ../rcldb/daterange.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/daterange.cpp > daterange.dep
	touch daterange.dep.stamp
expansiondbs.dep.stamp : ../rcldb/expansiondbs.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/expansiondbs.cpp > expansiondbs.dep
	touch expansiondbs.dep.stamp
rclabstract.dep.stamp : ../rcldb/rclabstract.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rclabstract.cpp > rclabstract.dep
	touch rclabstract.dep.stamp
rcldb.dep.stamp : ../rcldb/rcldb.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldb.cpp > rcldb.dep
	touch rcldb.dep.stamp
rcldoc.dep.stamp : ../rcldb/rcldoc.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldoc.cpp > rcldoc.dep
	touch rcldoc.dep.stamp
rcldups.dep.stamp : ../rcldb/rcldups.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldups.cpp > rcldups.dep
	touch rcldups.dep.stamp
rclquery.dep.stamp : ../rcldb/rclquery.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rclquery.cpp > rclquery.dep
	touch rclquery.dep.stamp
rclterms.dep.stamp : ../rcldb/rclterms.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rclterms.cpp > rclterms.dep
	touch rclterms.dep.stamp
searchdata.dep.stamp : ../rcldb/searchdata.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/searchdata.cpp > searchdata.dep
	touch searchdata.dep.stamp
searchdataxml.dep.stamp : ../rcldb/searchdataxml.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/searchdataxml.cpp > searchdataxml.dep
	touch searchdataxml.dep.stamp
stemdb.dep.stamp : ../rcldb/stemdb.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/stemdb.cpp > stemdb.dep
	touch stemdb.dep.stamp
stoplist.dep.stamp : ../rcldb/stoplist.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/stoplist.cpp > stoplist.dep
	touch stoplist.dep.stamp
synfamily.dep.stamp : ../rcldb/synfamily.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/synfamily.cpp > synfamily.dep
	touch synfamily.dep.stamp
unac.dep.stamp : ../unac/unac.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../unac/unac.cpp > unac.dep
	touch unac.dep.stamp
base64.dep.stamp : ../utils/base64.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/base64.cpp > base64.dep
	touch base64.dep.stamp
circache.dep.stamp : ../utils/circache.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/circache.cpp > circache.dep
	touch circache.dep.stamp
closefrom.dep.stamp : ../utils/closefrom.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/closefrom.cpp > closefrom.dep
	touch closefrom.dep.stamp
conftree.dep.stamp : ../utils/conftree.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/conftree.cpp > conftree.dep
	touch conftree.dep.stamp
copyfile.dep.stamp : ../utils/copyfile.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/copyfile.cpp > copyfile.dep
	touch copyfile.dep.stamp
cpuconf.dep.stamp : ../utils/cpuconf.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/cpuconf.cpp > cpuconf.dep
	touch cpuconf.dep.stamp
debuglog.dep.stamp : ../utils/debuglog.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/debuglog.cpp > debuglog.dep
	touch debuglog.dep.stamp
ecrontab.dep.stamp : ../utils/ecrontab.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/ecrontab.cpp > ecrontab.dep
	touch ecrontab.dep.stamp
execmd.dep.stamp : ../utils/execmd.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/execmd.cpp > execmd.dep
	touch execmd.dep.stamp
fstreewalk.dep.stamp : ../utils/fstreewalk.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/fstreewalk.cpp > fstreewalk.dep
	touch fstreewalk.dep.stamp
idfile.dep.stamp : ../utils/idfile.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/idfile.cpp > idfile.dep
	touch idfile.dep.stamp
fileudi.dep.stamp : ../utils/fileudi.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/fileudi.cpp > fileudi.dep
	touch fileudi.dep.stamp
md5.dep.stamp : ../utils/md5.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/md5.cpp > md5.dep
	touch md5.dep.stamp
mimeparse.dep.stamp : ../utils/mimeparse.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/mimeparse.cpp > mimeparse.dep
	touch mimeparse.dep.stamp
netcon.dep.stamp : ../utils/netcon.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/netcon.cpp > netcon.dep
	touch netcon.dep.stamp
pathut.dep.stamp : ../utils/pathut.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/pathut.cpp > pathut.dep
	touch pathut.dep.stamp
pxattr.dep.stamp : ../utils/pxattr.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/pxattr.cpp > pxattr.dep
	touch pxattr.dep.stamp
rclionice.dep.stamp : ../utils/rclionice.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/rclionice.cpp > rclionice.dep
	touch rclionice.dep.stamp
readfile.dep.stamp : ../utils/readfile.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/readfile.cpp > readfile.dep
	touch readfile.dep.stamp
smallut.dep.stamp : ../utils/smallut.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/smallut.cpp > smallut.dep
	touch smallut.dep.stamp
strmatcher.dep.stamp : ../utils/strmatcher.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/strmatcher.cpp > strmatcher.dep
	touch strmatcher.dep.stamp
transcode.dep.stamp : ../utils/transcode.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/transcode.cpp > transcode.dep
	touch transcode.dep.stamp
wipedir.dep.stamp : ../utils/wipedir.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/wipedir.cpp > wipedir.dep
	touch wipedir.dep.stamp
x11mon.dep.stamp : ../utils/x11mon.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/x11mon.cpp > x11mon.dep
	touch x11mon.dep.stamp
include rclaspell.dep
include beaglequeuecache.dep
include cstr.dep
include rclconfig.dep
include rclinit.dep
include textsplit.dep
include unacpp.dep
include beaglequeue.dep
include bglfetcher.dep
include fetcher.dep
include fsfetcher.dep
include fsindexer.dep
include indexer.dep
include mimetype.dep
include subtreelist.dep
include htmlparse.dep
include internfile.dep
include mh_exec.dep
include mh_execm.dep
include mh_html.dep
include mh_mail.dep
include mh_mbox.dep
include mh_text.dep
include mimehandler.dep
include myhtmlparse.dep
include txtdcode.dep
include uncomp.dep
include docseq.dep
include docseqdb.dep
include docseqhist.dep
include filtseq.dep
include dynconf.dep
include plaintorich.dep
include recollq.dep
include reslistpager.dep
include sortseq.dep
include wasastringtoquery.dep
include wasatorcl.dep
include daterange.dep
include expansiondbs.dep
include rclabstract.dep
include rcldb.dep
include rcldoc.dep
include rcldups.dep
include rclquery.dep
include rclterms.dep
include searchdata.dep
include searchdataxml.dep
include stemdb.dep
include stoplist.dep
include synfamily.dep
include unac.dep
include base64.dep
include circache.dep
include closefrom.dep
include conftree.dep
include copyfile.dep
include cpuconf.dep
include debuglog.dep
include ecrontab.dep
include execmd.dep
include fstreewalk.dep
include idfile.dep
include fileudi.dep
include md5.dep
include mimeparse.dep
include netcon.dep
include pathut.dep
include pxattr.dep
include rclionice.dep
include readfile.dep
include smallut.dep
include strmatcher.dep
include transcode.dep
include wipedir.dep
include x11mon.dep
mime-parsefull.dep.stamp : ../bincimapmime/mime-parsefull.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-parsefull.cc > mime-parsefull.dep
	touch mime-parsefull.dep.stamp
mime-parseonlyheader.dep.stamp : ../bincimapmime/mime-parseonlyheader.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-parseonlyheader.cc > mime-parseonlyheader.dep
	touch mime-parseonlyheader.dep.stamp
mime-printbody.dep.stamp : ../bincimapmime/mime-printbody.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-printbody.cc > mime-printbody.dep
	touch mime-printbody.dep.stamp
mime.dep.stamp : ../bincimapmime/mime.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime.cc > mime.dep
	touch mime.dep.stamp
convert.dep.stamp : ../bincimapmime/convert.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/convert.cc > convert.dep
	touch convert.dep.stamp
iodevice.dep.stamp : ../bincimapmime/iodevice.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/iodevice.cc > iodevice.dep
	touch iodevice.dep.stamp
iofactory.dep.stamp : ../bincimapmime/iofactory.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/iofactory.cc > iofactory.dep
	touch iofactory.dep.stamp
include mime-parsefull.dep
include mime-parseonlyheader.dep
include mime-printbody.dep
include mime.dep
include convert.dep
include iodevice.dep
include iofactory.dep
