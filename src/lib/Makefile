# DONT EDIT BY HAND: created by script mkMake
depth = ..
include $(depth)/mk/sysconf

LIBS = librcl.a

all: $(LIBS)

OBJS =  rclaspell.o beaglequeuecache.o rclconfig.o rclinit.o textsplit.o unacpp.o beaglequeue.o csguess.o fsindexer.o indexer.o mimetype.o htmlparse.o myhtmlparse.o mimehandler.o internfile.o mh_exec.o mh_execm.o mh_html.o mh_mail.o mh_mbox.o mh_text.o docseq.o docseqdb.o docseqhist.o filtseq.o dynconf.o plaintorich.o recollq.o reslistpager.o sortseq.o wasastringtoquery.o wasatorcl.o rcldb.o rcldoc.o rclquery.o searchdata.o stemdb.o stoplist.o base64.o circache.o closefrom.o conftree.o copyfile.o debuglog.o execmd.o fstreewalk.o idfile.o fileudi.o md5.o mimeparse.o netcon.o pathut.o pxattr.o rclionice.o readfile.o smallut.o transcode.o wipedir.o x11mon.o mime-getpart.o mime-parsefull.o mime-parseonlyheader.o mime-printbody.o mime-printdoc.o mime-printheader.o mime.o convert.o iodevice.o iofactory.o
DEPS =  rclaspell.dep.stamp beaglequeuecache.dep.stamp rclconfig.dep.stamp rclinit.dep.stamp textsplit.dep.stamp unacpp.dep.stamp beaglequeue.dep.stamp csguess.dep.stamp fsindexer.dep.stamp indexer.dep.stamp mimetype.dep.stamp htmlparse.dep.stamp myhtmlparse.dep.stamp mimehandler.dep.stamp internfile.dep.stamp mh_exec.dep.stamp mh_execm.dep.stamp mh_html.dep.stamp mh_mail.dep.stamp mh_mbox.dep.stamp mh_text.dep.stamp docseq.dep.stamp docseqdb.dep.stamp docseqhist.dep.stamp filtseq.dep.stamp dynconf.dep.stamp plaintorich.dep.stamp recollq.dep.stamp reslistpager.dep.stamp sortseq.dep.stamp wasastringtoquery.dep.stamp wasatorcl.dep.stamp rcldb.dep.stamp rcldoc.dep.stamp rclquery.dep.stamp searchdata.dep.stamp stemdb.dep.stamp stoplist.dep.stamp base64.dep.stamp circache.dep.stamp closefrom.dep.stamp conftree.dep.stamp copyfile.dep.stamp debuglog.dep.stamp execmd.dep.stamp fstreewalk.dep.stamp idfile.dep.stamp fileudi.dep.stamp md5.dep.stamp mimeparse.dep.stamp netcon.dep.stamp pathut.dep.stamp pxattr.dep.stamp rclionice.dep.stamp readfile.dep.stamp smallut.dep.stamp transcode.dep.stamp wipedir.dep.stamp x11mon.dep.stamp mime-getpart.dep.stamp mime-parsefull.dep.stamp mime-parseonlyheader.dep.stamp mime-printbody.dep.stamp mime-printdoc.dep.stamp mime-printheader.dep.stamp mime.dep.stamp convert.dep.stamp iodevice.dep.stamp iofactory.dep.stamp

librcl.a : $(DEPS) $(OBJS) unac.o
	ar ru librcl.a $(OBJS) unac.o
	$(RANLIB) librcl.a

unac.o : $(depth)/unac/unac.c $(depth)/unac/unac.h $(depth)/mk/localdefs
	$(CC) $(ALL_CXXFLAGS) -c $(depth)/unac/unac.c
rclaspell.o : ../aspell/rclaspell.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../aspell/rclaspell.cpp
beaglequeuecache.o : ../common/beaglequeuecache.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../common/beaglequeuecache.cpp
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
csguess.o : ../index/csguess.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/csguess.cpp
fsindexer.o : ../index/fsindexer.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/fsindexer.cpp
indexer.o : ../index/indexer.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/indexer.cpp
mimetype.o : ../index/mimetype.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../index/mimetype.cpp
htmlparse.o : ../internfile/htmlparse.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/htmlparse.cpp
myhtmlparse.o : ../internfile/myhtmlparse.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/myhtmlparse.cpp
mimehandler.o : ../internfile/mimehandler.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mimehandler.cpp
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
rcldb.o : ../rcldb/rcldb.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldb.cpp
rcldoc.o : ../rcldb/rcldoc.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldoc.cpp
rclquery.o : ../rcldb/rclquery.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rclquery.cpp
searchdata.o : ../rcldb/searchdata.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/searchdata.cpp
stemdb.o : ../rcldb/stemdb.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/stemdb.cpp
stoplist.o : ../rcldb/stoplist.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/stoplist.cpp
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
debuglog.o : ../utils/debuglog.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/debuglog.cpp
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
transcode.o : ../utils/transcode.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/transcode.cpp
wipedir.o : ../utils/wipedir.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/wipedir.cpp
x11mon.o : ../utils/x11mon.cpp $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/x11mon.cpp
mime-getpart.o : ../bincimapmime/mime-getpart.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-getpart.cc
mime-parsefull.o : ../bincimapmime/mime-parsefull.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-parsefull.cc
mime-parseonlyheader.o : ../bincimapmime/mime-parseonlyheader.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-parseonlyheader.cc
mime-printbody.o : ../bincimapmime/mime-printbody.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-printbody.cc
mime-printdoc.o : ../bincimapmime/mime-printdoc.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-printdoc.cc
mime-printheader.o : ../bincimapmime/mime-printheader.cc $(depth)/mk/localdefs
	$(CXX) $(ALL_CXXFLAGS) -c ../bincimapmime/mime-printheader.cc
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
	rm -f $(OBJS) $(LIBS) $(DEPS) *.stamp unac.o
	for i in *.dep;do test -f $$i && cp /dev/null $$i;done
distclean: clean
	rm -f *.dep   
rclaspell.dep.stamp : ../aspell/rclaspell.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../aspell/rclaspell.cpp > rclaspell.dep
	touch rclaspell.dep.stamp
beaglequeuecache.dep.stamp : ../common/beaglequeuecache.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../common/beaglequeuecache.cpp > beaglequeuecache.dep
	touch beaglequeuecache.dep.stamp
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
csguess.dep.stamp : ../index/csguess.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/csguess.cpp > csguess.dep
	touch csguess.dep.stamp
fsindexer.dep.stamp : ../index/fsindexer.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/fsindexer.cpp > fsindexer.dep
	touch fsindexer.dep.stamp
indexer.dep.stamp : ../index/indexer.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/indexer.cpp > indexer.dep
	touch indexer.dep.stamp
mimetype.dep.stamp : ../index/mimetype.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../index/mimetype.cpp > mimetype.dep
	touch mimetype.dep.stamp
htmlparse.dep.stamp : ../internfile/htmlparse.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/htmlparse.cpp > htmlparse.dep
	touch htmlparse.dep.stamp
myhtmlparse.dep.stamp : ../internfile/myhtmlparse.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/myhtmlparse.cpp > myhtmlparse.dep
	touch myhtmlparse.dep.stamp
mimehandler.dep.stamp : ../internfile/mimehandler.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mimehandler.cpp > mimehandler.dep
	touch mimehandler.dep.stamp
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
rcldb.dep.stamp : ../rcldb/rcldb.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldb.cpp > rcldb.dep
	touch rcldb.dep.stamp
rcldoc.dep.stamp : ../rcldb/rcldoc.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldoc.cpp > rcldoc.dep
	touch rcldoc.dep.stamp
rclquery.dep.stamp : ../rcldb/rclquery.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rclquery.cpp > rclquery.dep
	touch rclquery.dep.stamp
searchdata.dep.stamp : ../rcldb/searchdata.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/searchdata.cpp > searchdata.dep
	touch searchdata.dep.stamp
stemdb.dep.stamp : ../rcldb/stemdb.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/stemdb.cpp > stemdb.dep
	touch stemdb.dep.stamp
stoplist.dep.stamp : ../rcldb/stoplist.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/stoplist.cpp > stoplist.dep
	touch stoplist.dep.stamp
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
debuglog.dep.stamp : ../utils/debuglog.cpp $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/debuglog.cpp > debuglog.dep
	touch debuglog.dep.stamp
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
include rclconfig.dep
include rclinit.dep
include textsplit.dep
include unacpp.dep
include beaglequeue.dep
include csguess.dep
include fsindexer.dep
include indexer.dep
include mimetype.dep
include htmlparse.dep
include myhtmlparse.dep
include mimehandler.dep
include internfile.dep
include mh_exec.dep
include mh_execm.dep
include mh_html.dep
include mh_mail.dep
include mh_mbox.dep
include mh_text.dep
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
include rcldb.dep
include rcldoc.dep
include rclquery.dep
include searchdata.dep
include stemdb.dep
include stoplist.dep
include base64.dep
include circache.dep
include closefrom.dep
include conftree.dep
include copyfile.dep
include debuglog.dep
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
include transcode.dep
include wipedir.dep
include x11mon.dep
mime-getpart.dep.stamp : ../bincimapmime/mime-getpart.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-getpart.cc > mime-getpart.dep
	touch mime-getpart.dep.stamp
mime-parsefull.dep.stamp : ../bincimapmime/mime-parsefull.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-parsefull.cc > mime-parsefull.dep
	touch mime-parsefull.dep.stamp
mime-parseonlyheader.dep.stamp : ../bincimapmime/mime-parseonlyheader.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-parseonlyheader.cc > mime-parseonlyheader.dep
	touch mime-parseonlyheader.dep.stamp
mime-printbody.dep.stamp : ../bincimapmime/mime-printbody.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-printbody.cc > mime-printbody.dep
	touch mime-printbody.dep.stamp
mime-printdoc.dep.stamp : ../bincimapmime/mime-printdoc.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-printdoc.cc > mime-printdoc.dep
	touch mime-printdoc.dep.stamp
mime-printheader.dep.stamp : ../bincimapmime/mime-printheader.cc $(depth)/mk/localdefs
	$(CXX) -M $(ALL_CXXFLAGS) ../bincimapmime/mime-printheader.cc > mime-printheader.dep
	touch mime-printheader.dep.stamp
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
include mime-getpart.dep
include mime-parsefull.dep
include mime-parseonlyheader.dep
include mime-printbody.dep
include mime-printdoc.dep
include mime-printheader.dep
include mime.dep
include convert.dep
include iodevice.dep
include iofactory.dep
