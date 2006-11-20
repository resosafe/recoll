# DONT EDIT BY HAND: created by script mkMake
depth = ..
include $(depth)/mk/sysconf

UNACCFLAGS = -g -I. -I$(depth)/unac $(INCICONV) -DUNAC_VERSION=\"1.0.7\"

LIBS = librcl.a

all: $(LIBS)

OBJS =  rclconfig.o rclinit.o textsplit.o unacpp.o csguess.o indexer.o mimetype.o htmlparse.o internfile.o mh_exec.o mh_html.o mh_mail.o mh_text.o mimehandler.o myhtmlparse.o docseq.o history.o sortseq.o pathhash.o rcldb.o stemdb.o base64.o conftree.o copyfile.o debuglog.o execmd.o fstreewalk.o idfile.o md5.o mimeparse.o pathut.o readfile.o smallut.o transcode.o wipedir.o
DEPS =  rclconfig.dep.stamp rclinit.dep.stamp textsplit.dep.stamp unacpp.dep.stamp csguess.dep.stamp indexer.dep.stamp mimetype.dep.stamp htmlparse.dep.stamp internfile.dep.stamp mh_exec.dep.stamp mh_html.dep.stamp mh_mail.dep.stamp mh_text.dep.stamp mimehandler.dep.stamp myhtmlparse.dep.stamp docseq.dep.stamp history.dep.stamp sortseq.dep.stamp pathhash.dep.stamp rcldb.dep.stamp stemdb.dep.stamp base64.dep.stamp conftree.dep.stamp copyfile.dep.stamp debuglog.dep.stamp execmd.dep.stamp fstreewalk.dep.stamp idfile.dep.stamp md5.dep.stamp mimeparse.dep.stamp pathut.dep.stamp readfile.dep.stamp smallut.dep.stamp transcode.dep.stamp wipedir.dep.stamp

librcl.a : $(DEPS) $(OBJS) unac.o
	ar ru librcl.a $(OBJS) unac.o
	$(RANLIB) librcl.a

unac.o : $(depth)/unac/unac.c $(depth)/unac/unac.h
	$(CC) $(UNACCFLAGS) -c $(depth)/unac/unac.c
rclconfig.o : ../common/rclconfig.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../common/rclconfig.cpp
rclinit.o : ../common/rclinit.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../common/rclinit.cpp
textsplit.o : ../common/textsplit.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../common/textsplit.cpp
unacpp.o : ../common/unacpp.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../common/unacpp.cpp
csguess.o : ../index/csguess.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../index/csguess.cpp
indexer.o : ../index/indexer.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../index/indexer.cpp
mimetype.o : ../index/mimetype.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../index/mimetype.cpp
htmlparse.o : ../internfile/htmlparse.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/htmlparse.cpp
internfile.o : ../internfile/internfile.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/internfile.cpp
mh_exec.o : ../internfile/mh_exec.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_exec.cpp
mh_html.o : ../internfile/mh_html.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_html.cpp
mh_mail.o : ../internfile/mh_mail.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_mail.cpp
mh_text.o : ../internfile/mh_text.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mh_text.cpp
mimehandler.o : ../internfile/mimehandler.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/mimehandler.cpp
myhtmlparse.o : ../internfile/myhtmlparse.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../internfile/myhtmlparse.cpp
docseq.o : ../query/docseq.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../query/docseq.cpp
history.o : ../query/history.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../query/history.cpp
sortseq.o : ../query/sortseq.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../query/sortseq.cpp
pathhash.o : ../rcldb/pathhash.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/pathhash.cpp
rcldb.o : ../rcldb/rcldb.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/rcldb.cpp
stemdb.o : ../rcldb/stemdb.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../rcldb/stemdb.cpp
base64.o : ../utils/base64.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/base64.cpp
conftree.o : ../utils/conftree.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/conftree.cpp
copyfile.o : ../utils/copyfile.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/copyfile.cpp
debuglog.o : ../utils/debuglog.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/debuglog.cpp
execmd.o : ../utils/execmd.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/execmd.cpp
fstreewalk.o : ../utils/fstreewalk.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/fstreewalk.cpp
idfile.o : ../utils/idfile.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/idfile.cpp
md5.o : ../utils/md5.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/md5.cpp
mimeparse.o : ../utils/mimeparse.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/mimeparse.cpp
pathut.o : ../utils/pathut.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/pathut.cpp
readfile.o : ../utils/readfile.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/readfile.cpp
smallut.o : ../utils/smallut.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/smallut.cpp
transcode.o : ../utils/transcode.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/transcode.cpp
wipedir.o : ../utils/wipedir.cpp
	$(CXX) $(ALL_CXXFLAGS) -c ../utils/wipedir.cpp
depend: $(DEPS)
clean:
	rm -f $(OBJS) $(LIBS) $(DEPS) unac.o
	for i in *.dep;do test -f $$i && cp /dev/null $$i;done
distclean: clean
	rm -f *.dep   
rclconfig.dep.stamp : ../common/rclconfig.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../common/rclconfig.cpp > rclconfig.dep
	touch rclconfig.dep.stamp
rclinit.dep.stamp : ../common/rclinit.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../common/rclinit.cpp > rclinit.dep
	touch rclinit.dep.stamp
textsplit.dep.stamp : ../common/textsplit.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../common/textsplit.cpp > textsplit.dep
	touch textsplit.dep.stamp
unacpp.dep.stamp : ../common/unacpp.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../common/unacpp.cpp > unacpp.dep
	touch unacpp.dep.stamp
csguess.dep.stamp : ../index/csguess.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../index/csguess.cpp > csguess.dep
	touch csguess.dep.stamp
indexer.dep.stamp : ../index/indexer.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../index/indexer.cpp > indexer.dep
	touch indexer.dep.stamp
mimetype.dep.stamp : ../index/mimetype.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../index/mimetype.cpp > mimetype.dep
	touch mimetype.dep.stamp
htmlparse.dep.stamp : ../internfile/htmlparse.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/htmlparse.cpp > htmlparse.dep
	touch htmlparse.dep.stamp
internfile.dep.stamp : ../internfile/internfile.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/internfile.cpp > internfile.dep
	touch internfile.dep.stamp
mh_exec.dep.stamp : ../internfile/mh_exec.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_exec.cpp > mh_exec.dep
	touch mh_exec.dep.stamp
mh_html.dep.stamp : ../internfile/mh_html.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_html.cpp > mh_html.dep
	touch mh_html.dep.stamp
mh_mail.dep.stamp : ../internfile/mh_mail.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_mail.cpp > mh_mail.dep
	touch mh_mail.dep.stamp
mh_text.dep.stamp : ../internfile/mh_text.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mh_text.cpp > mh_text.dep
	touch mh_text.dep.stamp
mimehandler.dep.stamp : ../internfile/mimehandler.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/mimehandler.cpp > mimehandler.dep
	touch mimehandler.dep.stamp
myhtmlparse.dep.stamp : ../internfile/myhtmlparse.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../internfile/myhtmlparse.cpp > myhtmlparse.dep
	touch myhtmlparse.dep.stamp
docseq.dep.stamp : ../query/docseq.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../query/docseq.cpp > docseq.dep
	touch docseq.dep.stamp
history.dep.stamp : ../query/history.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../query/history.cpp > history.dep
	touch history.dep.stamp
sortseq.dep.stamp : ../query/sortseq.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../query/sortseq.cpp > sortseq.dep
	touch sortseq.dep.stamp
pathhash.dep.stamp : ../rcldb/pathhash.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/pathhash.cpp > pathhash.dep
	touch pathhash.dep.stamp
rcldb.dep.stamp : ../rcldb/rcldb.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/rcldb.cpp > rcldb.dep
	touch rcldb.dep.stamp
stemdb.dep.stamp : ../rcldb/stemdb.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../rcldb/stemdb.cpp > stemdb.dep
	touch stemdb.dep.stamp
base64.dep.stamp : ../utils/base64.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/base64.cpp > base64.dep
	touch base64.dep.stamp
conftree.dep.stamp : ../utils/conftree.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/conftree.cpp > conftree.dep
	touch conftree.dep.stamp
copyfile.dep.stamp : ../utils/copyfile.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/copyfile.cpp > copyfile.dep
	touch copyfile.dep.stamp
debuglog.dep.stamp : ../utils/debuglog.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/debuglog.cpp > debuglog.dep
	touch debuglog.dep.stamp
execmd.dep.stamp : ../utils/execmd.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/execmd.cpp > execmd.dep
	touch execmd.dep.stamp
fstreewalk.dep.stamp : ../utils/fstreewalk.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/fstreewalk.cpp > fstreewalk.dep
	touch fstreewalk.dep.stamp
idfile.dep.stamp : ../utils/idfile.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/idfile.cpp > idfile.dep
	touch idfile.dep.stamp
md5.dep.stamp : ../utils/md5.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/md5.cpp > md5.dep
	touch md5.dep.stamp
mimeparse.dep.stamp : ../utils/mimeparse.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/mimeparse.cpp > mimeparse.dep
	touch mimeparse.dep.stamp
pathut.dep.stamp : ../utils/pathut.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/pathut.cpp > pathut.dep
	touch pathut.dep.stamp
readfile.dep.stamp : ../utils/readfile.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/readfile.cpp > readfile.dep
	touch readfile.dep.stamp
smallut.dep.stamp : ../utils/smallut.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/smallut.cpp > smallut.dep
	touch smallut.dep.stamp
transcode.dep.stamp : ../utils/transcode.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/transcode.cpp > transcode.dep
	touch transcode.dep.stamp
wipedir.dep.stamp : ../utils/wipedir.cpp
	$(CXX) -M $(ALL_CXXFLAGS) ../utils/wipedir.cpp > wipedir.dep
	touch wipedir.dep.stamp
include rclconfig.dep
include rclinit.dep
include textsplit.dep
include unacpp.dep
include csguess.dep
include indexer.dep
include mimetype.dep
include htmlparse.dep
include internfile.dep
include mh_exec.dep
include mh_html.dep
include mh_mail.dep
include mh_text.dep
include mimehandler.dep
include myhtmlparse.dep
include docseq.dep
include history.dep
include sortseq.dep
include pathhash.dep
include rcldb.dep
include stemdb.dep
include base64.dep
include conftree.dep
include copyfile.dep
include debuglog.dep
include execmd.dep
include fstreewalk.dep
include idfile.dep
include md5.dep
include mimeparse.dep
include pathut.dep
include readfile.dep
include smallut.dep
include transcode.dep
include wipedir.dep
